#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eka2l1::machine::rh29 {
    static constexpr std::uint32_t expected_eka1_rom_base = 0x50000000u;
    static constexpr std::size_t eka1_rom_header_size = 512u;
    static constexpr std::uint32_t cold_reset_pc = 0x00000000u;
    static constexpr std::uint32_t candidate_sdram_base = 0x08000000u;
    static constexpr std::size_t candidate_sdram_size = 16u * 1024u * 1024u;

    // MACHINE1-D device evidence. Hardware identity is intentionally unknown:
    // allow only this exact observed write, never the surrounding MMIO range.
    static constexpr std::uint32_t observed_mmio_write_address = 0x0C150004u;
    static constexpr std::size_t observed_mmio_write_width = 2u;
    static constexpr std::uint16_t observed_mmio_write_value = 0x0080u;

    // RH-29 flashing evidence identifies this as the second 64-Mbit flash window.
    // MACHINE1-F still does not map/read that window: only this exact observed
    // 16-bit write is accepted as a flash-command probe.
    static constexpr std::uint32_t second_flash_base = 0x02000000u;
    static constexpr std::size_t second_flash_size = 8u * 1024u * 1024u;
    static constexpr std::uint32_t observed_flash_command_address = second_flash_base;
    static constexpr std::size_t observed_flash_command_width = 2u;
    static constexpr std::uint16_t observed_flash_command_value = 0x00FFu;

    enum class parse_error {
        none = 0,
        truncated_header,
        unexpected_rom_base,
        invalid_rom_size,
        rom_size_exceeds_file,
        mapped_range_overflow
    };

    struct rom_header_info {
        std::uint32_t restart_vector = 0;
        std::uint32_t rom_base = 0;
        std::uint32_t rom_size = 0;
        std::uint32_t rom_root_dir_list = 0;
        std::uint32_t kern_data_address = 0;
        std::uint32_t kern_limit = 0;
    };

    struct parse_result {
        bool ok = false;
        parse_error error = parse_error::none;
        rom_header_info header{};
    };

    parse_result parse_rom_header(const std::uint8_t *data, std::size_t size);

    enum class access_kind {
        code_read,
        data_read,
        data_write
    };

    enum class unresolved_cause {
        unmapped,
        rom_write,
        ram_uninitialized
    };

    struct unresolved_access {
        access_kind kind = access_kind::data_read;
        std::size_t width = 0;
        std::uint32_t address = 0;
        std::uint32_t pc = 0;
        std::uint32_t lr = 0;
        std::uint64_t value = 0;
        std::uint64_t count = 0;
        unresolved_cause cause = unresolved_cause::unmapped;
    };

    class strict_bus {
    public:
        strict_bus(const std::uint8_t *rom_data,
                   std::size_t rom_size,
                   std::uint32_t rom_base,
                   std::optional<std::uint32_t> read_alias_base = std::nullopt,
                   std::optional<std::uint32_t> ram_base = std::nullopt,
                   std::size_t ram_size = 0);

        bool read(access_kind kind, std::uint32_t address, void *out, std::size_t width,
                  std::uint32_t pc, std::uint32_t lr);
        bool write(std::uint32_t address, const void *value, std::size_t width,
                   std::uint32_t pc, std::uint32_t lr);

        const std::optional<unresolved_access> &first_unresolved() const;
        std::uint64_t ram_write_count() const;
        std::size_t ram_initialized_bytes() const;
        std::uint64_t observed_mmio_write_count() const;
        std::uint64_t observed_flash_command_count() const;

    private:
        bool range_inside_rom_mapping(std::uint32_t address, std::size_t width, std::size_t &offset) const;
        bool range_inside_ram(std::uint32_t address, std::size_t width, std::size_t &offset) const;
        void record_unresolved(access_kind kind, std::size_t width, std::uint32_t address,
                               std::uint32_t pc, std::uint32_t lr, std::uint64_t value,
                               unresolved_cause cause);

        const std::uint8_t *rom_data_ = nullptr;
        std::size_t rom_size_ = 0;
        std::uint32_t rom_base_ = 0;
        std::optional<std::uint32_t> read_alias_base_{};
        std::optional<std::uint32_t> ram_base_{};
        std::vector<std::uint8_t> ram_data_{};
        std::vector<std::uint8_t> ram_initialized_{};
        std::uint64_t ram_write_count_ = 0;
        std::size_t ram_initialized_bytes_ = 0;
        std::uint64_t observed_mmio_write_count_ = 0;
        std::uint64_t observed_flash_command_count_ = 0;
        std::optional<unresolved_access> first_unresolved_{};
    };
}
