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
state_header_path = root / "src/emu/cpu/include/cpu/dyncom/armstate.h"
state_impl_path = root / "src/emu/cpu/src/dyncom/armstate.cpp"

required_targets = [
    cmake_path,
    content_path,
    swift_bridge_path,
    ios_header_path,
    ios_impl_path,
    dyncom_header_path,
    dyncom_impl_path,
    state_header_path,
    state_impl_path,
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

# FIQ has its own R8-R14. Other modes share R8-R12, so save that shared bank
# before loading FIQ and restore it when leaving FIQ. Guard pinned-upstream
# anchors to avoid silently applying a partial CPU fix to a different revision.
state_header = state_header_path.read_text(encoding="utf-8")
shared_decl = "    std::array<std::uint32_t, 5> Reg_nonfiq_r8_r12{}; // Shared R8-R12 outside FIQ\n"
fiq_decl = "    std::array<std::uint32_t, 7> Reg_firq{}; // R8---R14 FIRQ\n"
if "Reg_nonfiq_r8_r12" not in state_header:
    if state_header.count(fiq_decl) != 1:
        raise SystemExit("Dyncom FIQ register declaration anchor not found")
    state_header = state_header.replace(fiq_decl, fiq_decl + shared_decl, 1)
    state_header_path.write_text(state_header, encoding="utf-8")

state_impl = state_impl_path.read_text(encoding="utf-8")
save_fiq = "            std::copy(Reg.begin() + 8, Reg.end() - 1, Reg_firq.begin());\n"
load_fiq = "            std::copy(Reg_firq.begin(), Reg_firq.end(), Reg.begin() + 8);\n"
restore_shared = "            std::copy(Reg_nonfiq_r8_r12.begin(), Reg_nonfiq_r8_r12.end(), Reg.begin() + 8);\n"
save_shared = "            std::copy(Reg.begin() + 8, Reg.begin() + 13, Reg_nonfiq_r8_r12.begin());\n"
if "Reg_nonfiq_r8_r12" not in state_impl:
    if state_impl.count(save_fiq) != 1 or state_impl.count(load_fiq) != 1:
        raise SystemExit("Dyncom FIQ mode transition anchors not found")
    state_impl = state_impl.replace(save_fiq, save_fiq + restore_shared, 1)
    state_impl = state_impl.replace(load_fiq, save_shared + load_fiq, 1)
    state_impl_path.write_text(state_impl, encoding="utf-8")
elif state_impl.count("Reg_nonfiq_r8_r12") != 3:
    raise SystemExit("Dyncom FIQ mode transition is partially patched")


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
        text = text.replace("RH29_MACHINE1_O", "RH29_MACHINE1_AY")
        text = text.replace("MACHINE1-O probes", "MACHINE1-AY probes")
        text = text.replace("MACHINE1-O must not let Dyncom", "MACHINE1-AY must not let Dyncom")

        report_anchor = '        out << "FLASH_UNLOCK_STAGE_AT_STOP=" << result.flash_unlock_stage_at_stop << "\\n";\n        write_hex(out, "KERN_DATA_ADDRESS", result.header.kern_data_address);\n'
        report_insert = (
            '        out << "FLASH_UNLOCK_STAGE_AT_STOP=" << result.flash_unlock_stage_at_stop << "\\n";\n'
            '        out << "RAM_PROBE_POLICY=" << (result.candidate_ram_probe_enabled\n'
            '            ? "symbian_boot_eight_step_sparse_loop_stride_0x3c" : "disabled") << "\\n";\n'
            '        write_hex(out, "RAM_PROBE_BASE", candidate_ram_probe_base);\n'
            '        out << "RAM_PROBE_SIZE=" << candidate_ram_probe_size << "\\n";\n'
            '        out << "RAM_PROBE_READ_COUNT=" << result.candidate_ram_probe_read_count << "\\n";\n'
            '        out << "RAM_PROBE_WRITE_COUNT=" << result.candidate_ram_probe_write_count << "\\n";\n'
            '        out << "BOOTSTRAP_COPY_POLICY=" << (result.candidate_bootstrap_copy_enabled\n'
            '            ? "Y_device_r11_0x744_r2_0x42_exact_write32_pc_0x2344_lr_0x3c8_initialized_readback_only" : "disabled") << "\\n";\n'
            '        write_hex(out, "BOOTSTRAP_COPY_SOURCE", candidate_bootstrap_copy_source);\n'
            '        write_hex(out, "BOOTSTRAP_COPY_BASE", candidate_bootstrap_copy_base);\n'
            '        out << "BOOTSTRAP_COPY_SIZE=" << candidate_bootstrap_copy_size << "\\n";\n'
            '        out << "BOOTSTRAP_COPY_WRITE_WIDTH_BITS=" << (candidate_bootstrap_copy_write_width * 8u) << "\\n";\n'
            '        write_hex(out, "BOOTSTRAP_COPY_GATE_PC", candidate_bootstrap_copy_pc);\n'
            '        write_hex(out, "BOOTSTRAP_COPY_GATE_LR", candidate_bootstrap_copy_lr);\n'
            '        out << "BOOTSTRAP_COPY_READ_COUNT=" << result.candidate_bootstrap_copy_read_count << "\\n";\n'
            '        out << "BOOTSTRAP_COPY_WRITE_COUNT=" << result.candidate_bootstrap_copy_write_count << "\\n";\n'
            '        out << "BOOTSTRAP_COPY_INITIALIZED_BYTES=" << result.candidate_bootstrap_copy_initialized_bytes << "\\n";\n'
            '        out << "BOOTSTRAP_POST_COPY_MUTATION_POLICY=" << (result.candidate_bootstrap_post_copy_mutation_enabled\n'
            '            ? "AB_device_exact_existing_word_writeback_addr_0x0a000010_pc_0x11d4_lr_0x22c4_value_0xb2800021_initialized_copy_only" : "disabled") << "\\n";\n'
            '        write_hex(out, "BOOTSTRAP_POST_COPY_MUTATION_ADDRESS", candidate_bootstrap_post_copy_mutation_address);\n'
            '        out << "BOOTSTRAP_POST_COPY_MUTATION_WIDTH_BITS=" << (candidate_bootstrap_post_copy_mutation_width * 8u) << "\\n";\n'
            '        write_hex(out, "BOOTSTRAP_POST_COPY_MUTATION_GATE_PC", candidate_bootstrap_post_copy_mutation_pc);\n'
            '        write_hex(out, "BOOTSTRAP_POST_COPY_MUTATION_GATE_LR", candidate_bootstrap_post_copy_mutation_lr);\n'
            '        write_hex(out, "BOOTSTRAP_POST_COPY_MUTATION_OBSERVED_INSTRUCTION", candidate_bootstrap_post_copy_mutation_instruction);\n'
            '        write_hex(out, "BOOTSTRAP_POST_COPY_MUTATION_OBSERVED_VALUE", candidate_bootstrap_post_copy_mutation_value);\n'
            '        out << "BOOTSTRAP_POST_COPY_MUTATION_ACCEPTED_COUNT=" << result.candidate_bootstrap_post_copy_mutation_count << "\\n";\n'
            '        out << "BOOTSTRAP_RECORD_LOOP_MUTATION_POLICY=" << (result.candidate_bootstrap_record_loop_mutation_enabled\n'
            '            ? "AD_device_16record_stride_0x10_field_plus8_pc_0x2220_lr_0x2244_exact_transform_initialized_only" : "disabled") << "\\n";\n'
            '        write_hex(out, "BOOTSTRAP_RECORD_TABLE_BASE", candidate_bootstrap_record_table_base);\n'
            '        out << "BOOTSTRAP_RECORD_COUNT=" << candidate_bootstrap_record_count << "\\n";\n'
            '        out << "BOOTSTRAP_RECORD_STRIDE=" << candidate_bootstrap_record_stride << "\\n";\n'
            '        out << "BOOTSTRAP_RECORD_CONTROL_OFFSET=" << candidate_bootstrap_record_control_offset << "\\n";\n'
            '        write_hex(out, "BOOTSTRAP_RECORD_LOOP_GATE_PC", candidate_bootstrap_record_loop_mutation_pc);\n'
            '        write_hex(out, "BOOTSTRAP_RECORD_LOOP_GATE_LR", candidate_bootstrap_record_loop_mutation_lr);\n'
            '        write_hex(out, "BOOTSTRAP_RECORD_LOOP_OBSERVED_INSTRUCTION", candidate_bootstrap_record_loop_mutation_instruction);\n'
            '        write_hex(out, "BOOTSTRAP_RECORD_LOOP_CLEAR_MASK", candidate_bootstrap_record_loop_clear_mask);\n'
            '        write_hex(out, "BOOTSTRAP_RECORD_LOOP_OR_MASK", candidate_bootstrap_record_loop_or_mask);\n'
            '        out << "BOOTSTRAP_RECORD_LOOP_MUTATION_ACCEPTED_COUNT=" << result.candidate_bootstrap_record_loop_mutation_count << "\\n";\n'
            '        out << "BOOTSTRAP_STACK_POLICY=" << (result.candidate_bootstrap_stack_enabled\n'
            '            ? "Z_device_sp_base_plus_0xff0_exact_first_stmdb_32bytes_pc_0x11a8_lr_0x3c8_initialized_readback_only" : "disabled") << "\\n";\n'
            '        write_hex(out, "BOOTSTRAP_STACK_TOP", candidate_bootstrap_stack_top);\n'
            '        write_hex(out, "BOOTSTRAP_STACK_PUSH_BASE", candidate_bootstrap_stack_push_base);\n'
            '        out << "BOOTSTRAP_STACK_PUSH_SIZE=" << candidate_bootstrap_stack_push_size << "\\n";\n'
            '        out << "BOOTSTRAP_STACK_WRITE_WIDTH_BITS=" << (candidate_bootstrap_stack_write_width * 8u) << "\\n";\n'
            '        write_hex(out, "BOOTSTRAP_STACK_GATE_PC", candidate_bootstrap_stack_push_pc);\n'
            '        write_hex(out, "BOOTSTRAP_STACK_GATE_LR", candidate_bootstrap_stack_push_lr);\n'
            '        write_hex(out, "BOOTSTRAP_STACK_OBSERVED_INSTRUCTION", candidate_bootstrap_stack_push_instruction);\n'
            '        out << "BOOTSTRAP_STACK_READ_COUNT=" << result.candidate_bootstrap_stack_read_count << "\\n";\n'
            '        out << "BOOTSTRAP_STACK_WRITE_COUNT=" << result.candidate_bootstrap_stack_write_count << "\\n";\n'
            '        out << "BOOTSTRAP_STACK_INITIALIZED_BYTES=" << result.candidate_bootstrap_stack_initialized_bytes << "\\n";\n'
            '        out << "BOOTSTRAP_NESTED_STACK_POLICY=" << (result.candidate_bootstrap_nested_stack_enabled\n'
            '            ? "AD_device_exact_second_plus_third_stmdb_same_16byte_nested_stack_window_no_range_widen" : "disabled") << "\\n";\n'
            '        write_hex(out, "BOOTSTRAP_NESTED_STACK_TOP", candidate_bootstrap_nested_stack_top);\n'
            '        write_hex(out, "BOOTSTRAP_NESTED_STACK_PUSH_BASE", candidate_bootstrap_nested_stack_push_base);\n'
            '        out << "BOOTSTRAP_NESTED_STACK_PUSH_SIZE=" << candidate_bootstrap_nested_stack_push_size << "\\n";\n'
            '        out << "BOOTSTRAP_NESTED_STACK_WRITE_WIDTH_BITS=" << (candidate_bootstrap_nested_stack_write_width * 8u) << "\\n";\n'
            '        write_hex(out, "BOOTSTRAP_NESTED_STACK_GATE_PC", candidate_bootstrap_nested_stack_push_pc);\n'
            '        write_hex(out, "BOOTSTRAP_NESTED_STACK_GATE_LR", candidate_bootstrap_nested_stack_push_lr);\n'
            '        write_hex(out, "BOOTSTRAP_NESTED_STACK_OBSERVED_INSTRUCTION", candidate_bootstrap_nested_stack_push_instruction);\n'
            '        out << "BOOTSTRAP_NESTED_STACK_READ_COUNT=" << result.candidate_bootstrap_nested_stack_read_count << "\\n";\n'
            '        out << "BOOTSTRAP_NESTED_STACK_WRITE_COUNT=" << result.candidate_bootstrap_nested_stack_write_count << "\\n";\n'
            '        out << "BOOTSTRAP_NESTED_STACK_INITIALIZED_BYTES=" << result.candidate_bootstrap_nested_stack_initialized_bytes << "\\n";\n'
            '        out << "BOOTSTRAP_THIRD_STACK_POLICY=AC_device_exact_existing_nested_stack_12bytes_pc_0x2228_lr_0x11e0_no_range_widen\\n";\n'
            '        write_hex(out, "BOOTSTRAP_THIRD_STACK_TOP", candidate_bootstrap_third_stack_top);\n'
            '        write_hex(out, "BOOTSTRAP_THIRD_STACK_PUSH_BASE", candidate_bootstrap_third_stack_push_base);\n'
            '        out << "BOOTSTRAP_THIRD_STACK_PUSH_SIZE=" << candidate_bootstrap_third_stack_push_size << "\\n";\n'
            '        write_hex(out, "BOOTSTRAP_THIRD_STACK_GATE_PC", candidate_bootstrap_third_stack_push_pc);\n'
            '        write_hex(out, "BOOTSTRAP_THIRD_STACK_GATE_LR", candidate_bootstrap_third_stack_push_lr);\n'
            '        write_hex(out, "BOOTSTRAP_THIRD_STACK_OBSERVED_INSTRUCTION", candidate_bootstrap_third_stack_push_instruction);\n'
            '        out << "POST_RAM_WORKSPACE_POLICY=" << (result.candidate_post_probe_workspace_enabled\n'
            '            ? "post_probe_0x20_candidate_zero_seeded_mutable" : "disabled") << "\\n";\n'
            '        write_hex(out, "POST_RAM_WORKSPACE_BASE", candidate_post_probe_workspace_base);\n'
            '        out << "POST_RAM_WORKSPACE_SIZE=" << candidate_post_probe_workspace_size << "\\n";\n'
            '        out << "POST_RAM_WORKSPACE_READ_COUNT=" << result.candidate_post_probe_workspace_read_count << "\\n";\n'
            '        out << "POST_RAM_WORKSPACE_WRITE_COUNT=" << result.candidate_post_probe_workspace_write_count << "\\n";\n'
            '        out << "LOW_VECTOR_SHADOW_POLICY=" << (result.low_vector_shadow_enabled\n'
            '            ? "arm_low_vectors_36byte_rom_seeded_mutable_write32" : "disabled") << "\\n";\n'
            '        write_hex(out, "LOW_VECTOR_SHADOW_BASE", low_vector_shadow_base);\n'
            '        out << "LOW_VECTOR_SHADOW_SIZE=" << low_vector_shadow_size << "\\n";\n'
            '        out << "LOW_VECTOR_SHADOW_WRITE_WIDTH_BITS=" << (low_vector_shadow_write_width * 8u) << "\\n";\n'
            '        out << "LOW_VECTOR_SHADOW_READ_COUNT=" << result.low_vector_shadow_read_count << "\\n";\n'
            '        out << "LOW_VECTOR_SHADOW_WRITE_COUNT=" << result.low_vector_shadow_write_count << "\\n";\n'
            '        out << "LOW_VECTOR_SHADOW_STATUS=X_admits_only_W_observed_write_at_0x20_0x24_plus_fail_closed\\n";\n'
            '        out << "SDRAM_WRITE_TRACE_POLICY=first_16_exact_transactions_no_new_mapping\\n";\n'
            '        out << "SDRAM_WRITE_TRACE_COUNT=" << result.candidate_sdram_write_trace_count << "\\n";\n'
            '        for (std::size_t i = 0; i < result.candidate_sdram_write_trace_count && i < result.candidate_sdram_write_trace.size(); ++i) {\n'
            '            const auto &e = result.candidate_sdram_write_trace[i];\n'
            '            std::ostringstream prefix;\n'
            '            prefix << "SDRAM_WRITE_TRACE_" << std::setfill(\'0\') << std::setw(2) << i << "_";\n'
            '            const std::string p = prefix.str();\n'
            '            write_hex(out, (p + "ADDRESS").c_str(), e.address);\n'
            '            out << p << "WIDTH_BITS=" << e.width_bits << "\\n";\n'
            '            write_hex(out, (p + "VALUE").c_str(), e.value, 16);\n'
            '            write_hex(out, (p + "PC").c_str(), e.pc);\n'
            '            write_hex(out, (p + "LR").c_str(), e.lr);\n'
            '        }\n'
            '        out << "EARLY_SETUP_TRACE_POLICY=pc_0x00000B00_0x00000B7F_all_r0_r12_no_model_change\\n";\n'
            '        out << "EARLY_SETUP_TRACE_COUNT=" << result.early_setup_trace_count << "\\n";\n'
            '        for (std::size_t i = 0; i < result.early_setup_trace_count && i < result.early_setup_trace.size(); ++i) {\n'
            '            const auto &e = result.early_setup_trace[i];\n'
            '            std::ostringstream prefix;\n'
            '            prefix << "EARLY_SETUP_TRACE_" << std::setfill(\'0\') << std::setw(2) << i << "_";\n'
            '            const std::string p = prefix.str();\n'
            '            write_hex(out, (p + "PC").c_str(), e.pc);\n'
            '            write_hex(out, (p + "INSTRUCTION").c_str(), e.instruction);\n'
            '            write_hex(out, (p + "CPSR").c_str(), e.cpsr);\n'
            '            for (std::size_t reg = 0; reg < e.r.size(); ++reg) {\n'
            '                const std::string name = p + "R" + std::to_string(reg);\n'
            '                write_hex(out, name.c_str(), e.r[reg]);\n'
            '            }\n'
            '            write_hex(out, (p + "SP").c_str(), e.sp);\n'
            '            write_hex(out, (p + "LR").c_str(), e.lr);\n'
            '        }\n'
            '        write_hex(out, "KERN_DATA_ADDRESS", result.header.kern_data_address);\n'
        )
        if report_anchor not in text:
            raise SystemExit("MACHINE1-AY runner report anchor not found")
        text = text.replace(report_anchor, report_insert, 1)

        enable_anchor = '        result.flash_unlock_autoselect_enabled = true;\n        strict_bus bus(rom.data(), parsed.header.rom_size, parsed.header.rom_base, cold_reset_pc,\n'
        if enable_anchor not in text:
            raise SystemExit("MACHINE1-AY runner enable anchor not found")
        text = text.replace(
            enable_anchor,
            '        result.flash_unlock_autoselect_enabled = true;\n'
            '        result.candidate_ram_probe_enabled = true;\n'
            '        result.candidate_bootstrap_copy_enabled = true;\n'
            '        result.candidate_bootstrap_post_copy_mutation_enabled = true;\n'
            '        result.candidate_bootstrap_record_loop_mutation_enabled = true;\n'
            '        result.candidate_bootstrap_stack_enabled = true;\n'
            '        result.candidate_bootstrap_nested_stack_enabled = true;\n'
            '        result.candidate_post_probe_workspace_enabled = true;\n'
            '        result.low_vector_shadow_enabled = true;\n'
            '        strict_bus bus(rom.data(), parsed.header.rom_size, parsed.header.rom_base, cold_reset_pc,\n',
            1,
        )

        capture_anchor = '        result.flash_unlock_autoselect_count = bus.observed_flash_unlock_autoselect_count();\n        result.flash_unlock_stage_at_stop = bus.flash_unlock_stage();\n'
        if capture_anchor not in text:
            raise SystemExit("MACHINE1-AY runner capture anchor not found")
        text = text.replace(
            capture_anchor,
            capture_anchor
            + '        result.candidate_ram_probe_read_count = bus.candidate_ram_probe_read_count();\n'
            + '        result.candidate_ram_probe_write_count = bus.candidate_ram_probe_write_count();\n'
            + '        result.candidate_bootstrap_copy_read_count = bus.candidate_bootstrap_copy_read_count();\n'
            + '        result.candidate_bootstrap_copy_write_count = bus.candidate_bootstrap_copy_write_count();\n'
            + '        result.candidate_bootstrap_copy_initialized_bytes = static_cast<std::uint64_t>(bus.candidate_bootstrap_copy_initialized_bytes());\n'
            + '        result.candidate_bootstrap_post_copy_mutation_count = bus.candidate_bootstrap_post_copy_mutation_count();\n'
            + '        result.candidate_bootstrap_record_loop_mutation_count = bus.candidate_bootstrap_record_loop_mutation_count();\n'
            + '        result.candidate_bootstrap_stack_read_count = bus.candidate_bootstrap_stack_read_count();\n'
            + '        result.candidate_bootstrap_stack_write_count = bus.candidate_bootstrap_stack_write_count();\n'
            + '        result.candidate_bootstrap_stack_initialized_bytes = static_cast<std::uint64_t>(bus.candidate_bootstrap_stack_initialized_bytes());\n'
            + '        result.candidate_bootstrap_nested_stack_read_count = bus.candidate_bootstrap_nested_stack_read_count();\n'
            + '        result.candidate_bootstrap_nested_stack_write_count = bus.candidate_bootstrap_nested_stack_write_count();\n'
            + '        result.candidate_bootstrap_nested_stack_initialized_bytes = static_cast<std::uint64_t>(bus.candidate_bootstrap_nested_stack_initialized_bytes());\n'
            + '        result.candidate_post_probe_workspace_read_count = bus.candidate_post_probe_workspace_read_count();\n'
            + '        result.candidate_post_probe_workspace_write_count = bus.candidate_post_probe_workspace_write_count();\n'
            + '        result.low_vector_shadow_read_count = bus.low_vector_shadow_read_count();\n'
            + '        result.low_vector_shadow_write_count = bus.low_vector_shadow_write_count();\n'
            + '        result.candidate_sdram_write_trace_count = static_cast<std::uint32_t>(bus.ram_write_trace_count());\n'
            + '        result.candidate_sdram_write_trace = bus.ram_write_trace();\n',
            1,
        )

    elif destination.name == "RH29MachineProbeView.swift":
        text = text.replace("MACHINE1-O", "MACHINE1-AY")
        text = text.replace("RH29_MACHINE1_O.txt", "RH29_MACHINE1_AY.txt")
        text = text.replace(
            "Probe giữ nguyên model MACHINE1-M và không giả lập 0x0A000000. Bản O ghi 64 lệnh A32 cuối cùng cùng R0/R1/R2/R3/R4/R8/R9/R10/SP/LR để lần ngược nguồn gốc con trỏ 0x0A000000.",
            "AE giữ toàn bộ Y/Z/AA/AB/AC/AD và cho phép đúng vòng lặp 0x2228 duyệt 16 record: chỉ field +8 của record đã copy, stride 0x10, write tại PC 0x2220/LR 0x2244, và chỉ khi giá trị đúng phép biến đổi của firmware. Không mở thêm RAM; nút Chia sẻ báo cáo vẫn ngay dưới Chạy probe; không gán ngữ nghĩa remap cho 0x0C150004."
        )
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
    report.text = @"RH29_MACHINE1_AY\nSTOP_REASON=io_error\nDETAIL=emulator is not ready\n";

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
        report.text = @"RH29_MACHINE1_AY\nSTOP_REASON=io_error\nDETAIL=current device is not RH-29\n";
        return report;
    }

    const std::string rom_path = eka2l1::add_path(storage, "roms/rh-29/SYM.ROM");
    LOG_INFO(eka2l1::FRONTEND_CMDLINE, "[RH29_MACHINE1_AY] start budget={} rom={}", budget, rom_path);
    eka2l1::machine::rh29::probe_options options{};
    options.instruction_budget = budget;
    const auto result = eka2l1::machine::rh29::run_probe(rom_path, options);
    const std::string text = eka2l1::machine::rh29::format_report(result);
    report.text = [NSString stringWithUTF8String:text.c_str()];
    report.succeeded = (
        result.stop_reason != eka2l1::machine::rh29::probe_stop_reason::invalid_rom
        && result.stop_reason != eka2l1::machine::rh29::probe_stop_reason::io_error);
    LOG_INFO(eka2l1::FRONTEND_CMDLINE,
        "[RH29_MACHINE1_AY] stop succeeded={} executed={}",
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

                    Button {
                        showingRH29MachineProbe = true
                    } label: {
                        Label("RH-29 Machine Probe", systemImage: "waveform.path.ecg")
                    }
"""
    if settings_button not in content:
        raise SystemExit("ContentView More/Settings button anchor not found")
    content = content.replace(settings_button, settings_button + probe_button, 1)
content_path.write_text(content, encoding="utf-8")

print("MACHINE1-AY diagnostic sources + iOS probe integration staged")
