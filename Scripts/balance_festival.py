# Monte Carlo of the festival's Master's Challenge: a player's press lands at a Gaussian error (reaction jitter in seconds x marker speed)
# around the band centre; per round 100 / 70 / 30 for perfect / good / miss, as in UVillageStorySubsystem::PressCraft.
# Run with the project's Python (-I). Edit CONFIG to try other bands, FailBelow values or pass score, then copy the winners
# into Scripts/gen_chapter6_story.py (crafts) and the constants at the top of the Chapter 4-6 block of VillageStorySubsystem.cpp.
import random
import statistics
import sys

random.seed(7)
N = 20000


def craft(rounds, jitter, bonus):
    """rounds: list of (good, perfect, speed). Returns average score 0..100."""
    total = 0
    for good, perfect, speed in rounds:
        g = min(good + bonus, 0.3)
        p = min(perfect + bonus * 0.35, 0.12)
        err = abs(random.gauss(0, jitter * speed))
        total += 100 if err <= p else 70 if err <= g else 30
    return total / len(rounds)


def build(cfg):
    return {
        "dye": [(cfg["dye"][0], cfg["dye"][1], cfg["dye"][2] + 0.05 * i) for i in range(3)],
        "weave": [(cfg["weave"][0], cfg["weave"][1], cfg["weave"][2] + 0.04 * i) for i in range(8)],
        "tailor": [(cfg["tailor"][0], cfg["tailor"][1], cfg["tailor"][2] + 0.05 * i) for i in range(5)],
        "stage": [(cfg["stage"][0], cfg["stage"][1], cfg["stage"][2] + 0.05 * i) for i in range(5)],
    }


def run(cfg, profile):
    rounds = build(cfg)
    jitter, bonus, culture, rel, design = profile
    passes = 0
    scores = []
    phase_fail = {"dye": 0, "weave": 0, "tailor": 0}
    for _ in range(N):
        d = craft(rounds["dye"], jitter, bonus)
        if d < cfg["fail"][0]:
            phase_fail["dye"] += 1
            continue
        w = craft(rounds["weave"], jitter, bonus)
        if w < cfg["fail"][1]:
            phase_fail["weave"] += 1
            continue
        t = craft(rounds["tailor"], jitter, bonus)
        if t < cfg["fail"][2]:
            phase_fail["tailor"] += 1
            continue
        s = craft(rounds["stage"], jitter, bonus)
        craftq = (d + w + t) / 3
        total = craftq * 0.30 + design * 0.25 + min(100, 100 * culture / cfg["culture_full"]) * 0.20 \
            + min(100, 100 * rel / cfg["rel_full"]) * 0.10 + s * 0.15
        scores.append(total)
        passes += total >= cfg["pass"]
    reach = len(scores) / N
    return reach, (statistics.mean(scores) if scores else 0), passes / N, phase_fail


PROFILES = {
    "casual, no prep": (0.16, 0.000, 38, 15, 75),
    "casual, some prep": (0.14, 0.015, 63, 30, 85),
    "prepared (focus craft)": (0.11, 0.035, 55, 28, 92),
    "prepared (focus lore)": (0.11, 0.012, 100, 45, 92),
    "expert, full prep": (0.08, 0.055, 90, 45, 100),
}
VARIANTS = {
 "A pass66": dict(dye=(0.11, 0.04, 0.95), weave=(0.10, 0.035, 1.05), tailor=(0.11, 0.04, 1.0), stage=(0.13, 0.045, 0.9), fail=(45, 45, 45), culture_full=70, rel_full=45, **{"pass": 66}),
 "B pass64": dict(dye=(0.11, 0.04, 0.95), weave=(0.10, 0.035, 1.05), tailor=(0.11, 0.04, 1.0), stage=(0.13, 0.045, 0.9), fail=(45, 45, 45), culture_full=70, rel_full=45, **{"pass": 64}),
 "C pass70": dict(dye=(0.115, 0.045, 0.9), weave=(0.105, 0.04, 1.0), tailor=(0.115, 0.045, 0.95), stage=(0.14, 0.05, 0.85), fail=(45, 45, 45), culture_full=70, rel_full=45, **{"pass": 70}),
}
for name, cfg in VARIANTS.items():
    print("==", name)
    for label, prof in PROFILES.items():
        reach, mean, passed, pf = run(cfg, prof)
        print("  %-24s reach %.0f%%  mean %.0f  pass(1st try) %.0f%%  phase fails dye/weave/tailor %.0f/%.0f/%.0f%%" % (
            label, reach * 100, mean, passed * 100, pf["dye"] / N * 100, pf["weave"] / N * 100, pf["tailor"] / N * 100))
