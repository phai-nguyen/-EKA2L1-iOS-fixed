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
        (cpu_inc / "armstate.h").write_text('    std::array<std::uint32_t, 7> Reg_firq{}; // R8---R14 FIRQ\n', encoding="utf-8")
        (cpu_src / "armstate.cpp").write_text(
            (ROOT / "tests/fixtures/change_privilege_mode.cpp").read_text(encoding="utf-8"), encoding="utf-8")

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
        model = (bridge / "RH29MachineModel.h").read_text()
        runner = (bridge / "RH29MachineRunner.cpp").read_text()
        self.assertIn("candidate_ram_probe_base = 0x0A000000u", model)
        self.assertIn("candidate_ram_probe_size = 16u", model)
        self.assertIn("candidate_ram_probe_stride = 0x0000003Cu", model)
        self.assertIn("candidate_ram_probe_loop_windows = 8u", model)
        self.assertIn("candidate_bootstrap_copy_source = 0x00000744u", model)
        self.assertIn("candidate_bootstrap_copy_size = 0x108u", model)
        self.assertIn("candidate_bootstrap_copy_pc = 0x00002344u", model)
        self.assertIn("candidate_bootstrap_copy_lr = 0x000003C8u", model)
        self.assertIn("candidate_bootstrap_stack_top", model)
        self.assertIn("candidate_bootstrap_stack_push_size = 0x20u", model)
        self.assertIn("candidate_bootstrap_stack_push_pc = 0x000011A8u", model)
        self.assertIn("candidate_bootstrap_stack_push_instruction = 0xE92D47F0u", model)
        self.assertIn("candidate_bootstrap_nested_stack_top = 0x0A000FBCu", model)
        self.assertIn("candidate_bootstrap_nested_stack_push_size = 0x10u", model)
        self.assertIn("candidate_bootstrap_nested_stack_push_pc = 0x000022A0u", model)
        self.assertIn("candidate_bootstrap_nested_stack_push_lr = 0x000011C8u", model)
        self.assertIn("candidate_bootstrap_nested_stack_push_instruction = 0xE92D4070u", model)
        self.assertIn("candidate_bootstrap_third_stack_top = 0x0A000FBCu", model)
        self.assertIn("candidate_bootstrap_third_stack_push_size = 0x0Cu", model)
        self.assertIn("candidate_bootstrap_third_stack_push_pc = 0x00002228u", model)
        self.assertIn("candidate_bootstrap_third_stack_push_lr = 0x000011E0u", model)
        self.assertIn("candidate_bootstrap_third_stack_push_instruction = 0xE92D4030u", model)
        self.assertIn("candidate_ram_bank_probe_anchor_address = 0x0A001100u", model)
        self.assertIn("candidate_ram_bank_probe_anchor_read_pc = 0x00001368u", model)
        self.assertIn("candidate_ram_bank_probe_anchor_read_lr = 0x00001348u", model)
        self.assertIn("candidate_ram_bank_probe_anchor_read_instruction = 0xE594C000u", model)
        self.assertIn("candidate_ram_bank_sparse_probe_first_offset = 0x00004000u", model)
        self.assertIn("candidate_ram_bank_sparse_probe_limit = 0x01000000u", model)
        self.assertIn("candidate_ram_bank_sparse_probe_point_count = 10u", model)
        self.assertIn("candidate_ram_bank_sparse_probe_original_read_pc = 0x0000137Cu", model)
        self.assertIn("candidate_ram_bank_sparse_probe_test_write_pc = 0x00001380u", model)
        self.assertIn("candidate_ram_bank_sparse_probe_anchor_verify_pc = 0x00001384u", model)
        self.assertIn("candidate_ram_bank_sparse_probe_readback_pc = 0x00001390u", model)
        self.assertIn("candidate_ram_bank_sparse_probe_restore_pc = 0x000013A4u", model)
        self.assertIn("candidate_bootstrap_local_frame_word_address = 0x0A000FBCu", model)
        self.assertIn("candidate_bootstrap_local_frame_word_write_pc = 0x000012ACu", model)
        self.assertIn("candidate_bootstrap_local_frame_word_write_lr = 0x00001270u", model)
        self.assertIn("candidate_bootstrap_local_frame_word_instruction = 0xE58D0000u", model)
        self.assertIn("candidate_bootstrap_local_frame_word_value = 0x09FFF400u", model)
        self.assertIn("candidate_bootstrap_relocation_source = candidate_bootstrap_copy_base", model)
        self.assertIn("candidate_bootstrap_relocation_base = 0x09FFF400u", model)
        self.assertIn("candidate_bootstrap_relocation_size = candidate_bootstrap_copy_size", model)
        self.assertIn("candidate_bootstrap_relocation_pc = 0x00002344u", model)
        self.assertIn("candidate_bootstrap_relocation_lr = 0x000012B8u", model)
        self.assertIn("candidate_bootstrap_relocation_instruction = 0xE4803004u", model)
        self.assertIn("candidate_bootstrap_local_frame_tail_word_count = 4u", model)
        self.assertIn("0x0A000FC0u, 0x0A000FC4u, 0x0A000FC8u, 0x0A000FCCu", model)
        self.assertIn("0x000012B8u, 0x000012BCu, 0x000012C0u, 0x000012C4u", model)
        self.assertIn("0xE58DA004u, 0xE58D9008u, 0xE58D800Cu, 0xE58D7010u", model)
        self.assertIn("0x00000080u, 0x00000000u, 0x00000000u, 0x01170000u", model)
        self.assertIn("candidate_bootstrap_post_copy_mutation_address", model)
        self.assertIn("candidate_bootstrap_post_copy_mutation_pc = 0x000011D4u", model)
        self.assertIn("candidate_bootstrap_post_copy_mutation_lr = 0x000022C4u", model)
        self.assertIn("candidate_bootstrap_post_copy_mutation_instruction = 0xE5803008u", model)
        self.assertIn("candidate_bootstrap_post_copy_mutation_value = 0xB2800021u", model)
        self.assertIn("candidate_bootstrap_record_table_base", model)
        self.assertIn("candidate_bootstrap_record_count = 16u", model)
        self.assertIn("candidate_bootstrap_record_stride = 0x10u", model)
        self.assertIn("candidate_bootstrap_record_loop_mutation_pc = 0x00002220u", model)
        self.assertIn("candidate_bootstrap_record_loop_mutation_lr = 0x00002244u", model)
        self.assertIn("candidate_bootstrap_record_loop_mutation_instruction = 0xE5803008u", model)
        self.assertIn("candidate_bootstrap_record_loop_or_mask = 0x80000020u", model)
        self.assertIn("candidate_post_probe_workspace_base", model)
        self.assertIn("candidate_post_probe_workspace_size = 0x20u", model)
        self.assertIn("low_vector_shadow_base = 0x00000000u", model)
        self.assertIn("low_vector_shadow_size = 9u * sizeof(std::uint32_t)", model)
        self.assertIn("symbian_boot_eight_step_sparse_loop_stride_0x3c", runner)
        self.assertIn("post_probe_0x20_candidate_zero_seeded_mutable", runner)
        self.assertIn("POST_RAM_WORKSPACE_READ_COUNT", runner)
        self.assertIn("arm_low_vectors_36byte_rom_seeded_mutable_write32", runner)
        self.assertIn("LOW_VECTOR_SHADOW_WRITE_COUNT", runner)
        self.assertIn("LOW_VECTOR_SHADOW_STATUS=X_admits_only_W_observed_write_at_0x20_0x24_plus_fail_closed", runner)
        self.assertIn("SDRAM_WRITE_TRACE_POLICY=first_16_exact_transactions_no_new_mapping", runner)
        self.assertIn('prefix << "SDRAM_WRITE_TRACE_"', runner)
        self.assertIn("EARLY_SETUP_TRACE_POLICY=pc_0x00000B00_0x00000B7F_all_r0_r12_no_model_change", runner)
        self.assertIn('prefix << "EARLY_SETUP_TRACE_"', runner)
        staged_runner_h = (bridge / "RH29MachineRunner.h").read_text()
        self.assertIn("early_setup_trace_pc_begin = 0x00000B00u", staged_runner_h)
        self.assertIn("early_setup_trace_pc_end = 0x00000B80u", staged_runner_h)
        self.assertIn("full_a32_trace_entry", staged_runner_h)
        self.assertIn("low_page_trace_capacity = 4096u", staged_runner_h)
        self.assertIn("callsite_trace_pc_begin = 0x00000300u", staged_runner_h)
        self.assertIn("setup_literal_pool_begin = 0x00000B90u", staged_runner_h)
        self.assertIn("ram_bank_handler_dump_begin = 0x000012A0u", staged_runner_h)
        self.assertIn("ram_bank_handler_dump_end = 0x00001400u", staged_runner_h)
        self.assertIn("MACHINE1_W_DIAG_POLICY=no_memory_response_change_no_remap_assumption", runner)
        self.assertIn("LOW_PAGE_TRACE_POLICY=low_4k_all_guest_bus_callbacks_no_response_change", runner)
        self.assertIn("CP15_TRACE_POLICY=all_condition_passed_p15_seen_before_barrier", runner)
        self.assertIn("SETUP_B68_INSTRUCTION_INDEX_BEFORE", runner)
        self.assertIn("RAM_BANK_PROBE_ANCHOR_POLICY=AJ_device_exact_first_read_pc_0x1368_lr_0x1348_addr_0x0a001100_synthetic_zero_seed_read_only_no_range_map", runner)
        self.assertIn("RAM_BANK_HANDLER_DUMP_POLICY=raw_rom_words_0x000012a0_0x000013fc_context_no_semantic_label", runner)
        self.assertIn("RAM_BANK_SPARSE_PROBE_POLICY=AK_raw_handler_10_power2_words_synthetic_independent_zero_seed_exact_callsites_no_range_map", runner)
        self.assertIn("RAM_BANK_SPARSE_PROBE_RESTORE_WRITE_COUNT", runner)
        self.assertIn("BOOTSTRAP_LOCAL_FRAME_WORD_POLICY=AL_device_exact_word_addr_0x0a000fbc_pc_0x12ac_lr_0x1270_value_0x09fff400_initialized_readback_only_no_range_widen", runner)
        self.assertIn("BOOTSTRAP_LOCAL_FRAME_WORD_WRITE_COUNT", runner)
        self.assertIn("BOOTSTRAP_RELOCATION_POLICY=AM_device_exact_0x108_copy_0x0a000000_to_0x09fff400_pc_0x2344_lr_0x12b8_source_verified_initialized_readback_only_no_range_widen", runner)
        self.assertIn("BOOTSTRAP_RELOCATION_WRITE_COUNT", runner)
        self.assertIn("BOOTSTRAP_RELOCATION_INITIALIZED_BYTES", runner)
        self.assertIn("BOOTSTRAP_LOCAL_FRAME_TAIL_POLICY=AN_exact_four_word_tail_pc_0x12b8_0x12bc_0x12c0_0x12c4_lr_0x12b8_values_0x80_0_0_0x01170000_initialized_readback_only_no_range_widen", runner)
        self.assertIn("BOOTSTRAP_LOCAL_FRAME_TAIL_WRITE_COUNT", runner)
        self.assertIn("BOOTSTRAP_LOCAL_FRAME_TAIL_INITIALIZED_WORDS", runner)
        self.assertIn("candidate_ram_bank_probe_anchor_read_count", runner)
        self.assertIn("RAM_PROBE_READ_COUNT", runner)
        self.assertIn("BOOTSTRAP_COPY_POLICY", runner)
        self.assertIn("Y_device_r11_0x744_r2_0x42_exact_write32_pc_0x2344_lr_0x3c8_initialized_readback_only", runner)
        self.assertIn("BOOTSTRAP_COPY_INITIALIZED_BYTES", runner)
        self.assertIn("BOOTSTRAP_STACK_POLICY", runner)
        self.assertIn("Z_device_sp_base_plus_0xff0_exact_first_stmdb_32bytes_pc_0x11a8_lr_0x3c8_initialized_readback_only", runner)
        self.assertIn("BOOTSTRAP_STACK_INITIALIZED_BYTES", runner)
        self.assertIn("BOOTSTRAP_NESTED_STACK_POLICY", runner)
        self.assertIn("AD_device_exact_second_plus_third_stmdb_same_16byte_nested_stack_window_no_range_widen", runner)
        self.assertIn("BOOTSTRAP_NESTED_STACK_INITIALIZED_BYTES", runner)
        self.assertIn("BOOTSTRAP_THIRD_STACK_POLICY=AC_device_exact_existing_nested_stack_12bytes_pc_0x2228_lr_0x11e0_no_range_widen", runner)
        self.assertIn("BOOTSTRAP_THIRD_STACK_OBSERVED_INSTRUCTION", runner)
        self.assertIn("BOOTSTRAP_POST_COPY_MUTATION_POLICY", runner)
        self.assertIn("AB_device_exact_existing_word_writeback_addr_0x0a000010_pc_0x11d4_lr_0x22c4_value_0xb2800021_initialized_copy_only", runner)
        self.assertIn("BOOTSTRAP_POST_COPY_MUTATION_ACCEPTED_COUNT", runner)
        self.assertIn("BOOTSTRAP_RECORD_LOOP_MUTATION_POLICY", runner)
        self.assertIn("AD_device_16record_stride_0x10_field_plus8_pc_0x2220_lr_0x2244_exact_transform_initialized_only", runner)
        self.assertIn("BOOTSTRAP_RECORD_LOOP_MUTATION_ACCEPTED_COUNT", runner)
        self.assertIn("RH29_MACHINE1_AO", runner)
        self.assertIn('write_hex(out, (p + "R11").c_str(), entry.r11)', runner)

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
        self.assertIn("[RH29_MACHINE1_AO]", impl)
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
        self.assertNotIn('firmwareCode.caseInsensitiveCompare("RH-29")', content)
        self.assertIn('Label("RH-29 Machine Probe"', content)
        self.assertIn("RH29MachineProbeView()", content)

    def test_probe_view_runs_detached_exports_report_and_offers_all_budgets(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        result = self.run_script(root)
        self.assertEqual(result.returncode, 0, result.stdout)
        view = (root / "src/emu/ios/App/RH29MachineProbeView.swift").read_text()
        self.assertIn("Task.detached(priority: .userInitiated)", view)
        self.assertIn("RH29_MACHINE1_AO.txt", view)
        self.assertIn("ShareLink", view)
        self.assertLess(view.index('Label("Chạy probe"'), view.index("ShareLink"))
        self.assertLess(view.index("ShareLink"), view.index("if !reportText.isEmpty"))
        self.assertEqual(view.count("ShareLink"), 1)
        self.assertIn(".textSelection(.enabled)", view)
        self.assertIn("[1_000, 10_000, 100_000, 1_000_000]", view)
        self.assertIn("@State private var instructionBudget: UInt32 = 10_000", view)
        self.assertIn("MACHINE1-AO", view)
        self.assertNotIn("RH29_MACHINE1_R.txt", view)

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

    def test_fiq_banking_preserves_shared_r8_r12(self):
        td, root = self.make_upstream()
        self.addCleanup(td.cleanup)
        result = self.run_script(root)
        self.assertEqual(result.returncode, 0, result.stdout)
        header = (root / "src/emu/cpu/include/cpu/dyncom/armstate.h").read_text()
        impl = (root / "src/emu/cpu/src/dyncom/armstate.cpp").read_text()
        self.assertIn("Reg_nonfiq_r8_r12", header)
        self.assertEqual(impl.count("Reg_nonfiq_r8_r12"), 3)
        program = '''#include <algorithm>
#include <array>
#include <cstdint>
#include <cassert>
enum { USERBANK, IRQBANK, SVCBANK, ABORTBANK, UNDEFBANK, FIQBANK, SYSTEMBANK };
enum { USER32MODE=0x10, FIQ32MODE=0x11, IRQ32MODE=0x12, SVC32MODE=0x13,
       ABORT32MODE=0x17, UNDEF32MODE=0x1b, SYSTEM32MODE=0x1f };
struct ARMul_State {
  std::array<std::uint32_t,16> Reg{};
  std::array<std::uint32_t,2> Reg_usr{}, Reg_irq{}, Reg_svc{}, Reg_abort{}, Reg_undef{};
  std::array<std::uint32_t,7> Reg_firq{}, Spsr{};
  std::array<std::uint32_t,5> Reg_nonfiq_r8_r12{};
  std::uint32_t Mode=SVC32MODE, Bank=SVCBANK, Cpsr=SVC32MODE, Spsr_copy=0;
  void ChangePrivilegeMode(std::uint32_t new_mode);
};
''' + impl + '''
int main() {
  ARMul_State cpu;
  for (int i=8;i<=12;i++) cpu.Reg[i]=0x700+i;
  cpu.Reg[13]=0x1234;
  cpu.ChangePrivilegeMode(FIQ32MODE);
  for (int i=8;i<=12;i++) cpu.Reg[i]=0x900+i;
  cpu.Reg[13]=0x4321;
  cpu.ChangePrivilegeMode(SVC32MODE);
  for (int i=8;i<=12;i++) assert(cpu.Reg[i]==0x700u+i);
  assert(cpu.Reg[13]==0x1234);
  cpu.ChangePrivilegeMode(FIQ32MODE);
  for (int i=8;i<=12;i++) assert(cpu.Reg[i]==0x900u+i);
  assert(cpu.Reg[13]==0x4321);
  cpu.ChangePrivilegeMode(IRQ32MODE);
  for (int i=8;i<=12;i++) assert(cpu.Reg[i]==0x700u+i);
  cpu.ChangePrivilegeMode(SVC32MODE);
  assert(cpu.Reg[11]==0x70b);
}
'''
        binary = root / "banking_test"
        compiled = subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-pedantic", "-x", "c++", "-", "-o", str(binary)],
                                  input=program, text=True, capture_output=True)
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        run = subprocess.run([str(binary)], capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stderr)


if __name__ == "__main__":
    unittest.main()
