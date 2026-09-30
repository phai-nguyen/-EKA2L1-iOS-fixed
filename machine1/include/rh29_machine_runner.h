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
        invalid_rom,
        io_error
    };

    struct probe_options {
        std::uint32_t instruction_budget = 10000;
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

    struct probe_result {
        rom_header_info header{};
        std::uint32_t instruction_budget = 0;
        std::uint32_t executed_instructions = 0;
        probe_stop_reason stop_reason = probe_stop_reason::io_error;
        register_snapshot registers{};
        std::optional<unresolved_access> unresolved{};
        std::optional<cpu_exception_info> exception{};
        std::string detail{};
    };

    probe_result run_probe(const std::string &rom_path, const probe_options &options);
    std::string format_report(const probe_result &result);
}
