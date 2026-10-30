import sys
import time

sys.path.insert(0, r"C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution

session = remote_execution.RemoteExecution()
session.start()
try:
    deadline = time.time() + 10.0
    node = None
    while time.time() < deadline:
        for candidate in session.remote_nodes:
            if candidate.get("project_name") == "LangToLua":
                node = candidate
                break
        if node:
            break
        time.sleep(0.25)
    if not node:
        raise RuntimeError("LangToLua Unreal Editor node was not found")
    session.open_command_connection(node["node_id"])
    result = session.run_command(sys.argv[1], exec_mode=remote_execution.MODE_EXEC_FILE, raise_on_failure=True)
    print(result)
finally:
    session.stop()
