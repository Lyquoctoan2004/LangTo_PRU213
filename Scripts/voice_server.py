"""Local speech server for the lobby talk area (UCharacterTalkVoice, TextToSpeechEndpoint).

POST /synthesize with JSON {"text": "...", "voice": "<piper voice>", "speaker_id": <n>} returns a 16-bit PCM WAV.
The line is read with Microsoft Edge's Vietnamese neural voice (vi-VN-NamMinhNeural, a man) through the unofficial
edge-tts library, retried a few times when the service refuses. Only that one voice is used: a sentence it cannot read
returns an error (the game still shows its text). --piper-fallback reads such sentences with the local Piper voice
instead, and --piper-only uses Piper for everything.

Run with Scripts/StartVoiceServer.bat (uses the venv in %LOCALAPPDATA%/LangToLuaVoice).
"""

import argparse
import asyncio
import io
import json
import logging
import os
import time
import wave
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import edge_tts
import miniaudio
from piper import PiperVoice, SynthesisConfig

LOG = logging.getLogger("voice_server")


def to_wav(pcm: bytes, rate: int) -> bytes:
    buffer = io.BytesIO()
    with wave.open(buffer, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes(pcm)
    return buffer.getvalue()


async def edge_mp3(text: str, voice: str, rate: str, pitch: str) -> bytes:
    audio = bytearray()
    async for chunk in edge_tts.Communicate(text, voice, rate=rate, pitch=pitch).stream():
        if chunk["type"] == "audio":
            audio.extend(chunk["data"])
    return bytes(audio)


class Speech:
    def __init__(self, args):
        self.args = args
        self.piper_voices = {}

    def edge(self, text: str) -> bytes:
        mp3 = asyncio.run(asyncio.wait_for(
            edge_mp3(text, self.args.edge_voice, self.args.edge_rate, self.args.edge_pitch), self.args.edge_timeout))
        if not mp3:
            raise RuntimeError("edge-tts returned no audio")
        decoded = miniaudio.decode(mp3, output_format=miniaudio.SampleFormat.SIGNED16, nchannels=1, sample_rate=24000)
        return to_wav(decoded.samples.tobytes(), decoded.sample_rate)

    def piper(self, text: str, voice_name: str, speaker_id) -> bytes:
        voice_name = voice_name or self.args.piper_voice
        voice = self.piper_voices.get(voice_name)
        if voice is None:
            voice = PiperVoice.load(os.path.join(self.args.piper_dir, voice_name + ".onnx"))
            self.piper_voices[voice_name] = voice
        if speaker_id is not None and voice.config.num_speakers <= 1:
            speaker_id = None
        buffer = io.BytesIO()
        with wave.open(buffer, "wb") as wav:
            voice.synthesize_wav(text, wav, syn_config=SynthesisConfig(speaker_id=speaker_id))
        return buffer.getvalue()

    def synthesize(self, request: dict) -> bytes:
        text = str(request.get("text", "")).strip()
        if not text:
            raise ValueError("no text")
        if not self.args.piper_only:
            # The Edge service now and then refuses a request; asking again usually works. Falling back to Piper would put a
            # second, different voice into the same reply, so that only happens when --piper-fallback is given.
            for attempt in range(1, self.args.edge_attempts + 1):
                try:
                    return self.edge(text)
                except Exception as error:  # noqa: BLE001
                    LOG.warning("Edge voice failed (attempt %d/%d): %s", attempt, self.args.edge_attempts, error)
                    time.sleep(0.2 * attempt)
            if not self.args.piper_fallback:
                raise RuntimeError("Edge voice unavailable")
            LOG.warning("Reading with Piper instead.")
        speaker_id = request.get("speaker_id")
        return self.piper(text, request.get("voice"), int(speaker_id) if speaker_id is not None and int(speaker_id) >= 0 else None)


def make_handler(speech: Speech):
    class Handler(BaseHTTPRequestHandler):
        def do_POST(self):
            if self.path.rstrip("/") != "/synthesize":
                self.send_error(404)
                return
            try:
                request = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
                wav = speech.synthesize(request)
            except Exception as error:  # noqa: BLE001
                LOG.error("Synthesis failed: %s", error)
                self.send_error(500, str(error))
                return
            self.send_response(200)
            self.send_header("Content-Type", "audio/wav")
            self.send_header("Content-Length", str(len(wav)))
            self.end_headers()
            self.wfile.write(wav)

        def log_message(self, fmt, *args):
            LOG.info("%s", fmt % args)

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5000)
    parser.add_argument("--edge-voice", default="vi-VN-NamMinhNeural")
    parser.add_argument("--edge-rate", default="+0%", help="speaking speed, e.g. +10%%")
    parser.add_argument("--edge-pitch", default="+0Hz", help="pitch, e.g. -5Hz")
    parser.add_argument("--edge-timeout", type=float, default=8.0)
    parser.add_argument("--piper-dir", required=True, help="folder with the Piper .onnx voices")
    parser.add_argument("--piper-voice", default="vi_VN-vivos-x_low")
    parser.add_argument("--piper-only", action="store_true", help="skip the Edge voice")
    parser.add_argument("--edge-attempts", type=int, default=3, help="tries per sentence before giving up on the Edge voice")
    parser.add_argument("--piper-fallback", action="store_true",
                        help="read with Piper when the Edge voice fails (mixes two voices in one reply)")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    server = ThreadingHTTPServer((args.host, args.port), make_handler(Speech(args)))
    LOG.info("Voice server on http://%s:%d/synthesize (%s, Piper fallback %s)", args.host, args.port,
             "Piper only" if args.piper_only else args.edge_voice, args.piper_voice)
    server.serve_forever()


if __name__ == "__main__":
    main()
