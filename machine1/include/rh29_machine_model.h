#pragma once

#include <array>
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

    // MACHINE1-P/Q device evidence proves a deterministic RAM-probe loop:
    // 16-byte probe, restore, R8 += 0x3C, R10--, repeat while R10 != 0.
    // R10 starts at 8 and Q confirms the third address 0x0A000078. MACHINE1-R
    // models the complete eight-island loop while keeping all inter-island gaps
    // unmapped. This still does not claim a contiguous RAM bank.
    static constexpr std::uint32_t candidate_ram_probe_base = 0x0A000000u;
    static constexpr std::size_t candidate_ram_probe_size = 16u;
    static constexpr std::uint32_t candidate_ram_probe_stride = 0x0000003Cu;
    static constexpr std::size_t candidate_ram_probe_loop_windows = 8u;

    // MACHINE1-R device evidence: after all eight probe iterations complete,
    // R8 advances once more to 0x0A0001E0. The caller passes R0=that address
    // and R1=0x20 to the next routine, whose first observed access is a 32-bit
    // read at +8. MACHINE1-S exposes only this exact 0x20-byte candidate
    // workspace, zero-seeded and mutable, without claiming a larger mapping.
    static constexpr std::uint32_t candidate_post_probe_workspace_base =
        candidate_ram_probe_base + candidate_ram_probe_stride
            * static_cast<std::uint32_t>(candidate_ram_probe_loop_windows);
    static constexpr std::size_t candidate_post_probe_workspace_size = 0x20u;

    // MACHINE1-S device evidence: after leaving the post-probe workspace,
    // bootstrap copies the first ROM vector word (0xEA0000C9) to address 0.
    // MACHINE1-W proves the copy loop continues with an exact 32-bit write at
    // 0x20. MACHINE1-X advances only that one newly observed word: nine words
    // total. 0x24 and above remain fail-closed so the huge R2 count cannot turn
    // this evidence into an invented broad low-memory mapping.
    static constexpr std::uint32_t low_vector_shadow_base = 0x00000000u;
    static constexpr std::size_t low_vector_shadow_size = 9u * sizeof(std::uint32_t);
    static constexpr std::size_t low_vector_shadow_write_width = sizeof(std::uint32_t);

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

    // MACHINE1-F device evidence. 0x90 is strongly consistent with entering
    // flash identification/autoselect mode, but G still accepts only this
    // exact observed bus transaction and does not synthesize an ID response.
    static constexpr std::uint32_t observed_flash_id_entry_address = 0x0200AAAAu;
    static constexpr std::size_t observed_flash_id_entry_width = 2u;
    static constexpr std::uint16_t observed_flash_id_entry_value = 0x0090u;

    // MACHINE1-H models only the documented AMD RH-29 reference variant.
    // After the observed ID-entry write, a 16-bit read at bank base returns
    // AMD manufacturer ID 0x0001. No other flash read is synthesized.
    static constexpr std::uint32_t amd_reference_manufacturer_id_address = second_flash_base;
    static constexpr std::size_t amd_reference_manufacturer_id_width = 2u;
    static constexpr std::uint16_t amd_reference_manufacturer_id = 0x0001u;

    // RH-29 flashing logs identify the second flash as Amd 29BDS064J with
    // composite ID 0001:277E. MACHINE1-I exposes only the first Device-ID word.
    static constexpr std::uint32_t amd_reference_device_id_address = second_flash_base + 0x2u;
    static constexpr std::size_t amd_reference_device_id_width = 2u;
    static constexpr std::uint16_t amd_reference_device_id = 0x277Eu;

    // MACHINE1-I device evidence plus AMD command-set documentation:
    // 0xF0 exits Autoselect and returns the flash to Read Array mode.
    static constexpr std::uint32_t observed_flash_id_exit_address = second_flash_base;
    static constexpr std::size_t observed_flash_id_exit_width = 2u;
    static constexpr std::uint16_t observed_flash_id_exit_value = 0x00F0u;

    // MACHINE1-J device evidence matches the x16 AMD unlock cycle 1:
    // word address 0x555 => byte offset 0xAAA, data 0x00AA.
    static constexpr std::uint32_t observed_flash_unlock1_address = second_flash_base + 0x00000AAAu;
    static constexpr std::size_t observed_flash_unlock1_width = 2u;
    static constexpr std::uint16_t observed_flash_unlock1_value = 0x00AAu;

    // MACHINE1-K device evidence matches AMD x16 unlock cycle 2:
    // word address 0x2AA => byte offset 0x554, data 0x0055.
    static constexpr std::uint32_t observed_flash_unlock2_address = second_flash_base + 0x00000554u;
    static constexpr std::size_t observed_flash_unlock2_width = 2u;
    static constexpr std::uint16_t observed_flash_unlock2_value = 0x0055u;

    // MACHINE1-L device evidence: after AA/55 unlock stages, firmware writes
    // command 0x0090 back to word address 0x555 (byte offset 0xAAA).
    static constexpr std::uint32_t observed_flash_unlock_autoselect_address = observed_flash_unlock1_address;
    static constexpr std::size_t observed_flash_unlock_autoselect_width = 2u;
    static constexpr std::uint16_t observed_flash_unlock_autoselect_value = 0x0090u;

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

    struct ram_write_trace_entry {
        std::uint32_t address = 0;
        std::uint32_t pc = 0;
        std::uint32_t lr = 0;
        std::uint32_t width_bits = 0;
        std::uint64_t value = 0;
    };

    static constexpr std::size_t ram_write_trace_capacity = 16u;

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
        std::size_t ram_write_trace_count() const;
        const std::array<ram_write_trace_entry, ram_write_trace_capacity> &ram_write_trace() const;
        std::uint64_t observed_mmio_write_count() const;
        std::uint64_t observed_flash_command_count() const;
        std::uint64_t observed_flash_id_entry_count() const;
        std::uint64_t amd_reference_manufacturer_read_count() const;
        std::uint64_t amd_reference_device_read_count() const;
        std::uint64_t observed_flash_id_exit_count() const;
        bool flash_autoselect_active() const;
        std::uint64_t observed_flash_unlock1_count() const;
        std::uint64_t observed_flash_unlock2_count() const;
        std::uint64_t observed_flash_unlock_autoselect_count() const;
        std::uint32_t flash_unlock_stage() const;
        std::uint64_t candidate_ram_probe_read_count() const;
        std::uint64_t candidate_ram_probe_write_count() const;
        std::uint64_t candidate_post_probe_workspace_read_count() const;
        std::uint64_t candidate_post_probe_workspace_write_count() const;
        std::uint64_t low_vector_shadow_read_count() const;
        std::uint64_t low_vector_shadow_write_count() const;

    private:
        bool range_inside_rom_mapping(std::uint32_t address, std::size_t width, std::size_t &offset) const;
        bool range_inside_ram(std::uint32_t address, std::size_t width, std::size_t &offset) const;
        bool range_inside_candidate_ram_probe(std::uint32_t address, std::size_t width,
                                              std::size_t &offset) const;
        bool range_inside_candidate_post_probe_workspace(std::uint32_t address,
                                                        std::size_t width,
                                                        std::size_t &offset) const;
        bool range_inside_low_vector_shadow(std::uint32_t address,
                                            std::size_t width,
                                            std::size_t &offset) const;
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
        std::array<ram_write_trace_entry, ram_write_trace_capacity> ram_write_trace_{};
        std::size_t ram_write_trace_count_ = 0;
        std::uint64_t observed_mmio_write_count_ = 0;
        std::uint64_t observed_flash_command_count_ = 0;
        std::uint64_t observed_flash_id_entry_count_ = 0;
        std::uint64_t amd_reference_manufacturer_read_count_ = 0;
        std::uint64_t amd_reference_device_read_count_ = 0;
        std::uint64_t observed_flash_id_exit_count_ = 0;
        bool flash_autoselect_active_ = false;
        std::uint64_t observed_flash_unlock1_count_ = 0;
        std::uint64_t observed_flash_unlock2_count_ = 0;
        std::uint64_t observed_flash_unlock_autoselect_count_ = 0;
        std::uint32_t flash_unlock_stage_ = 0;
        std::vector<std::uint8_t> candidate_ram_probe_data_{};
        std::uint64_t candidate_ram_probe_read_count_ = 0;
        std::uint64_t candidate_ram_probe_write_count_ = 0;
        std::vector<std::uint8_t> candidate_post_probe_workspace_data_{};
        std::uint64_t candidate_post_probe_workspace_read_count_ = 0;
        std::uint64_t candidate_post_probe_workspace_write_count_ = 0;
        std::vector<std::uint8_t> low_vector_shadow_data_{};
        std::uint64_t low_vector_shadow_read_count_ = 0;
        std::uint64_t low_vector_shadow_write_count_ = 0;
        std::optional<unresolved_access> first_unresolved_{};
    };
}
