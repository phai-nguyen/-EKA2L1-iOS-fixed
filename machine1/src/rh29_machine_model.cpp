#include "rh29_machine_model.h"

#include <cstring>
#include <limits>

namespace eka2l1::machine::rh29 {
    namespace {
        std::uint32_t read_u32_le(const std::uint8_t *p) {
            return static_cast<std::uint32_t>(p[0])
                | (static_cast<std::uint32_t>(p[1]) << 8)
                | (static_cast<std::uint32_t>(p[2]) << 16)
                | (static_cast<std::uint32_t>(p[3]) << 24);
        }

        std::uint64_t read_value_le(const void *value, const std::size_t width) {
            const auto *bytes = static_cast<const std::uint8_t *>(value);
            std::uint64_t result = 0;
            const std::size_t limit = width > sizeof(result) ? sizeof(result) : width;
            for (std::size_t i = 0; i < limit; ++i) {
                result |= static_cast<std::uint64_t>(bytes[i]) << (i * 8);
            }
            return result;
        }
    }

    parse_result parse_rom_header(const std::uint8_t *data, const std::size_t size) {
        parse_result result{};
        if (!data || size < eka1_rom_header_size) {
            result.error = parse_error::truncated_header;
            return result;
        }

        result.header.restart_vector = read_u32_le(data + 0x7C);
        result.header.rom_base = read_u32_le(data + 0x8C);
        result.header.rom_size = read_u32_le(data + 0x90);
        result.header.rom_root_dir_list = read_u32_le(data + 0x94);
        result.header.kern_data_address = read_u32_le(data + 0x98);
        result.header.kern_limit = read_u32_le(data + 0x9C);

        if (result.header.rom_base != expected_eka1_rom_base) {
            result.error = parse_error::unexpected_rom_base;
            return result;
        }
        if (result.header.rom_size == 0) {
            result.error = parse_error::invalid_rom_size;
            return result;
        }
        if (static_cast<std::size_t>(result.header.rom_size) > size) {
            result.error = parse_error::rom_size_exceeds_file;
            return result;
        }

        const std::uint64_t end = static_cast<std::uint64_t>(result.header.rom_base)
            + static_cast<std::uint64_t>(result.header.rom_size);
        if (end > (static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1ULL)) {
            result.error = parse_error::mapped_range_overflow;
            return result;
        }

        result.ok = true;
        result.error = parse_error::none;
        return result;
    }

    strict_bus::strict_bus(const std::uint8_t *rom_data,
                           const std::size_t rom_size,
                           const std::uint32_t rom_base)
        : rom_data_(rom_data)
        , rom_size_(rom_size)
        , rom_base_(rom_base) {
    }

    bool strict_bus::range_inside_rom(const std::uint32_t address,
                                      const std::size_t width,
                                      std::size_t &offset) const {
        if (!rom_data_ || width == 0 || address < rom_base_) {
            return false;
        }

        const std::uint64_t off64 = static_cast<std::uint64_t>(address) - rom_base_;
        const std::uint64_t end64 = off64 + static_cast<std::uint64_t>(width);
        if (end64 < off64 || end64 > rom_size_) {
            return false;
        }

        offset = static_cast<std::size_t>(off64);
        return true;
    }

    void strict_bus::record_unresolved(const access_kind kind,
                                       const std::size_t width,
                                       const std::uint32_t address,
                                       const std::uint32_t pc,
                                       const std::uint32_t lr,
                                       const std::uint64_t value) {
        if (first_unresolved_) {
            return;
        }

        first_unresolved_ = unresolved_access{kind, width, address, pc, lr, value, 1};
    }

    bool strict_bus::read(const access_kind kind,
                          const std::uint32_t address,
                          void *out,
                          const std::size_t width,
                          const std::uint32_t pc,
                          const std::uint32_t lr) {
        std::size_t offset = 0;
        if (!out || !range_inside_rom(address, width, offset)) {
            record_unresolved(kind, width, address, pc, lr, 0);
            return false;
        }

        std::memcpy(out, rom_data_ + offset, width);
        return true;
    }

    bool strict_bus::write(const std::uint32_t address,
                           const void *value,
                           const std::size_t width,
                           const std::uint32_t pc,
                           const std::uint32_t lr) {
        record_unresolved(access_kind::data_write, width, address, pc, lr,
                          value ? read_value_le(value, width) : 0);
        return false;
    }

    const std::optional<unresolved_access> &strict_bus::first_unresolved() const {
        return first_unresolved_;
    }
}
