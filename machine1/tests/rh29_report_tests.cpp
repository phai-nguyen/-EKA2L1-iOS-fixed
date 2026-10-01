#include "rh29_machine_runner.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>

using namespace eka2l1::machine::rh29;

static void require_contains(const std::string &text, const std::string &needle) {
    if (text.find(needle) == std::string::npos) {
        std::cerr << "missing: " << needle << "\n" << text << std::endl;
        std::abort();
    }
}

static probe_result sample_result() {
    probe_result result{};
    result.header.restart_vector = 0x00000000;
    result.header.rom_base = 0x50000000;
    result.header.rom_size = 0x01170000;
    result.header.kern_data_address = 0x80001000;
    result.header.kern_limit = 0x80002000;
    result.instruction_budget = 10000;
    result.reset_pc = cold_reset_pc;
    result.synthetic_reset_alias = true;
    result.reset_alias_base = cold_reset_pc;
    result.reset_alias_size = result.header.rom_size;
    result.candidate_sdram_enabled = true;
    result.candidate_sdram_base_address = candidate_sdram_base;
    result.candidate_sdram_size_bytes = static_cast<std::uint32_t>(candidate_sdram_size);
    result.candidate_sdram_write_count = 3;
    result.candidate_sdram_initialized_bytes = 12;
    result.exact_observed_mmio_write_enabled = true;
    result.exact_observed_mmio_write_count = 1;
    result.exact_observed_flash_command_enabled = true;
    result.exact_observed_flash_command_count = 1;
    result.exact_observed_flash_id_entry_enabled = true;
    result.exact_observed_flash_id_entry_count = 1;
    result.amd_reference_manufacturer_id_enabled = true;
    result.amd_reference_manufacturer_read_count = 1;
    result.amd_reference_device_id_enabled = true;
    result.amd_reference_device_read_count = 1;
    result.executed_instructions = 37;
    result.stop_reason = probe_stop_reason::unresolved_access;
    for (std::size_t i = 0; i < result.registers.r.size(); ++i) {
        result.registers.r[i] = static_cast<std::uint32_t>(0x1000 + i);
    }
    result.registers.sp = 0x20001000;
    result.registers.lr = 0x50001234;
    result.registers.pc = 0x50005678;
    result.registers.cpsr = 0x000000D3;
    result.unresolved = unresolved_access{access_kind::data_write, 4, 0x40000010,
                                          0x50005678, 0x50001234, 0xAABBCCDD, 1,
                                          unresolved_cause::unmapped};
    return result;
}

static void test_report_contains_probe_identity_and_rom_header() {
    const auto text = format_report(sample_result());
    require_contains(text, "RH29_MACHINE1_I");
    require_contains(text, "ROM_BASE=0x50000000");
    require_contains(text, "ROM_SIZE=0x01170000");
    require_contains(text, "RESTART_VECTOR_WORD=0x00000000");
    require_contains(text, "RESET_PC=0x00000000");
    require_contains(text, "RESET_ALIAS_MODE=synthetic_rom_alias_hypothesis");
    require_contains(text, "RESET_ALIAS_BASE=0x00000000");
    require_contains(text, "RESET_ALIAS_SIZE=0x01170000");
    require_contains(text, "RESET_ALIAS_SOURCE_BASE=0x50000000");
    require_contains(text, "SDRAM_MODE=wd2_rh29_128mbit_candidate_write_tracked");
    require_contains(text, "SDRAM_BASE=0x08000000");
    require_contains(text, "SDRAM_SIZE=0x01000000");
    require_contains(text, "SDRAM_INIT_POLICY=write_initialized_only");
    require_contains(text, "SDRAM_WRITE_COUNT=3");
    require_contains(text, "SDRAM_INITIALIZED_BYTES=12");
    require_contains(text, "MMIO_PROBE_POLICY=exact_observed_write_only");
    require_contains(text, "MMIO_ALLOW_ADDRESS=0x0C150004");
    require_contains(text, "MMIO_ALLOW_WIDTH_BITS=16");
    require_contains(text, "MMIO_ALLOW_VALUE=0x0080");
    require_contains(text, "MMIO_ACCEPTED_WRITE_COUNT=1");
    require_contains(text, "FLASH_PROBE_POLICY=exact_observed_command_write_only");
    require_contains(text, "FLASH2_BASE=0x02000000");
    require_contains(text, "FLASH2_SIZE=0x00800000");
    require_contains(text, "FLASH_ALLOW_ADDRESS=0x02000000");
    require_contains(text, "FLASH_ALLOW_WIDTH_BITS=16");
    require_contains(text, "FLASH_ALLOW_VALUE=0x00FF");
    require_contains(text, "FLASH_ACCEPTED_COMMAND_COUNT=1");
    require_contains(text, "FLASH_ID_ENTRY_POLICY=exact_observed_write_only");
    require_contains(text, "FLASH_ID_ENTRY_ADDRESS=0x0200AAAA");
    require_contains(text, "FLASH_ID_ENTRY_WIDTH_BITS=16");
    require_contains(text, "FLASH_ID_ENTRY_VALUE=0x0090");
    require_contains(text, "FLASH_ID_ENTRY_ACCEPTED_COUNT=1");
    require_contains(text, "FLASH_ID_RESPONSE_POLICY=amd_rh29_reference_variant_manufacturer_only");
    require_contains(text, "FLASH_MANUFACTURER_ID_ADDRESS=0x02000000");
    require_contains(text, "FLASH_MANUFACTURER_ID_WIDTH_BITS=16");
    require_contains(text, "FLASH_MANUFACTURER_ID_VALUE=0x0001");
    require_contains(text, "FLASH_MANUFACTURER_ID_READ_COUNT=1");
    require_contains(text, "FLASH_DEVICE_ID_POLICY=amd_29bds064j_rh29_reference_word1_only");
    require_contains(text, "FLASH_DEVICE_ID_ADDRESS=0x02000002");
    require_contains(text, "FLASH_DEVICE_ID_WIDTH_BITS=16");
    require_contains(text, "FLASH_DEVICE_ID_VALUE=0x277E");
    require_contains(text, "FLASH_DEVICE_ID_READ_COUNT=1");
    require_contains(text, "KERN_DATA_ADDRESS=0x80001000");
    require_contains(text, "KERN_LIMIT=0x80002000");
}

static void test_report_contains_budget_stop_and_registers() {
    const auto text = format_report(sample_result());
    require_contains(text, "INSTRUCTION_BUDGET=10000");
    require_contains(text, "EXECUTED_INSTRUCTIONS=37");
    require_contains(text, "STOP_REASON=unresolved_access");
    require_contains(text, "PC=0x50005678");
    require_contains(text, "LR=0x50001234");
    require_contains(text, "SP=0x20001000");
    require_contains(text, "CPSR=0x000000D3");
    for (int i = 0; i <= 12; ++i) {
        char name[16];
        std::snprintf(name, sizeof(name), "R%d=0x", i);
        require_contains(text, name);
    }
}

static void test_report_contains_unresolved_access() {
    const auto text = format_report(sample_result());
    require_contains(text, "UNRESOLVED_KIND=data_write");
    require_contains(text, "UNRESOLVED_WIDTH_BITS=32");
    require_contains(text, "UNRESOLVED_ADDRESS=0x40000010");
    require_contains(text, "UNRESOLVED_PC=0x50005678");
    require_contains(text, "UNRESOLVED_LR=0x50001234");
    require_contains(text, "UNRESOLVED_VALUE=0x00000000AABBCCDD");
    require_contains(text, "UNRESOLVED_COUNT=1");
    require_contains(text, "UNRESOLVED_CAUSE=unmapped");
}

static void test_report_contains_cpu_exception() {
    auto result = sample_result();
    result.stop_reason = probe_stop_reason::cpu_exception;
    result.unresolved.reset();
    result.exception = cpu_exception_info{4, 0xDEADBEEF, false};
    const auto text = format_report(result);
    require_contains(text, "STOP_REASON=cpu_exception");
    require_contains(text, "EXCEPTION_TYPE=4");
    require_contains(text, "EXCEPTION_DATA=0xDEADBEEF");
    require_contains(text, "SYSTEM_CALL=0");
}

static void test_report_contains_error_detail() {
    probe_result result{};
    result.stop_reason = probe_stop_reason::invalid_rom;
    result.detail = "unexpected ROM base";
    const auto text = format_report(result);
    require_contains(text, "STOP_REASON=invalid_rom");
    require_contains(text, "DETAIL=unexpected ROM base");
}

static void test_cp15_classifier_blocks_all_a32_p15_coprocessor_classes() {
    // MRC p15, 0, r0, c1, c0, 0
    assert(is_arm_cp15_instruction(0xEE110F10u));
    // MCR p15, 0, r0, c1, c0, 0
    assert(is_arm_cp15_instruction(0xEE010F10u));
    // CDP p15 class (bit 4 clear).
    assert(is_arm_cp15_instruction(0xEE000F00u));
    // LDC/STC coprocessor class with p15 selected.
    assert(is_arm_cp15_instruction(0xEC900F00u));

    // Same MRC encoding but p10, a normal MOV, and SVC/SWI are not CP15.
    assert(!is_arm_cp15_instruction(0xEE110A10u));
    assert(!is_arm_cp15_instruction(0xE1A00000u));
    assert(!is_arm_cp15_instruction(0xEF000000u));
}

static void test_observed_arm920t_control_write_is_exact_allowlist() {
    assert(is_observed_arm920t_control_write(0xEE010F10u, 0x00001272u));
    assert(!is_observed_arm920t_control_write(0xEE010F10u, 0x00001273u));
    assert(!is_observed_arm920t_control_write(0xEE110F10u, 0x00001272u));
    assert(!is_observed_arm920t_control_write(0xEE010F50u, 0x00001272u));
}

static void test_report_contains_cp15_emulation_evidence() {
    auto result = sample_result();
    result.cp15_emulated_count = 1;
    result.cp15_emulated = cp15_emulation_info{0x00002DC8u, 0xEE010F10u, 0x00001272u};

    const auto text = format_report(result);
    require_contains(text, "CP15_EMULATED_COUNT=1");
    require_contains(text, "CP15_EMULATED_PC=0x00002DC8");
    require_contains(text, "CP15_EMULATED_INSTRUCTION=0xEE010F10");
    require_contains(text, "CP15_EMULATED_VALUE=0x00001272");
    require_contains(text, "CP15_EMULATION_POLICY=observed_arm920t_c1_write_0x1272_mmu_off");
}

static void test_report_contains_cp15_barrier() {
    auto result = sample_result();
    result.unresolved.reset();
    result.stop_reason = probe_stop_reason::cp15_access;
    result.cp15 = cp15_access_info{0x50004000u, 0xEE110F10u};

    const auto text = format_report(result);
    require_contains(text, "STOP_REASON=cp15_access");
    require_contains(text, "CP15_PC=0x50004000");
    require_contains(text, "CP15_INSTRUCTION=0xEE110F10");
}

static void test_machine1_i_defaults_to_1k() {
    probe_options options{};
    assert(options.instruction_budget == 1000);
}

static void test_arm_condition_passed_matches_a32_flags() {
    // EQ: execute only when Z is set.
    assert(!arm_condition_passed(0x0E110F10u, 0x00000000u));
    assert(arm_condition_passed(0x0E110F10u, 1u << 30));

    // NE: execute only when Z is clear.
    assert(arm_condition_passed(0x1E110F10u, 0x00000000u));
    assert(!arm_condition_passed(0x1E110F10u, 1u << 30));

    // GE: N must equal V.
    assert(arm_condition_passed(0xAE110F10u, 0x00000000u));
    assert(arm_condition_passed(0xAE110F10u, (1u << 31) | (1u << 28)));
    assert(!arm_condition_passed(0xAE110F10u, 1u << 31));

    // AL always executes; cond=0xF is treated conservatively as unconditional.
    assert(arm_condition_passed(0xEE110F10u, 0x00000000u));
    assert(arm_condition_passed(0xFE110F10u, 0x00000000u));
}

int main() {
    test_report_contains_probe_identity_and_rom_header();
    test_report_contains_budget_stop_and_registers();
    test_report_contains_unresolved_access();
    test_report_contains_cpu_exception();
    test_report_contains_error_detail();
    test_cp15_classifier_blocks_all_a32_p15_coprocessor_classes();
    test_observed_arm920t_control_write_is_exact_allowlist();
    test_report_contains_cp15_emulation_evidence();
    test_report_contains_cp15_barrier();
    test_machine1_i_defaults_to_1k();
    test_arm_condition_passed_matches_a32_flags();
    std::cout << "rh29_report_tests: PASS\n";
    return 0;
}
