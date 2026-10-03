#pragma once

#include "rh29_machine_model.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace eka2l1::machine::rh29 {
    enum class probe_stop_reason {
        budget_exhausted,
        unresolved_access,
        cpu_exception,
        cp15_access,
        invalid_rom,
        io_error
    };

    struct probe_options {
        std::uint32_t instruction_budget = 1000;
    };

    struct register_snapshot {
        std::array<std::uint32_t, 13> r{};
        std::uint32_t sp = 0;
        std::uint32_t lr = 0;
        std::uint32_t pc = 0;
        std::uint32_t cpsr = 0;
    };

    struct cpu_exception_info {
        int type = 0;
        std::uint32_t data = 0;
        bool system_call = false;
    };

    struct cp15_access_info {
        std::uint32_t pc = 0;
        std::uint32_t instruction = 0;
    };

    struct cp15_emulation_info {
        std::uint32_t pc = 0;
        std::uint32_t instruction = 0;
        std::uint32_t value = 0;
    };

    struct a32_trace_entry {
        std::uint32_t pc = 0;
        std::uint32_t instruction = 0;
        std::uint32_t cpsr = 0;
        std::uint32_t r0 = 0;
        std::uint32_t r1 = 0;
        std::uint32_t r2 = 0;
        std::uint32_t r3 = 0;
        std::uint32_t r4 = 0;
        std::uint32_t r8 = 0;
        std::uint32_t r9 = 0;
        std::uint32_t r10 = 0;
        std::uint32_t r11 = 0;
        std::uint32_t r12 = 0;
        std::uint32_t sp = 0;
        std::uint32_t lr = 0;
    };

    static constexpr std::size_t a32_trace_capacity = 64;

    // MACHINE1-V diagnostic-only trace for the early setup window containing
    // the five observed SDRAM writes and the 0x0C150004 write.
    static constexpr std::uint32_t early_setup_trace_pc_begin = 0x00000B00u;
    static constexpr std::uint32_t early_setup_trace_pc_end = 0x00000B80u;
    static constexpr std::size_t early_setup_trace_capacity = 64u;

    struct full_a32_trace_entry {
        std::uint32_t pc = 0;
        std::uint32_t instruction = 0;
        std::uint32_t cpsr = 0;
        std::array<std::uint32_t, 13> r{};
        std::uint32_t sp = 0;
        std::uint32_t lr = 0;
    };

    // MACHINE1-W is diagnostic-only. These traces observe the already-existing
    // guest bus/CPU path without changing any memory response or remap state.
    enum class low_page_trace_kind {
        code_read,
        data_read,
        data_write
    };

    static constexpr std::uint32_t low_page_trace_begin = 0x00000000u;
    static constexpr std::uint32_t low_page_trace_end = 0x00001000u;
    static constexpr std::size_t low_page_trace_capacity = 4096u;
    static constexpr std::uint32_t callsite_trace_pc_begin = 0x00000300u;
    static constexpr std::uint32_t callsite_trace_pc_end = 0x00000380u;
    static constexpr std::size_t callsite_trace_capacity = 64u;
    static constexpr std::uint32_t setup_literal_pool_begin = 0x00000B90u;
    static constexpr std::uint32_t setup_literal_pool_end = 0x00000BC4u;
    static constexpr std::size_t setup_literal_pool_word_count =
        (setup_literal_pool_end - setup_literal_pool_begin) / sizeof(std::uint32_t);
    static constexpr std::uint32_t ram_bank_handler_dump_begin = 0x000012A0u;
    static constexpr std::uint32_t ram_bank_handler_dump_end = 0x00001400u;
    static constexpr std::size_t ram_bank_handler_dump_word_count =
        (ram_bank_handler_dump_end - ram_bank_handler_dump_begin) / sizeof(std::uint32_t);
    static constexpr std::size_t cp15_trace_capacity = 16u;

    struct low_page_trace_entry {
        std::uint64_t sequence = 0;
        std::uint32_t instruction_index = 0;
        low_page_trace_kind kind = low_page_trace_kind::data_read;
        std::uint32_t address = 0;
        std::uint32_t width_bits = 0;
        std::uint64_t value = 0;
        std::uint32_t pc = 0;
        std::uint32_t lr = 0;
        bool success = false;
    };

    struct cp15_trace_entry {
        std::uint32_t pc = 0;
        std::uint32_t instruction = 0;
        std::uint32_t rd = 0;
        std::uint32_t value = 0;
        bool emulated = false;
    };

    struct probe_result {
        rom_header_info header{};
        std::uint32_t instruction_budget = 0;
        std::uint32_t executed_instructions = 0;
        std::uint32_t reset_pc = cold_reset_pc;
        bool synthetic_reset_alias = false;
        std::uint32_t reset_alias_base = 0;
        std::uint32_t reset_alias_size = 0;
        bool candidate_sdram_enabled = false;
        std::uint32_t candidate_sdram_base_address = 0;
        std::uint32_t candidate_sdram_size_bytes = 0;
        std::uint64_t candidate_sdram_write_count = 0;
        std::uint64_t candidate_sdram_initialized_bytes = 0;
        std::array<ram_write_trace_entry, ram_write_trace_capacity> candidate_sdram_write_trace{};
        std::uint32_t candidate_sdram_write_trace_count = 0;
        bool exact_observed_mmio_write_enabled = false;
        std::uint64_t exact_observed_mmio_write_count = 0;
        bool exact_observed_flash_command_enabled = false;
        std::uint64_t exact_observed_flash_command_count = 0;
        bool exact_observed_flash_id_entry_enabled = false;
        std::uint64_t exact_observed_flash_id_entry_count = 0;
        bool amd_reference_manufacturer_id_enabled = false;
        std::uint64_t amd_reference_manufacturer_read_count = 0;
        bool amd_reference_device_id_enabled = false;
        std::uint64_t amd_reference_device_read_count = 0;
        bool flash_id_exit_enabled = false;
        std::uint64_t flash_id_exit_count = 0;
        bool flash_autoselect_active_at_stop = false;
        bool flash_unlock1_enabled = false;
        std::uint64_t flash_unlock1_count = 0;
        bool flash_unlock2_enabled = false;
        std::uint64_t flash_unlock2_count = 0;
        bool flash_unlock_autoselect_enabled = false;
        std::uint64_t flash_unlock_autoselect_count = 0;
        std::uint32_t flash_unlock_stage_at_stop = 0;
        bool candidate_ram_probe_enabled = false;
        std::uint64_t candidate_ram_probe_read_count = 0;
        std::uint64_t candidate_ram_probe_write_count = 0;
        bool candidate_bootstrap_copy_enabled = false;
        std::uint64_t candidate_bootstrap_copy_read_count = 0;
        std::uint64_t candidate_bootstrap_copy_write_count = 0;
        std::uint64_t candidate_bootstrap_copy_initialized_bytes = 0;
        bool candidate_bootstrap_post_copy_mutation_enabled = false;
        std::uint64_t candidate_bootstrap_post_copy_mutation_count = 0;
        bool candidate_bootstrap_record_loop_mutation_enabled = false;
        std::uint64_t candidate_bootstrap_record_loop_mutation_count = 0;
        bool candidate_bootstrap_stack_enabled = false;
        std::uint64_t candidate_bootstrap_stack_read_count = 0;
        std::uint64_t candidate_bootstrap_stack_write_count = 0;
        std::uint64_t candidate_bootstrap_stack_initialized_bytes = 0;
        bool candidate_bootstrap_nested_stack_enabled = false;
        std::uint64_t candidate_bootstrap_nested_stack_read_count = 0;
        std::uint64_t candidate_bootstrap_nested_stack_write_count = 0;
        std::uint64_t candidate_bootstrap_nested_stack_initialized_bytes = 0;
        std::uint64_t candidate_ram_bank_probe_anchor_read_count = 0;
        std::uint64_t candidate_ram_bank_sparse_probe_original_read_count = 0;
        std::uint64_t candidate_ram_bank_sparse_probe_test_write_count = 0;
        std::uint64_t candidate_ram_bank_sparse_probe_anchor_verify_read_count = 0;
        std::uint64_t candidate_ram_bank_sparse_probe_readback_count = 0;
        std::uint64_t candidate_ram_bank_sparse_probe_restore_write_count = 0;
        std::uint64_t candidate_bootstrap_local_frame_word_read_count = 0;
        std::uint64_t candidate_bootstrap_local_frame_word_write_count = 0;
        std::uint64_t candidate_bootstrap_relocation_read_count = 0;
        std::uint64_t candidate_bootstrap_relocation_write_count = 0;
        std::uint64_t candidate_bootstrap_relocation_initialized_bytes = 0;
        std::uint64_t candidate_bootstrap_local_frame_tail_read_count = 0;
        std::uint64_t candidate_bootstrap_local_frame_tail_write_count = 0;
        std::uint64_t candidate_bootstrap_local_frame_tail_initialized_words = 0;
        std::uint64_t candidate_bootstrap_relocated_stack_read_count = 0;
        std::uint64_t candidate_bootstrap_relocated_stack_write_count = 0;
        std::uint64_t candidate_bootstrap_relocated_stack_initialized_words = 0;
        std::uint64_t candidate_bootstrap_relocated_local_word_read_count = 0;
        std::uint64_t candidate_bootstrap_relocated_local_word_write_count = 0;
        std::uint64_t candidate_bootstrap_relocated_rom_base_word_read_count = 0;
        std::uint64_t candidate_bootstrap_relocated_rom_base_word_write_count = 0;
        std::uint64_t candidate_bootstrap_relocated_helper_word_read_count = 0;
        std::uint64_t candidate_bootstrap_relocated_helper_word_write_count = 0;
        std::uint64_t candidate_bootstrap_callee_stack_read_count = 0;
        std::uint64_t candidate_bootstrap_callee_stack_write_count = 0;
        std::uint64_t candidate_bootstrap_callee_stack_initialized_words = 0;
        std::uint64_t candidate_bootstrap_callee_copy_read_count = 0;
        std::uint64_t candidate_bootstrap_callee_copy_write_count = 0;
        std::uint64_t candidate_bootstrap_callee_copy_initialized_bytes = 0;
        std::uint64_t candidate_bootstrap_callee_copy2_read_count = 0;
        std::uint64_t candidate_bootstrap_callee_copy2_write_count = 0;
        std::uint64_t candidate_bootstrap_callee_copy2_initialized_bytes = 0;
        bool candidate_post_probe_workspace_enabled = false;
        std::uint64_t candidate_post_probe_workspace_read_count = 0;
        std::uint64_t candidate_post_probe_workspace_write_count = 0;
        bool low_vector_shadow_enabled = false;
        std::uint64_t low_vector_shadow_read_count = 0;
        std::uint64_t low_vector_shadow_write_count = 0;
        std::array<a32_trace_entry, a32_trace_capacity> a32_trace{};
        std::uint32_t a32_trace_count = 0;
        std::array<full_a32_trace_entry, early_setup_trace_capacity> early_setup_trace{};
        std::uint32_t early_setup_trace_count = 0;
        std::array<full_a32_trace_entry, callsite_trace_capacity> callsite_trace{};
        std::uint32_t callsite_trace_count = 0;
        std::array<std::uint32_t, setup_literal_pool_word_count> setup_literal_pool_words{};
        std::uint32_t setup_literal_pool_words_valid = 0;
        std::array<std::uint32_t, ram_bank_handler_dump_word_count> ram_bank_handler_dump_words{};
        std::uint32_t ram_bank_handler_dump_words_valid = 0;
        std::array<low_page_trace_entry, low_page_trace_capacity> low_page_trace{};
        std::uint32_t low_page_trace_count = 0;
        std::uint64_t low_page_trace_total_count = 0;
        std::array<cp15_trace_entry, cp15_trace_capacity> cp15_trace{};
        std::uint32_t cp15_trace_count = 0;
        bool setup_b68_observed = false;
        std::uint32_t setup_b68_instruction_index_before = 0;
        std::uint32_t setup_b68_instruction_index_after = 0;
        probe_stop_reason stop_reason = probe_stop_reason::io_error;
        register_snapshot registers{};
        std::optional<unresolved_access> unresolved{};
        std::optional<cpu_exception_info> exception{};
        std::optional<cp15_access_info> cp15{};
        std::optional<cp15_emulation_info> cp15_emulated{};
        std::uint32_t cp15_emulated_count = 0;
        std::string detail{};
    };

    bool is_arm_cp15_instruction(std::uint32_t instruction);
    bool is_observed_arm920t_control_write(std::uint32_t instruction, std::uint32_t value);
    bool arm_condition_passed(std::uint32_t instruction, std::uint32_t cpsr);
    probe_result run_probe(const std::string &rom_path, const probe_options &options);
    std::string format_report(const probe_result &result);
}
