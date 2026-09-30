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

    std::string format_report(const probe_result &result) {
        std::ostringstream out;
        out << "RH29_MACHINE1_A\n";
        write_hex(out, "ROM_BASE", result.header.rom_base);
        write_hex(out, "ROM_SIZE", result.header.rom_size);
        write_hex(out, "RESTART_VECTOR", result.header.restart_vector);
        write_hex(out, "KERN_DATA_ADDRESS", result.header.kern_data_address);
        write_hex(out, "KERN_LIMIT", result.header.kern_limit);
        out << "INSTRUCTION_BUDGET=" << result.instruction_budget << "\n";
        out << "EXECUTED_INSTRUCTIONS=" << result.executed_instructions << "\n";
        out << "STOP_REASON=" << stop_reason_name(result.stop_reason) << "\n";

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
        }

        if (result.exception) {
            out << "EXCEPTION_TYPE=" << result.exception->type << "\n";
            write_hex(out, "EXCEPTION_DATA", result.exception->data);
            out << "SYSTEM_CALL=" << (result.exception->system_call ? 1 : 0) << "\n";
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

        if (parsed.header.restart_vector < parsed.header.rom_base
            || static_cast<std::uint64_t>(parsed.header.restart_vector) + 4ULL
                > static_cast<std::uint64_t>(parsed.header.rom_base) + parsed.header.rom_size) {
            result.stop_reason = probe_stop_reason::invalid_rom;
            result.detail = "restart vector is outside mapped ROM";
            return result;
        }

        strict_bus bus(rom.data(), parsed.header.rom_size, parsed.header.rom_base);
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

        auto read8 = [&](std::uint32_t a, std::uint8_t *v, access_kind k) {
            return bus.read(k, a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
        };
        auto read16 = [&](std::uint32_t a, std::uint16_t *v, access_kind k) {
            return bus.read(k, a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
        };
        auto read32 = [&](std::uint32_t a, std::uint32_t *v, access_kind k) {
            return bus.read(k, a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
        };
        auto read64 = [&](std::uint32_t a, std::uint64_t *v, access_kind k) {
            return bus.read(k, a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
        };
        auto write8 = [&](std::uint32_t a, std::uint8_t *v) {
            return bus.write(a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
        };
        auto write16 = [&](std::uint32_t a, std::uint16_t *v) {
            return bus.write(a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
        };
        auto write32 = [&](std::uint32_t a, std::uint32_t *v) {
            return bus.write(a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
        };
        auto write64 = [&](std::uint32_t a, std::uint64_t *v) {
            return bus.write(a, v, sizeof(*v), cpu_ptr->get_pc(), cpu_ptr->get_lr());
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
            bus.write(a, &value, sizeof(value), cpu_ptr->get_pc(), cpu_ptr->get_lr()); cpu_ptr->stop(); return 0;
        };
        monitor->write_16bit = [&](arm::core *, std::uint32_t a, std::uint16_t value, std::uint16_t) {
            bus.write(a, &value, sizeof(value), cpu_ptr->get_pc(), cpu_ptr->get_lr()); cpu_ptr->stop(); return 0;
        };
        monitor->write_32bit = [&](arm::core *, std::uint32_t a, std::uint32_t value, std::uint32_t) {
            bus.write(a, &value, sizeof(value), cpu_ptr->get_pc(), cpu_ptr->get_lr()); cpu_ptr->stop(); return 0;
        };
        monitor->write_64bit = [&](arm::core *, std::uint32_t a, std::uint64_t value, std::uint64_t) {
            bus.write(a, &value, sizeof(value), cpu_ptr->get_pc(), cpu_ptr->get_lr()); cpu_ptr->stop(); return 0;
        };

        std::optional<cpu_exception_info> exception;
        std::optional<cp15_access_info> cp15;
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
        cpu->set_pc(parsed.header.restart_vector);

        std::uint32_t remaining = options.instruction_budget;
        while (remaining > 0 && !bus.first_unresolved() && !exception && !cp15) {
            // MACHINE1-A must not let Dyncom's ARM11/MPCore CP15 model answer
            // RH-29 hardware questions. Inspect the next A32 instruction while
            // it is still only ROM data and stop before any p15 operation runs.
            if (!cpu->is_thumb_mode()) {
                const std::uint32_t pc = cpu->get_pc();
                std::uint32_t instruction = 0;
                if (!bus.read(access_kind::code_read, pc, &instruction, sizeof(instruction),
                              pc, cpu->get_lr())) {
                    break;
                }
                if (is_arm_cp15_instruction(instruction)) {
                    cp15 = cp15_access_info{pc, instruction};
                    break;
                }
            }

            cpu->step();
            const std::uint32_t progressed = cpu->get_num_instruction_executed();
            if (progressed == 0) {
                break;
            }
            const std::uint32_t consumed = std::min(progressed, remaining);
            result.executed_instructions += consumed;
            remaining -= consumed;
        }

        snapshot_registers(cpu.get(), result.registers);
        result.unresolved = bus.first_unresolved();
        result.exception = exception;
        result.cp15 = cp15;

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
