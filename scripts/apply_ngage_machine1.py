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
swift_bridge_path = app / "EKA2L1Bridge.swift"
content_path = app / "ContentView.swift"
ios_header_path = bridge / "IosEmulator.h"
ios_impl_path = bridge / "IosEmulator.mm"
dyncom_header_path = root / "src/emu/cpu/include/cpu/dyncom/arm_dyncom.h"
dyncom_impl_path = root / "src/emu/cpu/src/dyncom/arm_dyncom.cpp"

required_targets = [
    cmake_path,
    content_path,
    swift_bridge_path,
    ios_header_path,
    ios_impl_path,
    dyncom_header_path,
    dyncom_impl_path,
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


def insert_before_interface_end(text: str, interface_marker: str, insertion: str, id_marker: str) -> str:
    if id_marker in text:
        return text
    begin = text.find(interface_marker)
    if begin < 0:
        raise SystemExit(f"Objective-C interface anchor not found: {interface_marker}")
    end = text.find("\n@end", begin)
    if end < 0:
        raise SystemExit(f"Objective-C interface end not found: {interface_marker}")
    return text[:end] + insertion + text[end:]


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
if "RH29MachineModel.h" not in cmake:
    cmake = cmake.replace(bridge_anchor, bridge_block + bridge_anchor, 1)
cmake_path.write_text(cmake, encoding="utf-8")

# Add a probe-only initial privilege mode without changing the constructor used
# by normal HLE execution.
dyn_h = dyncom_header_path.read_text(encoding="utf-8")
old_decl = "        explicit dyncom_core(arm::exclusive_monitor *monitor, const std::size_t page_bits);\n"
new_decl = old_decl + "        dyncom_core(arm::exclusive_monitor *monitor, const std::size_t page_bits, PrivilegeMode initial_mode);\n"
if "PrivilegeMode initial_mode" not in dyn_h:
    if old_decl not in dyn_h:
        raise SystemExit("Dyncom constructor declaration anchor not found")
    dyn_h = dyn_h.replace(old_decl, new_decl, 1)
dyncom_header_path.write_text(dyn_h, encoding="utf-8")

dyn_cpp = dyncom_impl_path.read_text(encoding="utf-8")
if "dyncom_core(monitor, page_bits, USER32MODE)" not in dyn_cpp:
    old_prefix = (
        "    dyncom_core::dyncom_core(arm::exclusive_monitor *monitor, const std::size_t page_bits)\n"
        "        : monitor_(monitor)"
    )
    new_prefix = (
        "    dyncom_core::dyncom_core(arm::exclusive_monitor *monitor, const std::size_t page_bits)\n"
        "        : dyncom_core(monitor, page_bits, USER32MODE) {\n"
        "    }\n\n"
        "    dyncom_core::dyncom_core(arm::exclusive_monitor *monitor, const std::size_t page_bits, const PrivilegeMode initial_mode)\n"
        "        : monitor_(monitor)"
    )
    if old_prefix not in dyn_cpp:
        raise SystemExit("Dyncom constructor implementation anchor not found")
    dyn_cpp = dyn_cpp.replace(old_prefix, new_prefix, 1)
    old_state = "state_ = std::make_unique<ARMul_State>(this, USER32MODE);"
    if old_state not in dyn_cpp:
        raise SystemExit("Dyncom ARMul_State mode anchor not found")
    dyn_cpp = dyn_cpp.replace(old_state, "state_ = std::make_unique<ARMul_State>(this, initial_mode);", 1)
dyncom_impl_path.write_text(dyn_cpp, encoding="utf-8")

hdr = ios_header_path.read_text(encoding="utf-8")
report_type = """
@interface EKA2L1MachineProbeReport : NSObject
@property(nonatomic, assign) BOOL succeeded;
@property(nonatomic, copy) NSString *text;
@end
"""
if "@interface EKA2L1MachineProbeReport" not in hdr:
    ngage_block = """@interface EKA2L1NGageInstallReport : NSObject
@property(nonatomic, assign) NSInteger result;
@property(nonatomic, copy) NSString *gameName;
@end
"""
    if ngage_block not in hdr:
        raise SystemExit("EKA2L1NGageInstallReport header anchor not found")
    hdr = hdr.replace(ngage_block, ngage_block + report_type, 1)
probe_decl = """

// Research-only RH-29 full-machine probe. Runs a separate Dyncom core over the
// installed ROM; it does not replace or reset the active HLE emulator session.
- (EKA2L1MachineProbeReport *)runRH29MachineProbeWithInstructionBudget:(uint32_t)budget
    NS_SWIFT_NAME(runRH29MachineProbe(instructionBudget:));
"""
hdr = insert_before_interface_end(
    hdr, "@interface EKA2L1Emulator : NSObject", probe_decl,
    "runRH29MachineProbeWithInstructionBudget")
ios_header_path.write_text(hdr, encoding="utf-8")

impl = ios_impl_path.read_text(encoding="utf-8")
if '#include "RH29MachineRunner.h"' not in impl:
    include_anchor = '#import "IosEmulator.h"\n'
    if include_anchor not in impl:
        raise SystemExit("IosEmulator include anchor not found")
    impl = impl.replace(include_anchor, include_anchor + '#include "RH29MachineRunner.h"\n', 1)
if "@implementation EKA2L1MachineProbeReport" not in impl:
    ngage_impl = "@implementation EKA2L1NGageInstallReport\n@end\n"
    if ngage_impl not in impl:
        raise SystemExit("NGage report implementation anchor not found")
    impl = impl.replace(
        ngage_impl,
        ngage_impl + "\n@implementation EKA2L1MachineProbeReport\n@end\n",
        1)

probe_method = r'''
- (EKA2L1MachineProbeReport *)runRH29MachineProbeWithInstructionBudget:(uint32_t)budget {
    EKA2L1MachineProbeReport *report = [[EKA2L1MachineProbeReport alloc] init];
    report.succeeded = NO;
    report.text = @"RH29_MACHINE1_E\nSTOP_REASON=io_error\nDETAIL=emulator is not ready\n";

    std::string storage;
    std::string firmware;
    {
        std::lock_guard<std::recursive_mutex> session_lock(_sessionMutex);
        if (!_state || !_state->mounted || !_state->symsys) {
            return report;
        }
        auto *dvc = _state->symsys->get_device_manager();
        if (!dvc) {
            return report;
        }
        std::lock_guard<std::mutex> dvc_lock(dvc->lock);
        const auto *current = dvc->get_current();
        if (!current) {
            return report;
        }
        firmware = current->firmware_code;
        storage = _state->conf.storage;
    }

    NSString *firmwareCode = [NSString stringWithUTF8String:firmware.c_str()];
    if (!firmwareCode || [firmwareCode caseInsensitiveCompare:@"RH-29"] != NSOrderedSame) {
        report.text = @"RH29_MACHINE1_E\nSTOP_REASON=io_error\nDETAIL=current device is not RH-29\n";
        return report;
    }

    const std::string rom_path = eka2l1::add_path(storage, "roms/rh-29/SYM.ROM");
    LOG_INFO(eka2l1::FRONTEND_CMDLINE, "[RH29_MACHINE1_E] start budget={} rom={}", budget, rom_path);
    eka2l1::machine::rh29::probe_options options{};
    options.instruction_budget = budget;
    const auto result = eka2l1::machine::rh29::run_probe(rom_path, options);
    const std::string text = eka2l1::machine::rh29::format_report(result);
    report.text = [NSString stringWithUTF8String:text.c_str()];
    report.succeeded = (
        result.stop_reason != eka2l1::machine::rh29::probe_stop_reason::invalid_rom
        && result.stop_reason != eka2l1::machine::rh29::probe_stop_reason::io_error);
    LOG_INFO(eka2l1::FRONTEND_CMDLINE,
        "[RH29_MACHINE1_E] stop succeeded={} executed={}",
        report.succeeded, result.executed_instructions);
    return report;
}

'''
if "runRH29MachineProbeWithInstructionBudget" not in impl:
    marker = "installedDevices {"
    pos = impl.find(marker)
    if pos < 0:
        raise SystemExit("installedDevices implementation anchor not found")
    line_start = impl.rfind("\n", 0, pos) + 1
    impl = impl[:line_start] + probe_method + impl[line_start:]
ios_impl_path.write_text(impl, encoding="utf-8")

swift = swift_bridge_path.read_text(encoding="utf-8")
probe_item = """
struct EKA2L1MachineProbeItem: Sendable {
    let succeeded: Bool
    let text: String
}

"""
class_anchor = "@MainActor\nfinal class EKA2L1Bridge"
if "struct EKA2L1MachineProbeItem: Sendable" not in swift:
    if class_anchor not in swift:
        raise SystemExit("Swift bridge class anchor not found")
    swift = swift.replace(class_anchor, probe_item + class_anchor, 1)
probe_wrapper = """

    nonisolated static func runRH29MachineProbe(instructionBudget: UInt32) -> EKA2L1MachineProbeItem {
        let report = EKA2L1Emulator.shared().runRH29MachineProbe(instructionBudget: instructionBudget)
        return EKA2L1MachineProbeItem(succeeded: report.succeeded, text: report.text)
    }
"""
if "static func runRH29MachineProbe(instructionBudget:" not in swift:
    init_anchor = "    private init() {}\n"
    if init_anchor not in swift:
        raise SystemExit("Swift bridge init anchor not found")
    swift = swift.replace(init_anchor, init_anchor + probe_wrapper, 1)
swift_bridge_path.write_text(swift, encoding="utf-8")

content = content_path.read_text(encoding="utf-8")
if "showingRH29MachineProbe" not in content:
    state_anchor = "    @State private var showingSettings = false\n"
    if state_anchor not in content:
        raise SystemExit("ContentView settings state anchor not found")
    content = content.replace(
        state_anchor,
        state_anchor + "    @State private var showingRH29MachineProbe = false\n",
        1)

destination = """            .navigationDestination(isPresented: $showingRH29MachineProbe) {
                RH29MachineProbeView()
            }
"""
if "RH29MachineProbeView()" not in content:
    settings_dest = '            .navigationDestination(isPresented: $showingSettings) { SettingsView() }\n'
    if settings_dest not in content:
        raise SystemExit("ContentView settings destination anchor not found")
    content = content.replace(settings_dest, settings_dest + destination, 1)

if 'firmwareCode.caseInsensitiveCompare("RH-29")' not in content:
    settings_button = """                    Button {
                        showingSettings = true
                    } label: {
                        Label("settings.title", systemImage: "gearshape")
                    }
"""
    probe_button = """

                    if store.currentDevice?.firmwareCode.caseInsensitiveCompare("RH-29") == .orderedSame {
                        Button {
                            showingRH29MachineProbe = true
                        } label: {
                            Label("RH-29 Machine Probe", systemImage: "waveform.path.ecg")
                        }
                    }
"""
    if settings_button not in content:
        raise SystemExit("ContentView More/Settings button anchor not found")
    content = content.replace(settings_button, settings_button + probe_button, 1)
content_path.write_text(content, encoding="utf-8")

print("MACHINE1-E sources + iOS probe integration staged")
