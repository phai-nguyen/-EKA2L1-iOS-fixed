#include "rh29_machine_runner.h"

#include <iomanip>
#include <sstream>

#ifndef RH29_MACHINE1_REPORT_ONLY
#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <common/types.h>
#include <cpu/arm_factory.h>
#include <cpu/arm_interface.h>
#include <cpu/dyncom/arm_dyncom.h>
#endif

namespace eka2l1::machine::rh29 {
    namespace {
        const char *stop_reason_name(const probe_stop_reason reason) {
            switch (reason) {
            case probe_stop_reason::budget_exhausted: return "budget_exhausted";
            case probe_stop_reason::unresolved_access: return "unresolved_access";
            case probe_stop_reason::cpu_exception: return "cpu_exception";
            case probe_stop_reason::cp15_access: return "cp15_access";
            case probe_stop_reason::invalid_rom: return "invalid_rom";
            case probe_stop_reason::io_error: return "io_error";
            }
            return "unknown";
        }

        const char *access_kind_name(const access_kind kind) {
            switch (kind) {
            case access_kind::code_read: return "code_read";
            case access_kind::data_read: return "data_read";
            case access_kind::data_write: return "data_write";
            }
            return "unknown";
        }

        const char *low_page_trace_kind_name(const low_page_trace_kind kind) {
            switch (kind) {
            case low_page_trace_kind::code_read: return "code_read";
            case low_page_trace_kind::data_read: return "data_read";
            case low_page_trace_kind::data_write: return "data_write";
            }
            return "unknown";
        }

        const char *unresolved_cause_name(const unresolved_cause cause) {
            switch (cause) {
            case unresolved_cause::unmapped: return "unmapped";
            case unresolved_cause::rom_write: return "rom_write";
            case unresolved_cause::ram_uninitialized: return "ram_uninitialized";
            }
            return "unknown";
        }

        template <typename T>
        void write_hex(std::ostringstream &out, const char *name, T value, int width = 8) {
            out << name << "=0x" << std::uppercase << std::hex << std::setfill('0')
                << std::setw(width) << static_cast<std::uint64_t>(value)
                << std::dec << "\n";
        }
    }

    bool is_arm_cp15_instruction(const std::uint32_t instruction) {
        // A32 coprocessor encodings carry the coprocessor number in bits 11:8.
        // Stop on every p15 instruction class before Dyncom can service it with
        // its ARM11/MPCore CP15 model. That model is not evidence for RH-29.
        if ((instruction & 0x00000F00u) != 0x00000F00u) {
            return false;
        }

        // LDC/STC and MCRR/MRRC families: bits 27:25 == 110.
        if ((instruction & 0x0E000000u) == 0x0C000000u) {
            return true;
        }

        // CDP/MCR/MRC families: bits 27:24 == 1110.
        return (instruction & 0x0F000000u) == 0x0E000000u;
    }

    bool is_observed_arm920t_control_write(const std::uint32_t instruction,
                                            const std::uint32_t value) {
        // DEVICE evidence from MACHINE1-B:
        //   PC=0x00002DC8, instruction=0xEE010F10, R0=0x00001272.
        // Decode: MCR p15,0,r0,c1,c0,0 (ARM920T CP15 control register write).
        // Keep this deliberately exact. 0x1272 leaves MMU bit M (bit 0) clear.
        return instruction == 0xEE010F10u && value == 0x00001272u;
    }

    bool arm_condition_passed(const std::uint32_t instruction, const std::uint32_t cpsr) {
        const std::uint32_t cond = instruction >> 28;
        const bool n = (cpsr & (1u << 31)) != 0;
        const bool z = (cpsr & (1u << 30)) != 0;
        const bool carry = (cpsr & (1u << 29)) != 0;
        const bool v = (cpsr & (1u << 28)) != 0;

        switch (cond) {
        case 0x0: return z;                       // EQ
        case 0x1: return !z;                      // NE
        case 0x2: return carry;                   // CS/HS
        case 0x3: return !carry;                  // CC/LO
        case 0x4: return n;                       // MI
        case 0x5: return !n;                      // PL
        case 0x6: return v;                       // VS
        case 0x7: return !v;                      // VC
        case 0x8: return carry && !z;             // HI
        case 0x9: return !carry || z;             // LS
        case 0xA: return n == v;                  // GE
        case 0xB: return n != v;                  // LT
        case 0xC: return !z && (n == v);          // GT
        case 0xD: return z || (n != v);           // LE
        case 0xE: return true;                    // AL
        default: return true;                     // Unconditional extension space
        }
    }

    std::string format_report(const probe_result &result) {
        std::ostringstream out;
        out << "RH29_MACHINE1_O\n";
        write_hex(out, "ROM_BASE", result.header.rom_base);
        write_hex(out, "ROM_SIZE", result.header.rom_size);
        write_hex(out, "RESTART_VECTOR_WORD", result.header.restart_vector);
        write_hex(out, "RESET_PC", result.reset_pc);
        out << "RESET_ALIAS_MODE=" << (result.synthetic_reset_alias ? "synthetic_rom_alias_hypothesis" : "none") << "\n";
        write_hex(out, "RESET_ALIAS_BASE", result.reset_alias_base);
        write_hex(out, "RESET_ALIAS_SIZE", result.reset_alias_size);
        write_hex(out, "RESET_ALIAS_SOURCE_BASE", result.header.rom_base);
        out << "SDRAM_MODE=" << (result.candidate_sdram_enabled ? "wd2_rh29_128mbit_candidate_write_tracked" : "disabled") << "\n";
        write_hex(out, "SDRAM_BASE", result.candidate_sdram_base_address);
        write_hex(out, "SDRAM_SIZE", result.candidate_sdram_size_bytes);
        out << "SDRAM_INIT_POLICY=write_initialized_only\n";
        out << "SDRAM_WRITE_COUNT=" << result.candidate_sdram_write_count << "\n";
        out << "SDRAM_INITIALIZED_BYTES=" << result.candidate_sdram_initialized_bytes << "\n";
        out << "MMIO_PROBE_POLICY=" << (result.exact_observed_mmio_write_enabled
            ? "exact_observed_write_only" : "disabled") << "\n";
        write_hex(out, "MMIO_ALLOW_ADDRESS", observed_mmio_write_address);
        out << "MMIO_ALLOW_WIDTH_BITS=" << (observed_mmio_write_width * 8) << "\n";
        write_hex(out, "MMIO_ALLOW_VALUE", observed_mmio_write_value, 4);
        out << "MMIO_ACCEPTED_WRITE_COUNT=" << result.exact_observed_mmio_write_count << "\n";
        out << "FLASH_PROBE_POLICY=" << (result.exact_observed_flash_command_enabled
            ? "exact_observed_command_write_only" : "disabled") << "\n";
        write_hex(out, "FLASH2_BASE", second_flash_base);
        write_hex(out, "FLASH2_SIZE", static_cast<std::uint32_t>(second_flash_size));
        write_hex(out, "FLASH_ALLOW_ADDRESS", observed_flash_command_address);
        out << "FLASH_ALLOW_WIDTH_BITS=" << (observed_flash_command_width * 8) << "\n";
        write_hex(out, "FLASH_ALLOW_VALUE", observed_flash_command_value, 4);
        out << "FLASH_ACCEPTED_COMMAND_COUNT=" << result.exact_observed_flash_command_count << "\n";
        out << "FLASH_ID_ENTRY_POLICY=" << (result.exact_observed_flash_id_entry_enabled
            ? "exact_observed_write_only" : "disabled") << "\n";
        write_hex(out, "FLASH_ID_ENTRY_ADDRESS", observed_flash_id_entry_address);
        out << "FLASH_ID_ENTRY_WIDTH_BITS=" << (observed_flash_id_entry_width * 8) << "\n";
        write_hex(out, "FLASH_ID_ENTRY_VALUE", observed_flash_id_entry_value, 4);
        out << "FLASH_ID_ENTRY_ACCEPTED_COUNT=" << result.exact_observed_flash_id_entry_count << "\n";
        out << "FLASH_ID_RESPONSE_POLICY=" << (result.amd_reference_manufacturer_id_enabled
            ? "amd_rh29_reference_variant_manufacturer_only" : "disabled") << "\n";
        write_hex(out, "FLASH_MANUFACTURER_ID_ADDRESS", amd_reference_manufacturer_id_address);
        out << "FLASH_MANUFACTURER_ID_WIDTH_BITS=" << (amd_reference_manufacturer_id_width * 8) << "\n";
        write_hex(out, "FLASH_MANUFACTURER_ID_VALUE", amd_reference_manufacturer_id, 4);
        out << "FLASH_MANUFACTURER_ID_READ_COUNT=" << result.amd_reference_manufacturer_read_count << "\n";
        out << "FLASH_DEVICE_ID_POLICY=" << (result.amd_reference_device_id_enabled
            ? "amd_29bds064j_rh29_reference_word1_only" : "disabled") << "\n";
        write_hex(out, "FLASH_DEVICE_ID_ADDRESS", amd_reference_device_id_address);
        out << "FLASH_DEVICE_ID_WIDTH_BITS=" << (amd_reference_device_id_width * 8) << "\n";
        write_hex(out, "FLASH_DEVICE_ID_VALUE", amd_reference_device_id, 4);
        out << "FLASH_DEVICE_ID_READ_COUNT=" << result.amd_reference_device_read_count << "\n";
        out << "FLASH_ID_EXIT_POLICY=" << (result.flash_id_exit_enabled
            ? "amd_f0_exit_autoselect_to_read_array" : "disabled") << "\n";
        write_hex(out, "FLASH_ID_EXIT_ADDRESS", observed_flash_id_exit_address);
        out << "FLASH_ID_EXIT_WIDTH_BITS=" << (observed_flash_id_exit_width * 8) << "\n";
        write_hex(out, "FLASH_ID_EXIT_VALUE", observed_flash_id_exit_value, 4);
        out << "FLASH_ID_EXIT_ACCEPTED_COUNT=" << result.flash_id_exit_count << "\n";
        out << "FLASH_AUTOSELECT_ACTIVE_AT_STOP=" << (result.flash_autoselect_active_at_stop ? 1 : 0) << "\n";
        out << "FLASH_UNLOCK1_POLICY=" << (result.flash_unlock1_enabled
            ? "amd_x16_unlock_cycle1_observed" : "disabled") << "\n";
        write_hex(out, "FLASH_UNLOCK1_ADDRESS", observed_flash_unlock1_address);
        out << "FLASH_UNLOCK1_WIDTH_BITS=" << (observed_flash_unlock1_width * 8) << "\n";
        write_hex(out, "FLASH_UNLOCK1_VALUE", observed_flash_unlock1_value, 4);
        out << "FLASH_UNLOCK1_ACCEPTED_COUNT=" << result.flash_unlock1_count << "\n";
        out << "FLASH_UNLOCK2_POLICY=" << (result.flash_unlock2_enabled
            ? "amd_x16_unlock_cycle2_observed_stage1_only" : "disabled") << "\n";
        write_hex(out, "FLASH_UNLOCK2_ADDRESS", observed_flash_unlock2_address);
        out << "FLASH_UNLOCK2_WIDTH_BITS=" << (observed_flash_unlock2_width * 8) << "\n";
        write_hex(out, "FLASH_UNLOCK2_VALUE", observed_flash_unlock2_value, 4);
        out << "FLASH_UNLOCK2_ACCEPTED_COUNT=" << result.flash_unlock2_count << "\n";
        out << "FLASH_UNLOCK_AUTOSELECT_POLICY=" << (result.flash_unlock_autoselect_enabled
            ? "amd_x16_unlock_stage2_command_0x90" : "disabled") << "\n";
        write_hex(out, "FLASH_UNLOCK_AUTOSELECT_ADDRESS", observed_flash_unlock_autoselect_address);
        out << "FLASH_UNLOCK_AUTOSELECT_WIDTH_BITS=" << (observed_flash_unlock_autoselect_width * 8) << "\n";
        write_hex(out, "FLASH_UNLOCK_AUTOSELECT_VALUE", observed_flash_unlock_autoselect_value, 4);
        out << "FLASH_UNLOCK_AUTOSELECT_ACCEPTED_COUNT=" << result.flash_unlock_autoselect_count << "\n";
        out << "FLASH_UNLOCK_STAGE_AT_STOP=" << result.flash_unlock_stage_at_stop << "\n";
        write_hex(out, "KERN_DATA_ADDRESS", result.header.kern_data_address);
        write_hex(out, "KERN_LIMIT", result.header.kern_limit);
        out << "INSTRUCTION_BUDGET=" << result.instruction_budget << "\n";
        out << "EXECUTED_INSTRUCTIONS=" << result.executed_instructions << "\n";
        out << "STOP_REASON=" << stop_reason_name(result.stop_reason) << "\n";
        out << "A32_TRACE_POLICY=last_64_with_pointer_registers_no_device_fabrication\n";
        out << "A32_TRACE_COUNT=" << result.a32_trace_count << "\n";
        for (std::size_t i = 0; i < result.a32_trace_count && i < result.a32_trace.size(); ++i) {
            const auto &entry = result.a32_trace[i];
            std::ostringstream prefix;
            prefix << "A32_TRACE_" << std::setfill('0') << std::setw(2) << i << "_";
            const std::string p = prefix.str();
            write_hex(out, (p + "PC").c_str(), entry.pc);
            write_hex(out, (p + "INSTRUCTION").c_str(), entry.instruction);
            write_hex(out, (p + "CPSR").c_str(), entry.cpsr);
            write_hex(out, (p + "R0").c_str(), entry.r0);
            write_hex(out, (p + "R1").c_str(), entry.r1);
            write_hex(out, (p + "R2").c_str(), entry.r2);
            write_hex(out, (p + "R3").c_str(), entry.r3);
            write_hex(out, (p + "R4").c_str(), entry.r4);
            write_hex(out, (p + "R8").c_str(), entry.r8);
            write_hex(out, (p + "R9").c_str(), entry.r9);
            write_hex(out, (p + "R10").c_str(), entry.r10);
            write_hex(out, (p + "R11").c_str(), entry.r11);
            write_hex(out, (p + "R12").c_str(), entry.r12);
            write_hex(out, (p + "SP").c_str(), entry.sp);
            write_hex(out, (p + "LR").c_str(), entry.lr);
        }

        out << "MACHINE1_W_DIAG_POLICY=no_memory_response_change_no_remap_assumption\n";
        out << "CALLSITE_TRACE_POLICY=pc_0x00000300_0x0000037F_all_r0_r12\n";
        out << "CALLSITE_TRACE_COUNT=" << result.callsite_trace_count << "\n";
        for (std::size_t i = 0; i < result.callsite_trace_count && i < result.callsite_trace.size(); ++i) {
            const auto &e = result.callsite_trace[i];
            std::ostringstream prefix;
            prefix << "CALLSITE_TRACE_" << std::setfill('0') << std::setw(2) << i << "_";
            const std::string p = prefix.str();
            write_hex(out, (p + "PC").c_str(), e.pc);
            write_hex(out, (p + "INSTRUCTION").c_str(), e.instruction);
            write_hex(out, (p + "CPSR").c_str(), e.cpsr);
            for (std::size_t reg = 0; reg < e.r.size(); ++reg) {
                const std::string name = p + "R" + std::to_string(reg);
                write_hex(out, name.c_str(), e.r[reg]);
            }
            write_hex(out, (p + "SP").c_str(), e.sp);
            write_hex(out, (p + "LR").c_str(), e.lr);
        }
        out << "SETUP_LITERAL_POOL_POLICY=raw_rom_words_0x00000B90_0x00000BC0_no_semantic_label\n";
        out << "SETUP_LITERAL_POOL_WORDS_VALID=" << result.setup_literal_pool_words_valid << "\n";
        for (std::size_t i = 0; i < result.setup_literal_pool_words_valid && i < result.setup_literal_pool_words.size(); ++i) {
            std::ostringstream p;
            p << "SETUP_LITERAL_POOL_" << std::setfill('0') << std::setw(2) << i << "_";
            const std::string prefix = p.str();
            write_hex(out, (prefix + "ADDRESS").c_str(), setup_literal_pool_begin + static_cast<std::uint32_t>(i * 4u));
            write_hex(out, (prefix + "VALUE").c_str(), result.setup_literal_pool_words[i]);
        }
        out << "RAM_BANK_PROBE_ANCHOR_POLICY=AJ_device_exact_first_read_pc_0x1368_lr_0x1348_addr_0x0a001100_synthetic_zero_seed_read_only_no_range_map\n";
        write_hex(out, "RAM_BANK_PROBE_ANCHOR_ADDRESS", candidate_ram_bank_probe_anchor_address);
        out << "RAM_BANK_PROBE_ANCHOR_WIDTH_BITS=" << (candidate_ram_bank_probe_anchor_width * 8u) << "\n";
        write_hex(out, "RAM_BANK_PROBE_ANCHOR_GATE_PC", candidate_ram_bank_probe_anchor_read_pc);
        write_hex(out, "RAM_BANK_PROBE_ANCHOR_GATE_LR", candidate_ram_bank_probe_anchor_read_lr);
        write_hex(out, "RAM_BANK_PROBE_ANCHOR_OBSERVED_INSTRUCTION", candidate_ram_bank_probe_anchor_read_instruction);
        write_hex(out, "RAM_BANK_PROBE_ANCHOR_SEED", candidate_ram_bank_probe_anchor_seed);
        out << "RAM_BANK_PROBE_ANCHOR_READ_COUNT=" << result.candidate_ram_bank_probe_anchor_read_count << "\n";
        out << "RAM_BANK_SPARSE_PROBE_POLICY=AK_raw_handler_10_power2_words_synthetic_independent_zero_seed_exact_callsites_no_range_map\n";
        out << "RAM_BANK_SPARSE_PROBE_POINT_COUNT=" << candidate_ram_bank_sparse_probe_point_count << "\n";
        write_hex(out, "RAM_BANK_SPARSE_PROBE_FIRST_OFFSET", candidate_ram_bank_sparse_probe_first_offset);
        write_hex(out, "RAM_BANK_SPARSE_PROBE_LIMIT", candidate_ram_bank_sparse_probe_limit);
        write_hex(out, "RAM_BANK_SPARSE_PROBE_TEST_VALUE", candidate_ram_bank_sparse_probe_test_value);
        out << "RAM_BANK_SPARSE_PROBE_ORIGINAL_READ_COUNT=" << result.candidate_ram_bank_sparse_probe_original_read_count << "\n";
        out << "RAM_BANK_SPARSE_PROBE_TEST_WRITE_COUNT=" << result.candidate_ram_bank_sparse_probe_test_write_count << "\n";
        out << "RAM_BANK_SPARSE_PROBE_ANCHOR_VERIFY_READ_COUNT=" << result.candidate_ram_bank_sparse_probe_anchor_verify_read_count << "\n";
        out << "RAM_BANK_SPARSE_PROBE_READBACK_COUNT=" << result.candidate_ram_bank_sparse_probe_readback_count << "\n";
        out << "RAM_BANK_SPARSE_PROBE_RESTORE_WRITE_COUNT=" << result.candidate_ram_bank_sparse_probe_restore_write_count << "\n";
        out << "BOOTSTRAP_LOCAL_FRAME_WORD_POLICY=AL_device_exact_word_addr_0x0a000fbc_pc_0x12ac_lr_0x1270_value_0x09fff400_initialized_readback_only_no_range_widen\n";
        write_hex(out, "BOOTSTRAP_LOCAL_FRAME_WORD_ADDRESS", candidate_bootstrap_local_frame_word_address);
        out << "BOOTSTRAP_LOCAL_FRAME_WORD_WIDTH_BITS=" << (candidate_bootstrap_local_frame_word_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_LOCAL_FRAME_WORD_GATE_PC", candidate_bootstrap_local_frame_word_write_pc);
        write_hex(out, "BOOTSTRAP_LOCAL_FRAME_WORD_GATE_LR", candidate_bootstrap_local_frame_word_write_lr);
        write_hex(out, "BOOTSTRAP_LOCAL_FRAME_WORD_OBSERVED_INSTRUCTION", candidate_bootstrap_local_frame_word_instruction);
        write_hex(out, "BOOTSTRAP_LOCAL_FRAME_WORD_OBSERVED_VALUE", candidate_bootstrap_local_frame_word_value);
        out << "BOOTSTRAP_LOCAL_FRAME_WORD_READ_COUNT=" << result.candidate_bootstrap_local_frame_word_read_count << "\n";
        out << "BOOTSTRAP_LOCAL_FRAME_WORD_WRITE_COUNT=" << result.candidate_bootstrap_local_frame_word_write_count << "\n";
        out << "BOOTSTRAP_RELOCATION_POLICY=AM_device_exact_0x108_copy_0x0a000000_to_0x09fff400_pc_0x2344_lr_0x12b8_source_verified_initialized_readback_only_no_range_widen\n";
        write_hex(out, "BOOTSTRAP_RELOCATION_SOURCE", candidate_bootstrap_relocation_source);
        write_hex(out, "BOOTSTRAP_RELOCATION_BASE", candidate_bootstrap_relocation_base);
        out << "BOOTSTRAP_RELOCATION_SIZE=" << candidate_bootstrap_relocation_size << "\n";
        out << "BOOTSTRAP_RELOCATION_WRITE_WIDTH_BITS=" << (candidate_bootstrap_relocation_write_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_RELOCATION_GATE_PC", candidate_bootstrap_relocation_pc);
        write_hex(out, "BOOTSTRAP_RELOCATION_GATE_LR", candidate_bootstrap_relocation_lr);
        write_hex(out, "BOOTSTRAP_RELOCATION_OBSERVED_INSTRUCTION", candidate_bootstrap_relocation_instruction);
        out << "BOOTSTRAP_RELOCATION_READ_COUNT=" << result.candidate_bootstrap_relocation_read_count << "\n";
        out << "BOOTSTRAP_RELOCATION_WRITE_COUNT=" << result.candidate_bootstrap_relocation_write_count << "\n";
        out << "BOOTSTRAP_RELOCATION_INITIALIZED_BYTES=" << result.candidate_bootstrap_relocation_initialized_bytes << "\n";
        out << "BOOTSTRAP_LOCAL_FRAME_TAIL_POLICY=AN_exact_four_word_tail_pc_0x12b8_0x12bc_0x12c0_0x12c4_lr_0x12b8_values_0x80_0_0_0x01170000_initialized_readback_only_no_range_widen\n";
        out << "BOOTSTRAP_LOCAL_FRAME_TAIL_WORD_COUNT=" << candidate_bootstrap_local_frame_tail_word_count << "\n";
        out << "BOOTSTRAP_LOCAL_FRAME_TAIL_WIDTH_BITS=" << (candidate_bootstrap_local_frame_tail_word_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_LOCAL_FRAME_TAIL_GATE_LR", candidate_bootstrap_local_frame_tail_lr);
        for (std::size_t i = 0; i < candidate_bootstrap_local_frame_tail_word_count; ++i) {
            std::ostringstream p;
            p << "BOOTSTRAP_LOCAL_FRAME_TAIL_" << std::setfill('0') << std::setw(2) << i << "_";
            const std::string prefix = p.str();
            write_hex(out, (prefix + "ADDRESS").c_str(), candidate_bootstrap_local_frame_tail_addresses[i]);
            write_hex(out, (prefix + "PC").c_str(), candidate_bootstrap_local_frame_tail_pcs[i]);
            write_hex(out, (prefix + "INSTRUCTION").c_str(), candidate_bootstrap_local_frame_tail_instructions[i]);
            write_hex(out, (prefix + "VALUE").c_str(), candidate_bootstrap_local_frame_tail_values[i]);
        }
        out << "BOOTSTRAP_LOCAL_FRAME_TAIL_READ_COUNT=" << result.candidate_bootstrap_local_frame_tail_read_count << "\n";
        out << "BOOTSTRAP_LOCAL_FRAME_TAIL_WRITE_COUNT=" << result.candidate_bootstrap_local_frame_tail_write_count << "\n";
        out << "BOOTSTRAP_LOCAL_FRAME_TAIL_INITIALIZED_WORDS=" << result.candidate_bootstrap_local_frame_tail_initialized_words << "\n";
        out << "BOOTSTRAP_RELOCATED_STACK_POLICY=AO_device_exact_stmdb_sp_32bytes_pc_0x0f18_lr_0x0f18_values_from_register_snapshot_initialized_readback_only_no_range_widen\n";
        write_hex(out, "BOOTSTRAP_RELOCATED_STACK_TOP", candidate_bootstrap_relocated_stack_top);
        write_hex(out, "BOOTSTRAP_RELOCATED_STACK_BASE", candidate_bootstrap_relocated_stack_base);
        out << "BOOTSTRAP_RELOCATED_STACK_WORD_COUNT=" << candidate_bootstrap_relocated_stack_word_count << "\n";
        out << "BOOTSTRAP_RELOCATED_STACK_WIDTH_BITS=" << (candidate_bootstrap_relocated_stack_word_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_RELOCATED_STACK_GATE_PC", candidate_bootstrap_relocated_stack_pc);
        write_hex(out, "BOOTSTRAP_RELOCATED_STACK_GATE_LR", candidate_bootstrap_relocated_stack_lr);
        write_hex(out, "BOOTSTRAP_RELOCATED_STACK_OBSERVED_INSTRUCTION", candidate_bootstrap_relocated_stack_instruction);
        for (std::size_t i = 0; i < candidate_bootstrap_relocated_stack_word_count; ++i) {
            std::ostringstream p;
            p << "BOOTSTRAP_RELOCATED_STACK_" << std::setfill('0') << std::setw(2) << i << "_";
            const std::string prefix = p.str();
            write_hex(out, (prefix + "ADDRESS").c_str(), candidate_bootstrap_relocated_stack_addresses[i]);
            write_hex(out, (prefix + "VALUE").c_str(), candidate_bootstrap_relocated_stack_values[i]);
        }
        out << "BOOTSTRAP_RELOCATED_STACK_READ_COUNT=" << result.candidate_bootstrap_relocated_stack_read_count << "\n";
        out << "BOOTSTRAP_RELOCATED_STACK_WRITE_COUNT=" << result.candidate_bootstrap_relocated_stack_write_count << "\n";
        out << "BOOTSTRAP_RELOCATED_STACK_INITIALIZED_WORDS=" << result.candidate_bootstrap_relocated_stack_initialized_words << "\n";
        out << "BOOTSTRAP_RELOCATED_LOCAL_WORD_POLICY=AP_device_exact_sp_plus_0x24_addr_0x09fff3ac_pc_0x0f24_lr_0x0f18_value_0_initialized_readback_only_no_frame_widen\n";
        write_hex(out, "BOOTSTRAP_RELOCATED_LOCAL_WORD_ADDRESS", candidate_bootstrap_relocated_local_word_address);
        out << "BOOTSTRAP_RELOCATED_LOCAL_WORD_WIDTH_BITS=" << (candidate_bootstrap_relocated_local_word_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_RELOCATED_LOCAL_WORD_GATE_PC", candidate_bootstrap_relocated_local_word_pc);
        write_hex(out, "BOOTSTRAP_RELOCATED_LOCAL_WORD_GATE_LR", candidate_bootstrap_relocated_local_word_lr);
        write_hex(out, "BOOTSTRAP_RELOCATED_LOCAL_WORD_OBSERVED_INSTRUCTION", candidate_bootstrap_relocated_local_word_instruction);
        write_hex(out, "BOOTSTRAP_RELOCATED_LOCAL_WORD_OBSERVED_VALUE", candidate_bootstrap_relocated_local_word_value);
        out << "BOOTSTRAP_RELOCATED_LOCAL_WORD_READ_COUNT=" << result.candidate_bootstrap_relocated_local_word_read_count << "\n";
        out << "BOOTSTRAP_RELOCATED_LOCAL_WORD_WRITE_COUNT=" << result.candidate_bootstrap_relocated_local_word_write_count << "\n";
        out << "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_POLICY=AQ_device_exact_f30_read_rom_header_irombase_then_f34_store_sp_plus_0x18_addr_0x09fff3a0_pc_0x0f34_lr_0x0f18_value_0x50000000_initialized_readback_only_no_frame_widen\n";
        write_hex(out, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_ADDRESS", candidate_bootstrap_relocated_rom_base_word_address);
        out << "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_WIDTH_BITS=" << (candidate_bootstrap_relocated_rom_base_word_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_GATE_PC", candidate_bootstrap_relocated_rom_base_word_pc);
        write_hex(out, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_GATE_LR", candidate_bootstrap_relocated_rom_base_word_lr);
        write_hex(out, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_OBSERVED_INSTRUCTION", candidate_bootstrap_relocated_rom_base_word_instruction);
        write_hex(out, "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_OBSERVED_VALUE", candidate_bootstrap_relocated_rom_base_word_value);
        out << "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_READ_COUNT=" << result.candidate_bootstrap_relocated_rom_base_word_read_count << "\n";
        out << "BOOTSTRAP_RELOCATED_ROM_BASE_WORD_WRITE_COUNT=" << result.candidate_bootstrap_relocated_rom_base_word_write_count << "\n";
        out << "BOOTSTRAP_RELOCATED_HELPER_WORD_POLICY=AR_device_exact_f3c_bl_helper_0x2af8_return_0x40000000_then_f40_store_sp_plus_0x14_addr_0x09fff39c_pc_lr_0x0f40_initialized_readback_only_no_frame_widen\n";
        write_hex(out, "BOOTSTRAP_RELOCATED_HELPER_WORD_ADDRESS", candidate_bootstrap_relocated_helper_word_address);
        out << "BOOTSTRAP_RELOCATED_HELPER_WORD_WIDTH_BITS=" << (candidate_bootstrap_relocated_helper_word_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_RELOCATED_HELPER_WORD_GATE_PC", candidate_bootstrap_relocated_helper_word_pc);
        write_hex(out, "BOOTSTRAP_RELOCATED_HELPER_WORD_GATE_LR", candidate_bootstrap_relocated_helper_word_lr);
        write_hex(out, "BOOTSTRAP_RELOCATED_HELPER_WORD_OBSERVED_INSTRUCTION", candidate_bootstrap_relocated_helper_word_instruction);
        write_hex(out, "BOOTSTRAP_RELOCATED_HELPER_WORD_OBSERVED_VALUE", candidate_bootstrap_relocated_helper_word_value);
        out << "BOOTSTRAP_RELOCATED_HELPER_WORD_READ_COUNT=" << result.candidate_bootstrap_relocated_helper_word_read_count << "\n";
        out << "BOOTSTRAP_RELOCATED_HELPER_WORD_WRITE_COUNT=" << result.candidate_bootstrap_relocated_helper_word_write_count << "\n";
        out << "BOOTSTRAP_CALLEE_STACK_POLICY=AS_device_exact_stmdb_sp_20bytes_pc_0x1580_lr_0x0f4c_values_from_register_snapshot_initialized_readback_only_no_range_widen\n";
        write_hex(out, "BOOTSTRAP_CALLEE_STACK_TOP", candidate_bootstrap_callee_stack_top);
        write_hex(out, "BOOTSTRAP_CALLEE_STACK_BASE", candidate_bootstrap_callee_stack_base);
        out << "BOOTSTRAP_CALLEE_STACK_WORD_COUNT=" << candidate_bootstrap_callee_stack_word_count << "\n";
        out << "BOOTSTRAP_CALLEE_STACK_WIDTH_BITS=" << (candidate_bootstrap_callee_stack_word_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_CALLEE_STACK_GATE_PC", candidate_bootstrap_callee_stack_pc);
        write_hex(out, "BOOTSTRAP_CALLEE_STACK_GATE_LR", candidate_bootstrap_callee_stack_lr);
        write_hex(out, "BOOTSTRAP_CALLEE_STACK_OBSERVED_INSTRUCTION", candidate_bootstrap_callee_stack_instruction);
        for (std::size_t i = 0; i < candidate_bootstrap_callee_stack_word_count; ++i) {
            std::ostringstream p;
            p << "BOOTSTRAP_CALLEE_STACK_" << std::setfill('0') << std::setw(2) << i << "_";
            const auto prefix = p.str();
            write_hex(out, (prefix + "ADDRESS").c_str(), candidate_bootstrap_callee_stack_addresses[i]);
            write_hex(out, (prefix + "VALUE").c_str(), candidate_bootstrap_callee_stack_values[i]);
        }
        out << "BOOTSTRAP_CALLEE_STACK_READ_COUNT=" << result.candidate_bootstrap_callee_stack_read_count << "\n";
        out << "BOOTSTRAP_CALLEE_STACK_WRITE_COUNT=" << result.candidate_bootstrap_callee_stack_write_count << "\n";
        out << "BOOTSTRAP_CALLEE_STACK_INITIALIZED_WORDS=" << result.candidate_bootstrap_callee_stack_initialized_words << "\n";
        out << "BOOTSTRAP_CALLEE_COPY_POLICY=AT_device_exact_16byte_copy_pc_0x2344_lr_0x15fc_src_0x09fff408_dst_0x09fff510_source_verified_from_initialized_relocation_no_range_widen\n";
        write_hex(out, "BOOTSTRAP_CALLEE_COPY_SOURCE", candidate_bootstrap_callee_copy_source);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY_BASE", candidate_bootstrap_callee_copy_base);
        out << "BOOTSTRAP_CALLEE_COPY_SIZE=" << candidate_bootstrap_callee_copy_size << "\n";
        out << "BOOTSTRAP_CALLEE_COPY_WIDTH_BITS=" << (candidate_bootstrap_callee_copy_write_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_CALLEE_COPY_GATE_PC", candidate_bootstrap_callee_copy_pc);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY_GATE_LR", candidate_bootstrap_callee_copy_lr);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY_OBSERVED_INSTRUCTION", candidate_bootstrap_callee_copy_instruction);
        out << "BOOTSTRAP_CALLEE_COPY_READ_COUNT=" << result.candidate_bootstrap_callee_copy_read_count << "\n";
        out << "BOOTSTRAP_CALLEE_COPY_WRITE_COUNT=" << result.candidate_bootstrap_callee_copy_write_count << "\n";
        out << "BOOTSTRAP_CALLEE_COPY_INITIALIZED_BYTES=" << result.candidate_bootstrap_callee_copy_initialized_bytes << "\n";
        out << "BOOTSTRAP_CALLEE_COPY2_POLICY=AU_device_exact_second_16byte_copy_pc_0x2344_lr_0x15fc_src_0x09fff418_dst_0x09fff520_source_verified_from_initialized_relocation_no_range_widen\n";
        write_hex(out, "BOOTSTRAP_CALLEE_COPY2_SOURCE", candidate_bootstrap_callee_copy2_source);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY2_BASE", candidate_bootstrap_callee_copy2_base);
        out << "BOOTSTRAP_CALLEE_COPY2_SIZE=" << candidate_bootstrap_callee_copy2_size << "\n";
        out << "BOOTSTRAP_CALLEE_COPY2_WIDTH_BITS=" << (candidate_bootstrap_callee_copy2_write_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_CALLEE_COPY2_GATE_PC", candidate_bootstrap_callee_copy2_pc);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY2_GATE_LR", candidate_bootstrap_callee_copy2_lr);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY2_OBSERVED_INSTRUCTION", candidate_bootstrap_callee_copy2_instruction);
        out << "BOOTSTRAP_CALLEE_COPY2_READ_COUNT=" << result.candidate_bootstrap_callee_copy2_read_count << "\n";
        out << "BOOTSTRAP_CALLEE_COPY2_WRITE_COUNT=" << result.candidate_bootstrap_callee_copy2_write_count << "\n";
        out << "BOOTSTRAP_CALLEE_COPY2_INITIALIZED_BYTES=" << result.candidate_bootstrap_callee_copy2_initialized_bytes << "\n";
        out << "BOOTSTRAP_CALLEE_COPY3_POLICY=AV_device_exact_third_16byte_copy_pc_0x2344_lr_0x15fc_src_0x09fff428_dst_0x09fff530_source_verified_from_initialized_relocation_no_range_widen\n";
        write_hex(out, "BOOTSTRAP_CALLEE_COPY3_SOURCE", candidate_bootstrap_callee_copy3_source);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY3_BASE", candidate_bootstrap_callee_copy3_base);
        out << "BOOTSTRAP_CALLEE_COPY3_SIZE=" << candidate_bootstrap_callee_copy3_size << "\n";
        out << "BOOTSTRAP_CALLEE_COPY3_WIDTH_BITS=" << (candidate_bootstrap_callee_copy3_write_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_CALLEE_COPY3_GATE_PC", candidate_bootstrap_callee_copy3_pc);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY3_GATE_LR", candidate_bootstrap_callee_copy3_lr);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY3_OBSERVED_INSTRUCTION", candidate_bootstrap_callee_copy3_instruction);
        out << "BOOTSTRAP_CALLEE_COPY3_READ_COUNT=" << result.candidate_bootstrap_callee_copy3_read_count << "\n";
        out << "BOOTSTRAP_CALLEE_COPY3_WRITE_COUNT=" << result.candidate_bootstrap_callee_copy3_write_count << "\n";
        out << "BOOTSTRAP_CALLEE_COPY3_INITIALIZED_BYTES=" << result.candidate_bootstrap_callee_copy3_initialized_bytes << "\n";
        out << "BOOTSTRAP_CALLEE_COPY4_POLICY=AW_device_exact_fourth_16byte_copy_pc_0x2344_lr_0x15fc_src_0x09fff438_dst_0x09fff540_source_verified_from_initialized_relocation_no_range_widen\n";
        write_hex(out, "BOOTSTRAP_CALLEE_COPY4_SOURCE", candidate_bootstrap_callee_copy4_source);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY4_BASE", candidate_bootstrap_callee_copy4_base);
        out << "BOOTSTRAP_CALLEE_COPY4_SIZE=" << candidate_bootstrap_callee_copy4_size << "\n";
        out << "BOOTSTRAP_CALLEE_COPY4_WIDTH_BITS=" << (candidate_bootstrap_callee_copy4_write_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_CALLEE_COPY4_GATE_PC", candidate_bootstrap_callee_copy4_pc);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY4_GATE_LR", candidate_bootstrap_callee_copy4_lr);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY4_OBSERVED_INSTRUCTION", candidate_bootstrap_callee_copy4_instruction);
        out << "BOOTSTRAP_CALLEE_COPY4_READ_COUNT=" << result.candidate_bootstrap_callee_copy4_read_count << "\n";
        out << "BOOTSTRAP_CALLEE_COPY4_WRITE_COUNT=" << result.candidate_bootstrap_callee_copy4_write_count << "\n";
        out << "BOOTSTRAP_CALLEE_COPY4_INITIALIZED_BYTES=" << result.candidate_bootstrap_callee_copy4_initialized_bytes << "\n";
        out << "BOOTSTRAP_CALLEE_COPY5_POLICY=AX_device_exact_fifth_16byte_copy_pc_0x2344_lr_0x15fc_src_0x09fff448_dst_0x09fff550_source_verified_from_initialized_relocation_no_range_widen\n";
        write_hex(out, "BOOTSTRAP_CALLEE_COPY5_SOURCE", candidate_bootstrap_callee_copy5_source);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY5_BASE", candidate_bootstrap_callee_copy5_base);
        out << "BOOTSTRAP_CALLEE_COPY5_SIZE=" << candidate_bootstrap_callee_copy5_size << "\n";
        out << "BOOTSTRAP_CALLEE_COPY5_WIDTH_BITS=" << (candidate_bootstrap_callee_copy5_write_width * 8u) << "\n";
        write_hex(out, "BOOTSTRAP_CALLEE_COPY5_GATE_PC", candidate_bootstrap_callee_copy5_pc);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY5_GATE_LR", candidate_bootstrap_callee_copy5_lr);
        write_hex(out, "BOOTSTRAP_CALLEE_COPY5_OBSERVED_INSTRUCTION", candidate_bootstrap_callee_copy5_instruction);
        out << "BOOTSTRAP_CALLEE_COPY5_READ_COUNT=" << result.candidate_bootstrap_callee_copy5_read_count << "\n";
        out << "BOOTSTRAP_CALLEE_COPY5_WRITE_COUNT=" << result.candidate_bootstrap_callee_copy5_write_count << "\n";
        out << "BOOTSTRAP_CALLEE_COPY5_INITIALIZED_BYTES=" << result.candidate_bootstrap_callee_copy5_initialized_bytes << "\n";
        out << "RAM_BANK_HANDLER_DUMP_POLICY=raw_rom_words_0x000012a0_0x000013fc_context_no_semantic_label\n";
        out << "RAM_BANK_HANDLER_DUMP_WORDS_VALID=" << result.ram_bank_handler_dump_words_valid << "\n";
        for (std::size_t i = 0; i < result.ram_bank_handler_dump_words_valid
                                  && i < result.ram_bank_handler_dump_words.size(); ++i) {
            std::ostringstream p;
            p << "RAM_BANK_HANDLER_DUMP_" << std::setfill('0') << std::setw(2) << i << "_";
            const std::string prefix = p.str();
            write_hex(out, (prefix + "ADDRESS").c_str(),
                      ram_bank_handler_dump_begin + static_cast<std::uint32_t>(i * 4u));
            write_hex(out, (prefix + "VALUE").c_str(), result.ram_bank_handler_dump_words[i]);
        }
        out << "SETUP_B68_OBSERVED=" << (result.setup_b68_observed ? 1 : 0) << "\n";
        out << "SETUP_B68_INSTRUCTION_INDEX_BEFORE=" << result.setup_b68_instruction_index_before << "\n";
        out << "SETUP_B68_INSTRUCTION_INDEX_AFTER=" << result.setup_b68_instruction_index_after << "\n";
        out << "LOW_PAGE_TRACE_POLICY=low_4k_all_guest_bus_callbacks_no_response_change\n";
        out << "LOW_PAGE_TRACE_TOTAL_COUNT=" << result.low_page_trace_total_count << "\n";
        out << "LOW_PAGE_TRACE_CAPTURED_COUNT=" << result.low_page_trace_count << "\n";
        for (std::size_t i = 0; i < result.low_page_trace_count && i < result.low_page_trace.size(); ++i) {
            const auto &e = result.low_page_trace[i];
            out << "LOW_PAGE_TRACE_" << std::setfill('0') << std::setw(4) << i
                << "=seq:" << std::dec << e.sequence
                << ",insn:" << e.instruction_index
                << ",kind:" << low_page_trace_kind_name(e.kind)
                << ",addr:0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << e.address
                << ",width:" << std::dec << e.width_bits
                << ",value:0x" << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << e.value
                << ",pc:0x" << std::setw(8) << e.pc
                << ",lr:0x" << std::setw(8) << e.lr
                << ",ok:" << std::dec << (e.success ? 1 : 0) << "\n";
        }
        out << "CP15_TRACE_POLICY=all_condition_passed_p15_seen_before_barrier\n";
        out << "CP15_TRACE_COUNT=" << result.cp15_trace_count << "\n";
        for (std::size_t i = 0; i < result.cp15_trace_count && i < result.cp15_trace.size(); ++i) {
            const auto &e = result.cp15_trace[i];
            std::ostringstream prefix;
            prefix << "CP15_TRACE_" << std::setfill('0') << std::setw(2) << i << "_";
            const std::string p = prefix.str();
            write_hex(out, (p + "PC").c_str(), e.pc);
            write_hex(out, (p + "INSTRUCTION").c_str(), e.instruction);
            out << p << "RD=" << e.rd << "\n";
            write_hex(out, (p + "VALUE").c_str(), e.value);
            out << p << "EMULATED=" << (e.emulated ? 1 : 0) << "\n";
        }

        for (std::size_t i = 0; i < result.registers.r.size(); ++i) {
            const std::string name = "R" + std::to_string(i);
            write_hex(out, name.c_str(), result.registers.r[i]);
        }
        write_hex(out, "SP", result.registers.sp);
        write_hex(out, "LR", result.registers.lr);
        write_hex(out, "PC", result.registers.pc);
        write_hex(out, "CPSR", result.registers.cpsr);

        if (result.unresolved) {
            const auto &u = *result.unresolved;
            out << "UNRESOLVED_KIND=" << access_kind_name(u.kind) << "\n";
            out << "UNRESOLVED_WIDTH_BITS=" << (u.width * 8) << "\n";
            write_hex(out, "UNRESOLVED_ADDRESS", u.address);
            write_hex(out, "UNRESOLVED_PC", u.pc);
            write_hex(out, "UNRESOLVED_LR", u.lr);
            write_hex(out, "UNRESOLVED_VALUE", u.value, 16);
            out << "UNRESOLVED_COUNT=" << u.count << "\n";
            out << "UNRESOLVED_CAUSE=" << unresolved_cause_name(u.cause) << "\n";
        }

        if (result.exception) {
            out << "EXCEPTION_TYPE=" << result.exception->type << "\n";
            write_hex(out, "EXCEPTION_DATA", result.exception->data);
            out << "SYSTEM_CALL=" << (result.exception->system_call ? 1 : 0) << "\n";
        }

        out << "CP15_EMULATED_COUNT=" << result.cp15_emulated_count << "\n";
        if (result.cp15_emulated) {
            write_hex(out, "CP15_EMULATED_PC", result.cp15_emulated->pc);
            write_hex(out, "CP15_EMULATED_INSTRUCTION", result.cp15_emulated->instruction);
            write_hex(out, "CP15_EMULATED_VALUE", result.cp15_emulated->value);
            out << "CP15_EMULATION_POLICY=observed_arm920t_c1_write_0x1272_mmu_off\n";
        }

        if (result.cp15) {
            write_hex(out, "CP15_PC", result.cp15->pc);
            write_hex(out, "CP15_INSTRUCTION", result.cp15->instruction);
        }

        if (!result.detail.empty()) {
            out << "DETAIL=" << result.detail << "\n";
        }
        return out.str();
    }

#ifndef RH29_MACHINE1_REPORT_ONLY
    namespace {
        std::string parse_error_name(const parse_error error) {
            switch (error) {
            case parse_error::none: return "none";
            case parse_error::truncated_header: return "truncated ROM header";
            case parse_error::unexpected_rom_base: return "unexpected ROM base";
            case parse_error::invalid_rom_size: return "invalid ROM size";
            case parse_error::rom_size_exceeds_file: return "ROM size exceeds file";
            case parse_error::mapped_range_overflow: return "ROM mapped range overflows 32-bit address space";
            }
            return "unknown ROM parse error";
        }

        void snapshot_registers(arm::core *cpu, register_snapshot &out) {
            if (!cpu) return;
            for (std::size_t i = 0; i < out.r.size(); ++i) {
                out.r[i] = cpu->get_reg(i);
            }
            out.sp = cpu->get_sp();
            out.lr = cpu->get_lr();
            out.pc = cpu->get_pc();
            out.cpsr = cpu->get_cpsr();
        }
    }

    probe_result run_probe(const std::string &rom_path, const probe_options &options) {
        probe_result result{};
        result.instruction_budget = options.instruction_budget;

        std::ifstream stream(rom_path, std::ios::binary);
        if (!stream) {
            result.stop_reason = probe_stop_reason::io_error;
            result.detail = "unable to open ROM: " + rom_path;
            return result;
        }

        std::vector<std::uint8_t> rom((std::istreambuf_iterator<char>(stream)),
                                      std::istreambuf_iterator<char>());
        if (!stream.good() && !stream.eof()) {
            result.stop_reason = probe_stop_reason::io_error;
            result.detail = "failed while reading ROM";
            return result;
        }

        const parse_result parsed = parse_rom_header(rom.data(), rom.size());
        result.header = parsed.header;
        if (!parsed.ok) {
            result.stop_reason = probe_stop_reason::invalid_rom;
            result.detail = parse_error_name(parsed.error);
            return result;
        }

        for (std::size_t i = 0; i < result.setup_literal_pool_words.size(); ++i) {
            const std::size_t offset = static_cast<std::size_t>(setup_literal_pool_begin) + i * 4u;
            if (offset + 4u > rom.size()) break;
            result.setup_literal_pool_words[i] =
                static_cast<std::uint32_t>(rom[offset]) |
                (static_cast<std::uint32_t>(rom[offset + 1u]) << 8u) |
                (static_cast<std::uint32_t>(rom[offset + 2u]) << 16u) |
                (static_cast<std::uint32_t>(rom[offset + 3u]) << 24u);
            ++result.setup_literal_pool_words_valid;
        }

        for (std::size_t i = 0; i < result.ram_bank_handler_dump_words.size(); ++i) {
            const std::size_t offset = static_cast<std::size_t>(ram_bank_handler_dump_begin) + i * 4u;
            if (offset + 4u > rom.size()) break;
            result.ram_bank_handler_dump_words[i] =
                static_cast<std::uint32_t>(rom[offset]) |
                (static_cast<std::uint32_t>(rom[offset + 1u]) << 8u) |
                (static_cast<std::uint32_t>(rom[offset + 2u]) << 16u) |
                (static_cast<std::uint32_t>(rom[offset + 3u]) << 24u);
            ++result.ram_bank_handler_dump_words_valid;
        }

        // TRomHeader::restart_vector is the 32-bit instruction word stored at
        // header offset 0x7C, not a guest address. MACHINE1-O probes the ARM
        // cold-reset PC (0x00000000) and exposes canonical ROM bytes there via
        // an explicitly-labelled synthetic alias hypothesis.
        result.reset_pc = cold_reset_pc;
        result.synthetic_reset_alias = true;
        result.reset_alias_base = cold_reset_pc;
        result.reset_alias_size = parsed.header.rom_size;
        result.candidate_sdram_enabled = true;
        result.candidate_sdram_base_address = candidate_sdram_base;
        result.candidate_sdram_size_bytes = static_cast<std::uint32_t>(candidate_sdram_size);
        result.exact_observed_mmio_write_enabled = true;
        result.exact_observed_flash_command_enabled = true;
        result.exact_observed_flash_id_entry_enabled = true;
        result.amd_reference_manufacturer_id_enabled = true;
        result.amd_reference_device_id_enabled = true;
        result.flash_id_exit_enabled = true;
        result.flash_unlock1_enabled = true;
        result.flash_unlock2_enabled = true;
        result.flash_unlock_autoselect_enabled = true;
        strict_bus bus(rom.data(), parsed.header.rom_size, parsed.header.rom_base, cold_reset_pc,
                       candidate_sdram_base, candidate_sdram_size);
        auto monitor = arm::create_exclusive_monitor(arm_emulator_type::dyncom, 1);
        if (!monitor) {
            result.stop_reason = probe_stop_reason::cpu_exception;
            result.detail = "failed to create Dyncom exclusive monitor";
            return result;
        }

        // Probe-only Dyncom constructor starts the internal banked-register Mode in SVC.
        // The ordinary factory constructor starts in USER mode; merely calling set_cpsr(0xD3)
        // does not update ARMul_State::Mode, so CP15 privilege checks would be wrong.
        auto cpu = std::make_unique<arm::dyncom_core>(monitor.get(), 12, SVC32MODE);
        arm::core *cpu_ptr = cpu.get();
        cpu_ptr->set_core_number(0);

        std::uint64_t low_page_sequence = 0;
        const auto trace_low_page = [&](const low_page_trace_kind kind, const std::uint32_t address,
                                        const std::size_t width, const std::uint64_t value,
                                        const bool success) {
            if (address < low_page_trace_begin || address >= low_page_trace_end) return;
            ++result.low_page_trace_total_count;
            const std::uint64_t seq = low_page_sequence++;
            if (result.low_page_trace_count >= result.low_page_trace.size()) return;
            auto &e = result.low_page_trace[result.low_page_trace_count++];
            e.sequence = seq;
            e.instruction_index = result.executed_instructions;
            e.kind = kind;
            e.address = address;
            e.width_bits = static_cast<std::uint32_t>(width * 8u);
            e.value = value;
            e.pc = cpu_ptr->get_pc();
            e.lr = cpu_ptr->get_lr();
            e.success = success;
        };
        const auto to_low_kind = [](const access_kind kind) {
            return kind == access_kind::code_read ? low_page_trace_kind::code_read
                 : low_page_trace_kind::data_read;
        };
        auto read8 = [&](std::uint32_t a, std::uint8_t *v, access_kind k) {
            const bool ok = bus.read(k, a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
            trace_low_page(to_low_kind(k), a, sizeof(*v), ok ? *v : 0u, ok);
            return ok;
        };
        auto read16 = [&](std::uint32_t a, std::uint16_t *v, access_kind k) {
            const bool ok = bus.read(k, a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
            trace_low_page(to_low_kind(k), a, sizeof(*v), ok ? *v : 0u, ok);
            return ok;
        };
        auto read32 = [&](std::uint32_t a, std::uint32_t *v, access_kind k) {
            const bool ok = bus.read(k, a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
            trace_low_page(to_low_kind(k), a, sizeof(*v), ok ? *v : 0u, ok);
            return ok;
        };
        auto read64 = [&](std::uint32_t a, std::uint64_t *v, access_kind k) {
            const bool ok = bus.read(k, a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
            trace_low_page(to_low_kind(k), a, sizeof(*v), ok ? *v : 0u, ok);
            return ok;
        };
        auto write8 = [&](std::uint32_t a, std::uint8_t *v) {
            const bool ok = bus.write(a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
            trace_low_page(low_page_trace_kind::data_write, a, sizeof(*v), *v, ok);
            return ok;
        };
        auto write16 = [&](std::uint32_t a, std::uint16_t *v) {
            const bool ok = bus.write(a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
            trace_low_page(low_page_trace_kind::data_write, a, sizeof(*v), *v, ok);
            return ok;
        };
        auto write32 = [&](std::uint32_t a, std::uint32_t *v) {
            const bool ok = bus.write(a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
            trace_low_page(low_page_trace_kind::data_write, a, sizeof(*v), *v, ok);
            return ok;
        };
        auto write64 = [&](std::uint32_t a, std::uint64_t *v) {
            const bool ok = bus.write(a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
            trace_low_page(low_page_trace_kind::data_write, a, sizeof(*v), *v, ok);
            return ok;
        };

        cpu->read_8bit = [&](std::uint32_t a, std::uint8_t *v) { return read8(a, v, access_kind::data_read); };
        cpu->read_16bit = [&](std::uint32_t a, std::uint16_t *v) { return read16(a, v, access_kind::data_read); };
        cpu->read_32bit = [&](std::uint32_t a, std::uint32_t *v) { return read32(a, v, access_kind::data_read); };
        cpu->read_64bit = [&](std::uint32_t a, std::uint64_t *v) { return read64(a, v, access_kind::data_read); };
        cpu->read_code = [&](std::uint32_t a, std::uint32_t *v) { return read32(a, v, access_kind::code_read); };
        cpu->write_8bit = write8;
        cpu->write_16bit = write16;
        cpu->write_32bit = write32;
        cpu->write_64bit = write64;

        monitor->read_8bit = [&](arm::core *, std::uint32_t a, std::uint8_t *v) {
            const bool ok = read8(a, v, access_kind::data_read); if (!ok) cpu_ptr->stop(); return ok;
        };
        monitor->read_16bit = [&](arm::core *, std::uint32_t a, std::uint16_t *v) {
            const bool ok = read16(a, v, access_kind::data_read); if (!ok) cpu_ptr->stop(); return ok;
        };
        monitor->read_32bit = [&](arm::core *, std::uint32_t a, std::uint32_t *v) {
            const bool ok = read32(a, v, access_kind::data_read); if (!ok) cpu_ptr->stop(); return ok;
        };
        monitor->read_64bit = [&](arm::core *, std::uint32_t a, std::uint64_t *v) {
            const bool ok = read64(a, v, access_kind::data_read); if (!ok) cpu_ptr->stop(); return ok;
        };
        monitor->write_8bit = [&](arm::core *, std::uint32_t a, std::uint8_t value, std::uint8_t) {
            const bool ok = write8(a, &value);
            if (!ok) cpu_ptr->stop();
            return 0;
        };
        monitor->write_16bit = [&](arm::core *, std::uint32_t a, std::uint16_t value, std::uint16_t) {
            const bool ok = write16(a, &value);
            if (!ok) cpu_ptr->stop();
            return 0;
        };
        monitor->write_32bit = [&](arm::core *, std::uint32_t a, std::uint32_t value, std::uint32_t) {
            const bool ok = write32(a, &value);
            if (!ok) cpu_ptr->stop();
            return 0;
        };
        monitor->write_64bit = [&](arm::core *, std::uint32_t a, std::uint64_t value, std::uint64_t) {
            const bool ok = write64(a, &value);
            if (!ok) cpu_ptr->stop();
            return 0;
        };

        std::optional<cpu_exception_info> exception;
        std::optional<cp15_access_info> cp15;
        const auto push_a32_trace = [&](const a32_trace_entry &entry) {
            if (result.a32_trace_count < result.a32_trace.size()) {
                result.a32_trace[result.a32_trace_count++] = entry;
                return;
            }
            for (std::size_t i = 1; i < result.a32_trace.size(); ++i) {
                result.a32_trace[i - 1] = result.a32_trace[i];
            }
            result.a32_trace.back() = entry;
        };
        const auto capture_full_trace = [&](std::array<full_a32_trace_entry, early_setup_trace_capacity> &trace,
                                            std::uint32_t &count, const std::uint32_t pc,
                                            const std::uint32_t instruction) {
            if (count >= trace.size()) return;
            auto &entry = trace[count++];
            entry.pc = pc;
            entry.instruction = instruction;
            entry.cpsr = cpu->get_cpsr();
            for (std::size_t reg = 0; reg < entry.r.size(); ++reg) {
                entry.r[reg] = cpu->get_reg(reg);
            }
            entry.sp = cpu->get_sp();
            entry.lr = cpu->get_lr();
        };
        const auto capture_callsite_trace = [&](const std::uint32_t pc, const std::uint32_t instruction) {
            if (result.callsite_trace_count >= result.callsite_trace.size()) return;
            auto &entry = result.callsite_trace[result.callsite_trace_count++];
            entry.pc = pc;
            entry.instruction = instruction;
            entry.cpsr = cpu->get_cpsr();
            for (std::size_t reg = 0; reg < entry.r.size(); ++reg) {
                entry.r[reg] = cpu->get_reg(reg);
            }
            entry.sp = cpu->get_sp();
            entry.lr = cpu->get_lr();
        };
        cpu->exception_handler = [&](arm::exception_type type, std::uint32_t data) {
            // A failed bus callback already carries the more precise unresolved-access record.
            if (!bus.first_unresolved() && !exception) {
                exception = cpu_exception_info{static_cast<int>(type), data, false};
            }
            cpu_ptr->stop();
            return false;
        };
        cpu->system_call_handler = [&](std::uint32_t value) {
            if (!exception) {
                exception = cpu_exception_info{static_cast<int>(arm::exception_type_unk), value, true};
            }
            cpu_ptr->stop();
        };

        for (std::size_t i = 0; i <= 12; ++i) {
            cpu->set_reg(i, 0);
        }
        cpu->set_sp(0);
        cpu->set_lr(0);
        cpu->set_cpsr(0x000000D3u);
        cpu->set_pc(result.reset_pc);

        std::uint32_t remaining = options.instruction_budget;
        while (remaining > 0 && !bus.first_unresolved() && !exception && !cp15) {
            // MACHINE1-O must not let Dyncom's ARM11/MPCore CP15 model answer
            // RH-29 hardware questions. Inspect the next A32 instruction while
            // it is still only ROM data and stop before any p15 operation runs.
            if (!cpu->is_thumb_mode()) {
                const std::uint32_t pc = cpu->get_pc();
                std::uint32_t instruction = 0;
                if (!bus.read(access_kind::code_read, pc, &instruction, sizeof(instruction),
                              pc, cpu->get_lr())) {
                    break;
                }
                push_a32_trace(a32_trace_entry{
                    pc,
                    instruction,
                    cpu->get_cpsr(),
                    cpu->get_reg(0),
                    cpu->get_reg(1),
                    cpu->get_reg(2),
                    cpu->get_reg(3),
                    cpu->get_reg(4),
                    cpu->get_reg(8),
                    cpu->get_reg(9),
                    cpu->get_reg(10),
                    cpu->get_reg(11),
                    cpu->get_reg(12),
                    cpu->get_sp(),
                    cpu->get_lr()
                });
                if (pc >= early_setup_trace_pc_begin && pc < early_setup_trace_pc_end) {
                    capture_full_trace(result.early_setup_trace, result.early_setup_trace_count, pc, instruction);
                }
                if (pc >= callsite_trace_pc_begin && pc < callsite_trace_pc_end) {
                    capture_callsite_trace(pc, instruction);
                }
                if (is_arm_cp15_instruction(instruction)
                    && arm_condition_passed(instruction, cpu->get_cpsr())) {
                    const std::uint32_t rd = (instruction >> 12) & 0xFu;
                    const std::uint32_t value = rd <= 15 ? cpu->get_reg(rd) : 0;
                    cp15_trace_entry *cp15_trace = nullptr;
                    if (result.cp15_trace_count < result.cp15_trace.size()) {
                        cp15_trace = &result.cp15_trace[result.cp15_trace_count++];
                        cp15_trace->pc = pc;
                        cp15_trace->instruction = instruction;
                        cp15_trace->rd = rd;
                        cp15_trace->value = value;
                    }
                    if (remaining > 0 && is_observed_arm920t_control_write(instruction, value)) {
                        if (cp15_trace) cp15_trace->emulated = true;
                        result.cp15_emulated = cp15_emulation_info{pc, instruction, value};
                        ++result.cp15_emulated_count;
                        ++result.executed_instructions;
                        --remaining;
                        cpu->set_pc(pc + 4);
                        continue;
                    }

                    cp15 = cp15_access_info{pc, instruction};
                    break;
                }
            }

            const std::uint32_t step_pc = cpu->get_pc();
            const bool stepping_b68 = !cpu->is_thumb_mode() && step_pc == 0x00000B68u;
            if (stepping_b68 && !result.setup_b68_observed) {
                result.setup_b68_instruction_index_before = result.executed_instructions;
            }
            cpu->step();
            const std::uint32_t progressed = cpu->get_num_instruction_executed();
            if (progressed == 0) {
                break;
            }
            const std::uint32_t consumed = std::min(progressed, remaining);
            result.executed_instructions += consumed;
            remaining -= consumed;
            if (stepping_b68 && !result.setup_b68_observed) {
                result.setup_b68_observed = true;
                result.setup_b68_instruction_index_after = result.executed_instructions;
            }
        }

        snapshot_registers(cpu.get(), result.registers);
        result.unresolved = bus.first_unresolved();
        result.exception = exception;
        result.cp15 = cp15;
        result.candidate_sdram_write_count = bus.ram_write_count();
        result.candidate_sdram_initialized_bytes = bus.ram_initialized_bytes();
        result.exact_observed_mmio_write_count = bus.observed_mmio_write_count();
        result.exact_observed_flash_command_count = bus.observed_flash_command_count();
        result.exact_observed_flash_id_entry_count = bus.observed_flash_id_entry_count();
        result.amd_reference_manufacturer_read_count = bus.amd_reference_manufacturer_read_count();
        result.amd_reference_device_read_count = bus.amd_reference_device_read_count();
        result.flash_id_exit_count = bus.observed_flash_id_exit_count();
        result.flash_autoselect_active_at_stop = bus.flash_autoselect_active();
        result.flash_unlock1_count = bus.observed_flash_unlock1_count();
        result.flash_unlock2_count = bus.observed_flash_unlock2_count();
        result.flash_unlock_autoselect_count = bus.observed_flash_unlock_autoselect_count();
        result.flash_unlock_stage_at_stop = bus.flash_unlock_stage();

        result.candidate_ram_bank_probe_anchor_read_count =
            bus.candidate_ram_bank_probe_anchor_read_count();
        result.candidate_ram_bank_sparse_probe_original_read_count =
            bus.candidate_ram_bank_sparse_probe_original_read_count();
        result.candidate_ram_bank_sparse_probe_test_write_count =
            bus.candidate_ram_bank_sparse_probe_test_write_count();
        result.candidate_ram_bank_sparse_probe_anchor_verify_read_count =
            bus.candidate_ram_bank_sparse_probe_anchor_verify_read_count();
        result.candidate_ram_bank_sparse_probe_readback_count =
            bus.candidate_ram_bank_sparse_probe_readback_count();
        result.candidate_ram_bank_sparse_probe_restore_write_count =
            bus.candidate_ram_bank_sparse_probe_restore_write_count();
        result.candidate_bootstrap_local_frame_word_read_count =
            bus.candidate_bootstrap_local_frame_word_read_count();
        result.candidate_bootstrap_local_frame_word_write_count =
            bus.candidate_bootstrap_local_frame_word_write_count();
        result.candidate_bootstrap_relocation_read_count =
            bus.candidate_bootstrap_relocation_read_count();
        result.candidate_bootstrap_relocation_write_count =
            bus.candidate_bootstrap_relocation_write_count();
        result.candidate_bootstrap_relocation_initialized_bytes =
            bus.candidate_bootstrap_relocation_initialized_bytes();
        result.candidate_bootstrap_local_frame_tail_read_count =
            bus.candidate_bootstrap_local_frame_tail_read_count();
        result.candidate_bootstrap_local_frame_tail_write_count =
            bus.candidate_bootstrap_local_frame_tail_write_count();
        result.candidate_bootstrap_local_frame_tail_initialized_words =
            bus.candidate_bootstrap_local_frame_tail_initialized_words();
        result.candidate_bootstrap_relocated_stack_read_count =
            bus.candidate_bootstrap_relocated_stack_read_count();
        result.candidate_bootstrap_relocated_stack_write_count =
            bus.candidate_bootstrap_relocated_stack_write_count();
        result.candidate_bootstrap_relocated_stack_initialized_words =
            bus.candidate_bootstrap_relocated_stack_initialized_words();
        result.candidate_bootstrap_relocated_local_word_read_count =
            bus.candidate_bootstrap_relocated_local_word_read_count();
        result.candidate_bootstrap_relocated_local_word_write_count =
            bus.candidate_bootstrap_relocated_local_word_write_count();
        result.candidate_bootstrap_relocated_rom_base_word_read_count =
            bus.candidate_bootstrap_relocated_rom_base_word_read_count();
        result.candidate_bootstrap_relocated_rom_base_word_write_count =
            bus.candidate_bootstrap_relocated_rom_base_word_write_count();
        result.candidate_bootstrap_relocated_helper_word_read_count =
            bus.candidate_bootstrap_relocated_helper_word_read_count();
        result.candidate_bootstrap_relocated_helper_word_write_count =
            bus.candidate_bootstrap_relocated_helper_word_write_count();
        result.candidate_bootstrap_callee_stack_read_count =
            bus.candidate_bootstrap_callee_stack_read_count();
        result.candidate_bootstrap_callee_stack_write_count =
            bus.candidate_bootstrap_callee_stack_write_count();
        result.candidate_bootstrap_callee_stack_initialized_words =
            bus.candidate_bootstrap_callee_stack_initialized_words();
        result.candidate_bootstrap_callee_copy_read_count =
            bus.candidate_bootstrap_callee_copy_read_count();
        result.candidate_bootstrap_callee_copy_write_count =
            bus.candidate_bootstrap_callee_copy_write_count();
        result.candidate_bootstrap_callee_copy_initialized_bytes =
            bus.candidate_bootstrap_callee_copy_initialized_bytes();
        result.candidate_bootstrap_callee_copy2_read_count =
            bus.candidate_bootstrap_callee_copy2_read_count();
        result.candidate_bootstrap_callee_copy2_write_count =
            bus.candidate_bootstrap_callee_copy2_write_count();
        result.candidate_bootstrap_callee_copy2_initialized_bytes =
            bus.candidate_bootstrap_callee_copy2_initialized_bytes();
        result.candidate_bootstrap_callee_copy3_read_count =
            bus.candidate_bootstrap_callee_copy3_read_count();
        result.candidate_bootstrap_callee_copy3_write_count =
            bus.candidate_bootstrap_callee_copy3_write_count();
        result.candidate_bootstrap_callee_copy3_initialized_bytes =
            bus.candidate_bootstrap_callee_copy3_initialized_bytes();
        result.candidate_bootstrap_callee_copy4_read_count =
            bus.candidate_bootstrap_callee_copy4_read_count();
        result.candidate_bootstrap_callee_copy4_write_count =
            bus.candidate_bootstrap_callee_copy4_write_count();
        result.candidate_bootstrap_callee_copy4_initialized_bytes =
            bus.candidate_bootstrap_callee_copy4_initialized_bytes();
        result.candidate_bootstrap_callee_copy5_read_count =
            bus.candidate_bootstrap_callee_copy5_read_count();
        result.candidate_bootstrap_callee_copy5_write_count =
            bus.candidate_bootstrap_callee_copy5_write_count();
        result.candidate_bootstrap_callee_copy5_initialized_bytes =
            bus.candidate_bootstrap_callee_copy5_initialized_bytes();

        if (result.unresolved) {
            result.stop_reason = probe_stop_reason::unresolved_access;
        } else if (result.cp15) {
            result.stop_reason = probe_stop_reason::cp15_access;
        } else if (result.exception) {
            result.stop_reason = probe_stop_reason::cpu_exception;
        } else if (result.executed_instructions >= options.instruction_budget) {
            result.stop_reason = probe_stop_reason::budget_exhausted;
        } else {
            result.stop_reason = probe_stop_reason::cpu_exception;
            result.detail = "CPU stopped before budget without an explicit exception";
        }
        return result;
    }
#endif
}
