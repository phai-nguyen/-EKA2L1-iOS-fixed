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
        bool exact_observed_mmio_write_enabled = false;
        std::uint64_t exact_observed_mmio_write_count = 0;
        bool exact_observed_flash_command_enabled = false;
        std::uint64_t exact_observed_flash_command_count = 0;
        bool exact_observed_flash_id_entry_enabled = false;
        std::uint64_t exact_observed_flash_id_entry_count = 0;
        bool amd_reference_manufacturer_id_enabled = false;
        std::uint64_t amd_reference_manufacturer_read_count = 0;
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
