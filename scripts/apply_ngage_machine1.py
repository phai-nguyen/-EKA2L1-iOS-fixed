#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: apply_ngage_machine1.py <eka2l1-source>")

controller = Path(__file__).resolve().parents[1]
root = Path(sys.argv[1]).resolve()
ios = root / "src/emu/ios"
app = ios / "App"
bridge = ios / "Bridge"
cmake_path = ios / "CMakeLists.txt"

required_targets = [
    cmake_path,
    app / "ContentView.swift",
    bridge / "IosEmulator.h",
    bridge / "IosEmulator.mm",
]
for path in required_targets:
    if not path.exists():
        raise SystemExit(f"required upstream anchor file missing: {path}")

source_map = [
    (controller / "machine1/include/rh29_machine_model.h", bridge / "RH29MachineModel.h"),
    (controller / "machine1/src/rh29_machine_model.cpp", bridge / "RH29MachineModel.cpp"),
    (controller / "machine1/include/rh29_machine_runner.h", bridge / "RH29MachineRunner.h"),
    (controller / "machine1/src/rh29_machine_runner.cpp", bridge / "RH29MachineRunner.cpp"),
    (controller / "machine1/ios/RH29MachineProbeView.swift", app / "RH29MachineProbeView.swift"),
]
for source, _ in source_map:
    if not source.exists():
        raise SystemExit(f"controller MACHINE1 source missing: {source}")

bridge.mkdir(parents=True, exist_ok=True)
app.mkdir(parents=True, exist_ok=True)
for source, destination in source_map:
    text = source.read_text(encoding="utf-8")
    if destination.name == "RH29MachineModel.cpp":
        text = text.replace('#include "rh29_machine_model.h"', '#include "RH29MachineModel.h"', 1)
    elif destination.name == "RH29MachineRunner.h":
        text = text.replace('#include "rh29_machine_model.h"', '#include "RH29MachineModel.h"', 1)
    elif destination.name == "RH29MachineRunner.cpp":
        text = text.replace('#include "rh29_machine_runner.h"', '#include "RH29MachineRunner.h"', 1)
    destination.write_text(text, encoding="utf-8")

cmake = cmake_path.read_text(encoding="utf-8")
app_anchor = '    "${EKA2L1_IOS_APP_DIR}/ContentView.swift"\n'
bridge_anchor = '    "${EKA2L1_IOS_BRIDGE_DIR}/IosEmulator.h"\n'
if app_anchor not in cmake:
    raise SystemExit("CMake App source anchor not found")
if bridge_anchor not in cmake:
    raise SystemExit("CMake Bridge source anchor not found")

app_line = '    "${EKA2L1_IOS_APP_DIR}/RH29MachineProbeView.swift"\n'
if app_line not in cmake:
    cmake = cmake.replace(app_anchor, app_anchor + app_line, 1)

bridge_block = (
    '    "${EKA2L1_IOS_BRIDGE_DIR}/RH29MachineModel.h"\n'
    '    "${EKA2L1_IOS_BRIDGE_DIR}/RH29MachineModel.cpp"\n'
    '    "${EKA2L1_IOS_BRIDGE_DIR}/RH29MachineRunner.h"\n'
    '    "${EKA2L1_IOS_BRIDGE_DIR}/RH29MachineRunner.cpp"\n'
)
if 'RH29MachineModel.h' not in cmake:
    cmake = cmake.replace(bridge_anchor, bridge_block + bridge_anchor, 1)

cmake_path.write_text(cmake, encoding="utf-8")
print("MACHINE1-A sources staged")
