import pathlib
import sys
import time

sys.path.insert(0, r"C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution

session = remote_execution.RemoteExecution()
session.start()
try:
    for _ in range(30):
        nodes = [n for n in session.remote_nodes if n.get("project_name") == "MutableSample"]
        if nodes:
            break
        time.sleep(0.2)
    if not nodes:
        raise RuntimeError("MutableSample Unreal Editor was not found")
    session.open_command_connection(nodes[0]["node_id"])
    result = session.run_command(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"), exec_mode=remote_execution.MODE_EXEC_FILE)
    print('SUCCESS', result.get('success'), 'RESULT', result.get('result'))
    for line in result.get('output', []):
        print(line.get('output', '').rstrip())
finally:
    session.stop()
