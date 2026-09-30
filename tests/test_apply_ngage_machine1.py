import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "apply_ngage_machine1.py"


class ApplyNGageMachine1Tests(unittest.TestCase):
    def make_upstream(self, *, good_cmake=True):
        td = tempfile.TemporaryDirectory()
        root = Path(td.name)
        app = root / "src/emu/ios/App"
        bridge = root / "src/emu/ios/Bridge"
        cpu_inc = root / "src/emu/cpu/include/cpu/dyncom"
        cpu_src = root / "src/emu/cpu/src/dyncom"
        app.mkdir(parents=True)
        bridge.mkdir(parents=True)
        cpu_inc.mkdir(parents=True)
        cpu_src.mkdir(parents=True)

        (app / "ContentView.swift").write_text('''import SwiftUI
struct ContentView: View {
    @StateObject private var store = DeviceStore()
    @State private var showingSettings = false
    @State private var showingDeviceManager = false

    var body: some View {
        NavigationStack {
            Text("home")
                .navigationDestination(isPresented: $showingSettings) { SettingsView() }
                .navigationDestination(isPresented: $showingDeviceManager) {
                    DeviceManagerView(store: store) { }
                }
        }
    }

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        if !store.devices.isEmpty {
            ToolbarItemGroup(placement: .topBarTrailing) {
                Menu("home.more", systemImage: "ellipsis.circle") {
                    Button {
                        showingSettings = true
                    } label: {
                        Label("settings.title", systemImage: "gearshape")
                    }
                    Button {
                    } label: {
                        Label("home.showSystemApps", systemImage: "eye")
                    }
                }
            }
        }
    }
}
''', encoding="utf-8")
        (app / "EKA2L1Bridge.swift").write_text('''import Foundation
struct EKA2L1NGageInstallItem {
    let result: Int
    let gameName: String
    var succeeded: Bool { result == 0 }
}
@MainActor
final class EKA2L1Bridge {
    static let shared = EKA2L1Bridge()
    private init() {}
    nonisolated static func installedDevices() -> [EKA2L1DeviceItem] { [] }
}
''', encoding="utf-8")
        (bridge / "IosEmulator.h").write_text('''#import <Foundation/Foundation.h>
NS_ASSUME_NONNULL_BEGIN
@interface EKA2L1NGageInstallReport : NSObject
@property(nonatomic, assign) NSInteger result;
@property(nonatomic, copy) NSString *gameName;
@end
@interface EKA2L1Emulator : NSObject
+ (instancetype)shared;
- (NSArray *)installedDevices;
@end
NS_ASSUME_NONNULL_END
''', encoding="utf-8")
        (bridge / "IosEmulator.mm").write_text('''#import "IosEmulator.h"
#include <system/epoc.h>
@implementation EKA2L1NGageInstallReport
@end
namespace eka2l1::ios {
struct emulator {
    std::unique_ptr<eka2l1::system> symsys;
    config::state conf;
    std::atomic<bool> mounted{false};
};
}
@implementation EKA2L1Emulator {
    std::recursive_mutex _sessionMutex;
    std::unique_ptr<eka2l1::ios::emulator> _state;
}
- (NSArray *)installedDevices { return @[]; }
@end
''', encoding="utf-8")

        (cpu_inc / "arm_dyncom.h").write_text('''#pragma once
#include <cpu/dyncom/armstate.h>
namespace eka2l1::arm {
class dyncom_core final : public core {
public:
        explicit dyncom_core(arm::exclusive_monitor *monitor, const std::size_t page_bits);
        ~dyncom_core() override;
};
}
''', encoding="utf-8")
        (cpu_src / "arm_dyncom.cpp").write_text('''#include <cpu/dyncom/arm_dyncom.h>
namespace eka2l1::arm {
    dyncom_core::dyncom_core(arm::exclusive_monitor *monitor, const std::size_t page_bits)
        : monitor_(monitor)
        , state_(nullptr)
        , ticks_executed_(0)
        , mem_cache_(page_bits) {
        state_ = std::make_unique<ARMul_State>(this, USER32MODE);
        state_->mem_cache_ = &mem_cache_;
    }
}
''', encoding="utf-8")

        if good_cmake:
            cmake = '''set(EKA2L1_IOS_SOURCES
    "${EKA2L1_IOS_APP_DIR}/ContentView.swift"
    "${EKA2L1_IOS_APP_DIR}/EKA2L1Bridge.swift"
    "${EKA2L1_IOS_BRIDGE_DIR}/IosEmulator.h"
    "${EKA2L1_IOS_BRIDGE_DIR}/IosEmulator.mm"
    "${EKA2L1_IOS_BRIDGE_DIR}/EKA2L1-Bridging-Header.h"
)
'''
        else:
            cmake = "set(EKA2L1_IOS_SOURCES)\n"
        (root / "src/emu/ios/CMakeLists.txt").write_text(cmake, encoding="utf-8")
        return td, root

    def run_script(self, root):
        return subprocess.run(
            [sys.executable, str(SCRIPT), str(root)],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

    def test_refuses_missing_cmake_anchor(self):
        td, root = self.make_upstream(good_cmake=False)
        self.addCleanup(td.cleanup)
        result = self.run_script(root)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("anchor", result.stdout.lower())

    def test_copies_machine_sources_and_swift_view(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        result = self.run_script(root)
        self.assertEqual(result.returncode, 0, result.stdout)
        bridge = root / "src/emu/ios/Bridge"
        app = root / "src/emu/ios/App"
        for name in ["RH29MachineModel.h", "RH29MachineModel.cpp", "RH29MachineRunner.h", "RH29MachineRunner.cpp"]:
            self.assertTrue((bridge / name).is_file(), name)
        self.assertTrue((app / "RH29MachineProbeView.swift").is_file())
        self.assertIn('#include "RH29MachineModel.h"', (bridge / "RH29MachineModel.cpp").read_text())
        self.assertIn('#include "RH29MachineModel.h"', (bridge / "RH29MachineRunner.h").read_text())
        self.assertIn('#include "RH29MachineRunner.h"', (bridge / "RH29MachineRunner.cpp").read_text())

    def test_adds_cmake_sources_exactly_once_and_is_idempotent(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        first = self.run_script(root)
        self.assertEqual(first.returncode, 0, first.stdout)
        second = self.run_script(root)
        self.assertEqual(second.returncode, 0, second.stdout)
        cmake = (root / "src/emu/ios/CMakeLists.txt").read_text()
        for source in ["RH29MachineProbeView.swift", "RH29MachineModel.h", "RH29MachineModel.cpp", "RH29MachineRunner.h", "RH29MachineRunner.cpp"]:
            self.assertEqual(cmake.count(source), 1, source)

    def test_patches_objc_bridge_with_rh29_gate_and_installed_rom_path(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        result = self.run_script(root)
        self.assertEqual(result.returncode, 0, result.stdout)
        header = (root / "src/emu/ios/Bridge/IosEmulator.h").read_text()
        impl = (root / "src/emu/ios/Bridge/IosEmulator.mm").read_text()
        self.assertIn("EKA2L1MachineProbeReport", header)
        self.assertIn("runRH29MachineProbeWithInstructionBudget", header)
        self.assertIn("NS_SWIFT_NAME(runRH29MachineProbe(instructionBudget:))", header)
        self.assertIn('caseInsensitiveCompare:@"RH-29"', impl)
        self.assertIn('roms/rh-29/SYM.ROM', impl)
        self.assertIn("[RH29_MACHINE1_B]", impl)
        self.assertNotIn("reset(false", impl)
        self.assertNotIn("set_device(", impl)

    def test_patches_swift_bridge_and_rh29_only_content_view_entry(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        result = self.run_script(root)
        self.assertEqual(result.returncode, 0, result.stdout)
        bridge = (root / "src/emu/ios/App/EKA2L1Bridge.swift").read_text()
        content = (root / "src/emu/ios/App/ContentView.swift").read_text()
        self.assertIn("struct EKA2L1MachineProbeItem: Sendable", bridge)
        self.assertIn("runRH29MachineProbe(instructionBudget:", bridge)
        self.assertIn("showingRH29MachineProbe", content)
        self.assertIn('firmwareCode.caseInsensitiveCompare("RH-29")', content)
        self.assertIn('Label("RH-29 Machine Probe"', content)
        self.assertIn("RH29MachineProbeView()", content)

    def test_probe_view_runs_detached_exports_report_and_offers_all_budgets(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        result = self.run_script(root)
        self.assertEqual(result.returncode, 0, result.stdout)
        view = (root / "src/emu/ios/App/RH29MachineProbeView.swift").read_text()
        self.assertIn("Task.detached(priority: .userInitiated)", view)
        self.assertIn("RH29_MACHINE1_B.txt", view)
        self.assertIn("ShareLink", view)
        self.assertIn(".textSelection(.enabled)", view)
        self.assertIn("[1_000, 10_000, 100_000, 1_000_000]", view)
        self.assertIn("@State private var instructionBudget: UInt32 = 1_000", view)
        self.assertIn("MACHINE1-B", view)

    def test_adds_probe_only_svc_dyncom_constructor_without_changing_default_mode(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        result = self.run_script(root)
        self.assertEqual(result.returncode, 0, result.stdout)
        header = (root / "src/emu/cpu/include/cpu/dyncom/arm_dyncom.h").read_text()
        impl = (root / "src/emu/cpu/src/dyncom/arm_dyncom.cpp").read_text()
        self.assertIn("PrivilegeMode initial_mode", header)
        self.assertIn("dyncom_core(monitor, page_bits, USER32MODE)", impl)
        self.assertIn("ARMul_State>(this, initial_mode)", impl)
        self.assertIn("SVC32MODE", (root / "src/emu/ios/Bridge/RH29MachineRunner.cpp").read_text())

    def test_keeps_original_ios_and_dyncom_anchors_present(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        result = self.run_script(root)
        self.assertEqual(result.returncode, 0, result.stdout)
        content = (root / "src/emu/ios/App/ContentView.swift").read_text()
        impl = (root / "src/emu/ios/Bridge/IosEmulator.mm").read_text()
        dyncom = (root / "src/emu/cpu/src/dyncom/arm_dyncom.cpp").read_text()
        self.assertIn('Menu("home.more"', content)
        self.assertIn("installedDevices", impl)
        self.assertIn("USER32MODE", dyncom)


if __name__ == "__main__":
    unittest.main()
