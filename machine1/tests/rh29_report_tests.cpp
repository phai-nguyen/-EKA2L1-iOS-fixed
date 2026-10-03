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
    result.flash_id_exit_enabled = true;
    result.flash_id_exit_count = 1;
    result.flash_autoselect_active_at_stop = false;
    result.flash_unlock1_enabled = true;
    result.flash_unlock1_count = 1;
    result.flash_unlock2_enabled = true;
    result.flash_unlock2_count = 1;
    result.flash_unlock_autoselect_enabled = true;
    result.flash_unlock_autoselect_count = 1;
    result.flash_unlock_stage_at_stop = 0;
    result.candidate_ram_bank_probe_anchor_read_count = 1;
    result.candidate_ram_bank_sparse_probe_original_read_count = 10;
    result.candidate_ram_bank_sparse_probe_test_write_count = 10;
    result.candidate_ram_bank_sparse_probe_anchor_verify_read_count = 10;
    result.candidate_ram_bank_sparse_probe_readback_count = 10;
    result.candidate_ram_bank_sparse_probe_restore_write_count = 10;
    result.candidate_bootstrap_local_frame_word_read_count = 1;
    result.candidate_bootstrap_local_frame_word_write_count = 1;
    result.candidate_bootstrap_relocation_read_count = 2;
    result.candidate_bootstrap_relocation_write_count = 66;
    result.candidate_bootstrap_relocation_initialized_bytes = 264;
    result.candidate_bootstrap_local_frame_tail_read_count = 4;
    result.candidate_bootstrap_local_frame_tail_write_count = 4;
    result.candidate_bootstrap_local_frame_tail_initialized_words = 4;
    result.candidate_bootstrap_relocated_stack_read_count = 8;
    result.candidate_bootstrap_relocated_stack_write_count = 8;
    result.candidate_bootstrap_relocated_stack_initialized_words = 8;
    result.candidate_bootstrap_relocated_local_word_read_count = 1;
    result.candidate_bootstrap_relocated_local_word_write_count = 1;
    result.candidate_bootstrap_relocated_rom_base_word_read_count = 1;
    result.candidate_bootstrap_relocated_rom_base_word_write_count = 1;
    result.candidate_bootstrap_relocated_helper_word_read_count = 1;
    result.candidate_bootstrap_relocated_helper_word_write_count = 1;
    result.candidate_bootstrap_callee_stack_read_count = 5;
    result.candidate_bootstrap_callee_stack_write_count = 5;
    result.candidate_bootstrap_callee_stack_initialized_words = 5;
    result.ram_bank_handler_dump_words_valid = 2;
    result.ram_bank_handler_dump_words[0] = 0xE2440B03u;
    result.ram_bank_handler_dump_words[1] = 0xE58D0000u;
    result.a32_trace_count = 2;
    result.a32_trace[0] = a32_trace_entry{
        0x00002660u, 0xE5901000u, 0x200000D1u,
        0x0A000000u, 0x0000241Cu, 0x600000D3u, 0x600000D1u,
        0x00000001u, 0x0A000000u, 0x00000001u, 0x00000008u,
        0x00000744u, 0x00000010u, 0x0000241Cu, 0x000024C4u
    };
    result.a32_trace[1] = a32_trace_entry{
        0x00002664u, 0xE89800F0u, 0x200000D1u,
        0x0A000000u, 0x0000241Cu, 0x600000D3u, 0x600000D1u,
        0x00000001u, 0x0A000000u, 0x00000001u, 0x00000008u,
        0x00000744u, 0x00000010u, 0x0000241Cu, 0x000024C4u
    };
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
    require_contains(text, "RH29_MACHINE1_O");
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
    require_contains(text, "FLASH_ID_EXIT_POLICY=amd_f0_exit_autoselect_to_read_array");
    require_contains(text, "FLASH_ID_EXIT_ADDRESS=0x02000000");
    require_contains(text, "FLASH_ID_EXIT_WIDTH_BITS=16");
    require_contains(text, "FLASH_ID_EXIT_VALUE=0x00F0");
    require_contains(text, "FLASH_ID_EXIT_ACCEPTED_COUNT=1");
    require_contains(text, "FLASH_AUTOSELECT_ACTIVE_AT_STOP=0");
    require_contains(text, "FLASH_UNLOCK1_POLICY=amd_x16_unlock_cycle1_observed");
    require_contains(text, "FLASH_UNLOCK1_ADDRESS=0x02000AAA");
    require_contains(text, "FLASH_UNLOCK1_WIDTH_BITS=16");
    require_contains(text, "FLASH_UNLOCK1_VALUE=0x00AA");
    require_contains(text, "FLASH_UNLOCK1_ACCEPTED_COUNT=1");
    require_contains(text, "FLASH_UNLOCK2_POLICY=amd_x16_unlock_cycle2_observed_stage1_only");
    require_contains(text, "FLASH_UNLOCK2_ADDRESS=0x02000554");
    require_contains(text, "FLASH_UNLOCK2_WIDTH_BITS=16");
    require_contains(text, "FLASH_UNLOCK2_VALUE=0x0055");
    require_contains(text, "FLASH_UNLOCK2_ACCEPTED_COUNT=1");
    require_contains(text, "FLASH_UNLOCK_AUTOSELECT_POLICY=amd_x16_unlock_stage2_command_0x90");
    require_contains(text, "FLASH_UNLOCK_AUTOSELECT_ADDRESS=0x02000AAA");
    require_contains(text, "FLASH_UNLOCK_AUTOSELECT_WIDTH_BITS=16");
    require_contains(text, "FLASH_UNLOCK_AUTOSELECT_VALUE=0x0090");
    require_contains(text, "FLASH_UNLOCK_AUTOSELECT_ACCEPTED_COUNT=1");
    require_contains(text, "FLASH_UNLOCK_STAGE_AT_STOP=0");
    require_contains(text, "KERN_DATA_ADDRESS=0x80001000");
    require_contains(text, "KERN_LIMIT=0x80002000");
}

static void test_report_contains_at_ram_probe_relocation_and_exact_callee_stack_evidence() {
    const auto text = format_report(sample_result());
    require_contains(text, "RAM_BANK_PROBE_ANCHOR_POLICY=AJ_device_exact_first_read_pc_0x1368_lr_0x1348_addr_0x0a001100_synthetic_zero_seed_read_only_no_range_map");
    require_contains(text, "RAM_BANK_PROBE_ANCHOR_ADDRESS=0x0A001100");
    require_contains(text, "RAM_BANK_PROBE_ANCHOR_WIDTH_BITS=32");
    require_contains(text, "RAM_BANK_PROBE_ANCHOR_GATE_PC=0x00001368");
    require_contains(text, "RAM_BANK_PROBE_ANCHOR_GATE_LR=0x00001348");
    require_contains(text, "RAM_BANK_PROBE_ANCHOR_OBSERVED_INSTRUCTION=0xE594C000");
    require_contains(text, "RAM_BANK_PROBE_ANCHOR_SEED=0x00000000");
    require_contains(text, "RAM_BANK_PROBE_ANCHOR_READ_COUNT=1");
    require_contains(text, "RAM_BANK_SPARSE_PROBE_POLICY=AK_raw_handler_10_power2_words_synthetic_independent_zero_seed_exact_callsites_no_range_map");
    require_contains(text, "RAM_BANK_SPARSE_PROBE_POINT_COUNT=10");
    require_contains(text, "RAM_BANK_SPARSE_PROBE_FIRST_OFFSET=0x00004000");
    require_contains(text, "RAM_BANK_SPARSE_PROBE_LIMIT=0x01000000");
    require_contains(text, "RAM_BANK_SPARSE_PROBE_TEST_VALUE=0xFFFFFFFF");
    require_contains(text, "RAM_BANK_SPARSE_PROBE_ORIGINAL_READ_COUNT=10");
    require_contains(text, "RAM_BANK_SPARSE_PROBE_TEST_WRITE_COUNT=10");
    require_contains(text, "RAM_BANK_SPARSE_PROBE_ANCHOR_VERIFY_READ_COUNT=10");
    require_contains(text, "RAM_BANK_SPARSE_PROBE_READBACK_COUNT=10");
    require_contains(text, "RAM_BANK_SPARSE_PROBE_RESTORE_WRITE_COUNT=10");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_WORD_POLICY=AL_device_exact_word_addr_0x0a000fbc_pc_0x12ac_lr_0x1270_value_0x09fff400_initialized_readback_only_no_range_widen");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_WORD_ADDRESS=0x0A000FBC");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_WORD_WIDTH_BITS=32");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_WORD_GATE_PC=0x000012AC");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_WORD_GATE_LR=0x00001270");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_WORD_OBSERVED_INSTRUCTION=0xE58D0000");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_WORD_OBSERVED_VALUE=0x09FFF400");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_WORD_READ_COUNT=1");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_WORD_WRITE_COUNT=1");
    require_contains(text, "BOOTSTRAP_RELOCATION_POLICY=AM_device_exact_0x108_copy_0x0a000000_to_0x09fff400_pc_0x2344_lr_0x12b8_source_verified_initialized_readback_only_no_range_widen");
    require_contains(text, "BOOTSTRAP_RELOCATION_SOURCE=0x0A000000");
    require_contains(text, "BOOTSTRAP_RELOCATION_BASE=0x09FFF400");
    require_contains(text, "BOOTSTRAP_RELOCATION_SIZE=264");
    require_contains(text, "BOOTSTRAP_RELOCATION_WRITE_WIDTH_BITS=32");
    require_contains(text, "BOOTSTRAP_RELOCATION_GATE_PC=0x00002344");
    require_contains(text, "BOOTSTRAP_RELOCATION_GATE_LR=0x000012B8");
    require_contains(text, "BOOTSTRAP_RELOCATION_OBSERVED_INSTRUCTION=0xE4803004");
    require_contains(text, "BOOTSTRAP_RELOCATION_READ_COUNT=2");
    require_contains(text, "BOOTSTRAP_RELOCATION_WRITE_COUNT=66");
    require_contains(text, "BOOTSTRAP_RELOCATION_INITIALIZED_BYTES=264");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_POLICY=AN_exact_four_word_tail_pc_0x12b8_0x12bc_0x12c0_0x12c4_lr_0x12b8_values_0x80_0_0_0x01170000_initialized_readback_only_no_range_widen");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_WORD_COUNT=4");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_WIDTH_BITS=32");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_GATE_LR=0x000012B8");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_00_ADDRESS=0x0A000FC0");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_00_PC=0x000012B8");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_00_INSTRUCTION=0xE58DA004");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_00_VALUE=0x00000080");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_03_ADDRESS=0x0A000FCC");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_03_PC=0x000012C4");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_03_INSTRUCTION=0xE58D7010");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_03_VALUE=0x01170000");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_READ_COUNT=4");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_WRITE_COUNT=4");
    require_contains(text, "BOOTSTRAP_LOCAL_FRAME_TAIL_INITIALIZED_WORDS=4");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_POLICY=AO_device_exact_stmdb_sp_32bytes_pc_0x0f18_lr_0x0f18_values_from_register_snapshot_initialized_readback_only_no_range_widen");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_TOP=0x09FFF3FC");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_BASE=0x09FFF3DC");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_WORD_COUNT=8");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_WIDTH_BITS=32");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_GATE_PC=0x00000F18");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_GATE_LR=0x00000F18");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_OBSERVED_INSTRUCTION=0xE92D47F0");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_00_ADDRESS=0x09FFF3DC");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_00_VALUE=0x0A000000");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_07_ADDRESS=0x09FFF3F8");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_07_VALUE=0x00000F18");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_READ_COUNT=8");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_WRITE_COUNT=8");
    require_contains(text, "BOOTSTRAP_RELOCATED_STACK_INITIALIZED_WORDS=8");
    require_contains(text, "BOOTSTRAP_RELOCATED_LOCAL_WORD_POLICY=AP_device_exact_sp_plus_0x24_addr_0x09fff3ac_pc_0x0f24_lr_0x0f18_value_0_initialized_readback_only_no_frame_widen");
    require_contains(text, "BOOTSTRAP_RELOCATED_LOCAL_WORD_ADDRESS=0x09FFF3AC");
    require_contains(text, "BOOTSTRAP_RELOCATED_LOCAL_WORD_WIDTH_BITS=32");
    require_contains(text, "BOOTSTRAP_RELOCATED_LOCAL_WORD_GATE_PC=0x00000F24");
    require_contains(text, "BOOTSTRAP_RELOCATED_LOCAL_WORD_GATE_LR=0x00000F18");
    require_contains(text, "BOOTSTRAP_RELOCATED_LOCAL_WORD_OBSERVED_INSTRUCTION=0xE58DC024");
    require_contains(text, "BOOTSTRAP_RELOCATED_LOCAL_WORD_OBSERVED_VALUE=0x00000000");
    require_contains(text, "BOOTSTRAP_RELOCATED_LOCAL_WORD_READ_COUNT=1");
    require_contains(text, "BOOTSTRAP_RELOCATED_LOCAL_WORD_WRITE_COUNT=1");
    require_contains(text, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_POLICY=AQ_device_exact_f30_read_rom_header_irombase_then_f34_store_sp_plus_0x18_addr_0x09fff3a0_pc_0x0f34_lr_0x0f18_value_0x50000000_initialized_readback_only_no_frame_widen");
    require_contains(text, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_ADDRESS=0x09FFF3A0");
    require_contains(text, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_WIDTH_BITS=32");
    require_contains(text, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_GATE_PC=0x00000F34");
    require_contains(text, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_GATE_LR=0x00000F18");
    require_contains(text, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_OBSERVED_INSTRUCTION=0xE58DC018");
    require_contains(text, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_OBSERVED_VALUE=0x50000000");
    require_contains(text, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_READ_COUNT=1");
    require_contains(text, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_WRITE_COUNT=1");
    require_contains(text, "BOOTSTRAP_RELOCATED_HELPER_WORD_POLICY=AR_device_exact_f3c_bl_helper_0x2af8_return_0x40000000_then_f40_store_sp_plus_0x14_addr_0x09fff39c_pc_lr_0x0f40_initialized_readback_only_no_frame_widen");
    require_contains(text, "BOOTSTRAP_RELOCATED_HELPER_WORD_ADDRESS=0x09FFF39C");
    require_contains(text, "BOOTSTRAP_RELOCATED_HELPER_WORD_WIDTH_BITS=32");
    require_contains(text, "BOOTSTRAP_RELOCATED_HELPER_WORD_GATE_PC=0x00000F40");
    require_contains(text, "BOOTSTRAP_RELOCATED_HELPER_WORD_GATE_LR=0x00000F40");
    require_contains(text, "BOOTSTRAP_RELOCATED_HELPER_WORD_OBSERVED_INSTRUCTION=0xE58D0014");
    require_contains(text, "BOOTSTRAP_RELOCATED_HELPER_WORD_OBSERVED_VALUE=0x40000000");
    require_contains(text, "BOOTSTRAP_RELOCATED_HELPER_WORD_READ_COUNT=1");
    require_contains(text, "BOOTSTRAP_RELOCATED_HELPER_WORD_WRITE_COUNT=1");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_POLICY=AS_device_exact_stmdb_sp_20bytes_pc_0x1580_lr_0x0f4c_values_from_register_snapshot_initialized_readback_only_no_range_widen");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_TOP=0x09FFF388");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_BASE=0x09FFF374");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_WORD_COUNT=5");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_WIDTH_BITS=32");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_GATE_PC=0x00001580");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_GATE_LR=0x00000F4C");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_OBSERVED_INSTRUCTION=0xE92D40F0");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_00_ADDRESS=0x09FFF374");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_00_VALUE=0x01170000");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_04_ADDRESS=0x09FFF384");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_04_VALUE=0x00000F4C");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_READ_COUNT=5");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_WRITE_COUNT=5");
    require_contains(text, "BOOTSTRAP_CALLEE_STACK_INITIALIZED_WORDS=5");
    require_contains(text, "RAM_BANK_HANDLER_DUMP_POLICY=raw_rom_words_0x000012a0_0x000013fc_context_no_semantic_label");
    require_contains(text, "RAM_BANK_HANDLER_DUMP_WORDS_VALID=2");
    require_contains(text, "RAM_BANK_HANDLER_DUMP_00_ADDRESS=0x000012A0");
    require_contains(text, "RAM_BANK_HANDLER_DUMP_00_VALUE=0xE2440B03");
    require_contains(text, "RAM_BANK_HANDLER_DUMP_01_ADDRESS=0x000012A4");
    require_contains(text, "RAM_BANK_HANDLER_DUMP_01_VALUE=0xE58D0000");
}

static void test_report_contains_budget_stop_and_registers() {
    const auto text = format_report(sample_result());
    require_contains(text, "INSTRUCTION_BUDGET=10000");
    require_contains(text, "EXECUTED_INSTRUCTIONS=37");
    require_contains(text, "STOP_REASON=unresolved_access");
    require_contains(text, "A32_TRACE_POLICY=last_64_with_pointer_registers_no_device_fabrication");
    require_contains(text, "A32_TRACE_COUNT=2");
    require_contains(text, "A32_TRACE_00_PC=0x00002660");
    require_contains(text, "A32_TRACE_00_INSTRUCTION=0xE5901000");
    require_contains(text, "A32_TRACE_01_PC=0x00002664");
    require_contains(text, "A32_TRACE_01_INSTRUCTION=0xE89800F0");
    require_contains(text, "A32_TRACE_01_CPSR=0x200000D1");
    require_contains(text, "A32_TRACE_01_R0=0x0A000000");
    require_contains(text, "A32_TRACE_01_R1=0x0000241C");
    require_contains(text, "A32_TRACE_01_R4=0x00000001");
    require_contains(text, "A32_TRACE_01_R8=0x0A000000");
    require_contains(text, "A32_TRACE_01_R9=0x00000001");
    require_contains(text, "A32_TRACE_01_R10=0x00000008");
    require_contains(text, "A32_TRACE_01_R11=0x00000744");
    require_contains(text, "A32_TRACE_01_R12=0x00000010");
    require_contains(text, "A32_TRACE_01_SP=0x0000241C");
    require_contains(text, "A32_TRACE_01_LR=0x000024C4");
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

static void test_machine1_o_defaults_to_1k() {
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
    test_report_contains_at_ram_probe_relocation_and_exact_callee_stack_evidence();
    test_report_contains_budget_stop_and_registers();
    test_report_contains_unresolved_access();
    test_report_contains_cpu_exception();
    test_report_contains_error_detail();
    test_cp15_classifier_blocks_all_a32_p15_coprocessor_classes();
    test_observed_arm920t_control_write_is_exact_allowlist();
    test_report_contains_cp15_emulation_evidence();
    test_report_contains_cp15_barrier();
    test_machine1_o_defaults_to_1k();
    test_arm_condition_passed_matches_a32_flags();
    std::cout << "rh29_report_tests: PASS\n";
    return 0;
}
