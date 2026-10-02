#include "rh29_machine_model.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

using namespace eka2l1::machine::rh29;

static void put_u32(std::vector<std::uint8_t> &buf, std::size_t off, std::uint32_t v) {
    buf[off + 0] = static_cast<std::uint8_t>(v & 0xff);
    buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    buf[off + 2] = static_cast<std::uint8_t>((v >> 16) & 0xff);
    buf[off + 3] = static_cast<std::uint8_t>((v >> 24) & 0xff);
}

static std::vector<std::uint8_t> valid_rom(std::size_t size = 0x1000) {
    std::vector<std::uint8_t> rom(size, 0);
    put_u32(rom, 0x7C, 0x00000000);
    put_u32(rom, 0x8C, 0x50000000);
    put_u32(rom, 0x90, static_cast<std::uint32_t>(size));
    put_u32(rom, 0x94, 0x50000200);
    put_u32(rom, 0x98, 0x80001000);
    put_u32(rom, 0x9C, 0x80002000);
    return rom;
}

static void test_parse_valid_header() {
    auto rom = valid_rom();
    const auto result = parse_rom_header(rom.data(), rom.size());
    assert(result.ok);
    assert(result.header.restart_vector == 0x00000000);
    assert(result.header.rom_base == 0x50000000);
    assert(result.header.rom_size == rom.size());
    assert(result.header.rom_root_dir_list == 0x50000200);
    assert(result.header.kern_data_address == 0x80001000);
    assert(result.header.kern_limit == 0x80002000);
}

static void test_parse_rejects_short_header() {
    std::array<std::uint8_t, 511> short_rom{};
    const auto result = parse_rom_header(short_rom.data(), short_rom.size());
    assert(!result.ok);
    assert(result.error == parse_error::truncated_header);
}

static void test_parse_rejects_wrong_base() {
    auto rom = valid_rom();
    put_u32(rom, 0x8C, 0x60000000);
    const auto result = parse_rom_header(rom.data(), rom.size());
    assert(!result.ok);
    assert(result.error == parse_error::unexpected_rom_base);
}

static void test_parse_rejects_zero_size() {
    auto rom = valid_rom();
    put_u32(rom, 0x90, 0);
    const auto result = parse_rom_header(rom.data(), rom.size());
    assert(!result.ok);
    assert(result.error == parse_error::invalid_rom_size);
}

static void test_parse_rejects_size_beyond_file() {
    auto rom = valid_rom();
    put_u32(rom, 0x90, static_cast<std::uint32_t>(rom.size() + 1));
    const auto result = parse_rom_header(rom.data(), rom.size());
    assert(!result.ok);
    assert(result.error == parse_error::rom_size_exceeds_file);
}

static void test_parse_rejects_32bit_range_overflow() {
    auto header = valid_rom(0x1000);
    put_u32(header, 0x90, 0xF0000000);
    const auto result = parse_rom_header(header.data(), std::numeric_limits<std::size_t>::max());
    assert(!result.ok);
    assert(result.error == parse_error::mapped_range_overflow);
}

static void test_bus_reads_little_endian_inside_rom() {
    auto rom = valid_rom();
    rom[0x300] = 0x11;
    rom[0x301] = 0x22;
    rom[0x302] = 0x33;
    rom[0x303] = 0x44;
    rom[0x304] = 0x55;
    rom[0x305] = 0x66;
    rom[0x306] = 0x77;
    rom[0x307] = 0x88;

    strict_bus bus(rom.data(), rom.size(), 0x50000000);
    std::uint8_t u8 = 0;
    std::uint16_t u16 = 0;
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
    assert(bus.read(access_kind::data_read, 0x50000300, &u8, sizeof(u8), 0x50000100, 0));
    assert(bus.read(access_kind::data_read, 0x50000300, &u16, sizeof(u16), 0x50000100, 0));
    assert(bus.read(access_kind::data_read, 0x50000300, &u32, sizeof(u32), 0x50000100, 0));
    assert(bus.read(access_kind::code_read, 0x50000300, &u64, sizeof(u64), 0x50000100, 0));
    assert(u8 == 0x11);
    assert(u16 == 0x2211);
    assert(u32 == 0x44332211);
    assert(u64 == 0x8877665544332211ULL);
    assert(!bus.first_unresolved().has_value());
}

static void test_bus_reset_alias_reads_same_rom_bytes() {
    auto rom = valid_rom();
    rom[0] = 0x78;
    rom[1] = 0x56;
    rom[2] = 0x34;
    rom[3] = 0x12;

    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc);
    std::uint32_t canonical = 0;
    std::uint32_t aliased = 0;
    assert(bus.read(access_kind::code_read, 0x50000000, &canonical, sizeof(canonical), 0x50000000, 0));
    assert(bus.read(access_kind::code_read, cold_reset_pc, &aliased, sizeof(aliased), cold_reset_pc, 0));
    assert(canonical == 0x12345678u);
    assert(aliased == canonical);
    assert(!bus.first_unresolved().has_value());
}

static void test_bus_rejects_cross_rom_end() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000);
    std::uint32_t out = 0;
    assert(!bus.read(access_kind::data_read,
                     0x50000000u + static_cast<std::uint32_t>(rom.size()) - 2,
                     &out, sizeof(out), 0x50000120, 0x50000124));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->kind == access_kind::data_read);
    assert(bus.first_unresolved()->width == 4);
}

static void test_bus_records_rom_write() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000);
    const std::uint32_t value = 0xAABBCCDD;
    assert(!bus.write(0x50000300, &value, sizeof(value), 0x50000130, 0x50000134));
    const auto &u = bus.first_unresolved();
    assert(u.has_value());
    assert(u->kind == access_kind::data_write);
    assert(u->width == 4);
    assert(u->address == 0x50000300);
    assert(u->pc == 0x50000130);
    assert(u->lr == 0x50000134);
    assert(u->value == 0xAABBCCDD);
    assert(u->count == 1);
    assert(u->cause == unresolved_cause::rom_write);
}

static void test_candidate_sdram_write_then_read_is_tracked() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc, candidate_sdram_base, 0x100);
    const std::uint32_t value = 0x7037F800u;
    assert(bus.write(candidate_sdram_base, &value, sizeof(value), 0x00000B2C, 0x00000344));
    assert(bus.ram_write_count() == 1);
    assert(bus.ram_initialized_bytes() == sizeof(value));
    assert(!bus.first_unresolved().has_value());

    std::uint32_t out = 0;
    assert(bus.read(access_kind::data_read, candidate_sdram_base, &out, sizeof(out), 0x00000B30, 0x00000344));
    assert(out == value);
    assert(!bus.first_unresolved().has_value());
}

static void test_candidate_sdram_uninitialized_read_stops() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc, candidate_sdram_base, 0x100);
    std::uint32_t out = 0;
    assert(!bus.read(access_kind::data_read, candidate_sdram_base + 4, &out, sizeof(out), 0x00000B30, 0x00000344));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->cause == unresolved_cause::ram_uninitialized);
}

static void test_candidate_sdram_write_trace_records_exact_bus_transactions() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    const std::uint32_t first = 0x7037F800u;
    const std::uint32_t second = 0x11223344u;
    assert(bus.write(candidate_sdram_base, &first, sizeof(first), 0x00000B2Cu, 0x00000B30u));
    assert(bus.write(candidate_sdram_base + 4u, &second, sizeof(second), 0x00000B40u, 0x00000B44u));
    assert(bus.ram_write_trace_count() == 2u);
    const auto &trace = bus.ram_write_trace();
    assert(trace[0].address == candidate_sdram_base);
    assert(trace[0].pc == 0x00000B2Cu);
    assert(trace[0].lr == 0x00000B30u);
    assert(trace[0].width_bits == 32u);
    assert(trace[0].value == first);
    assert(trace[1].address == candidate_sdram_base + 4u);
    assert(trace[1].value == second);
}

static void test_exact_observed_mmio_write_is_the_only_allowlisted_mmio() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    const std::uint16_t exact = observed_mmio_write_value;
    assert(bus.write(observed_mmio_write_address, &exact, sizeof(exact), 0x00000B68, 0x00000344));
    assert(bus.observed_mmio_write_count() == 1);
    assert(!bus.first_unresolved().has_value());

    const std::uint16_t wrong = 0x0081u;
    assert(!bus.write(observed_mmio_write_address, &wrong, sizeof(wrong), 0x00000B68, 0x00000344));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->address == observed_mmio_write_address);
    assert(bus.first_unresolved()->value == wrong);
}

static void test_observed_mmio_neighbor_is_not_mapped() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint16_t exact = observed_mmio_write_value;
    assert(!bus.write(observed_mmio_write_address + 2, &exact, sizeof(exact), 0x00000B68, 0x00000344));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->cause == unresolved_cause::unmapped);
}

static void test_exact_observed_flash_command_is_allowlisted_only_at_fl1_base() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    const std::uint16_t exact = observed_flash_command_value;
    assert(bus.write(observed_flash_command_address, &exact, sizeof(exact), 0x00000984, 0x00000AFC));
    assert(bus.observed_flash_command_count() == 1);
    assert(!bus.first_unresolved().has_value());

    const std::uint16_t wrong = 0x00F1u;
    assert(!bus.write(observed_flash_command_address, &wrong, sizeof(wrong), 0x00000984, 0x00000AFC));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->address == observed_flash_command_address);
    assert(bus.first_unresolved()->value == wrong);
}

static void test_second_flash_window_is_not_generically_mapped() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    const std::uint16_t exact = observed_flash_command_value;
    assert(!bus.write(observed_flash_command_address + 2, &exact, sizeof(exact), 0x00000984, 0x00000AFC));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->cause == unresolved_cause::unmapped);
}

static void test_exact_observed_flash_id_entry_write_is_allowlisted() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint16_t exact = observed_flash_id_entry_value;
    assert(bus.write(observed_flash_id_entry_address, &exact, sizeof(exact),
                     0x00000994, 0x00000AFC));
    assert(bus.observed_flash_id_entry_count() == 1);
    assert(!bus.first_unresolved().has_value());
}

static void test_flash_id_entry_allowlist_is_exact() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint16_t wrong = 0x0091u;
    assert(!bus.write(observed_flash_id_entry_address, &wrong, sizeof(wrong),
                      0x00000994, 0x00000AFC));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->address == observed_flash_id_entry_address);
    assert(bus.first_unresolved()->value == wrong);
}

static void test_amd_reference_manufacturer_id_requires_id_entry() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    std::uint16_t out = 0;
    assert(!bus.read(access_kind::data_read, amd_reference_manufacturer_id_address,
                     &out, sizeof(out), 0x0000099C, 0x00000AFC));
    assert(bus.first_unresolved().has_value());
}

static void test_amd_reference_manufacturer_id_read_after_id_entry() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    const std::uint16_t entry = observed_flash_id_entry_value;
    assert(bus.write(observed_flash_id_entry_address, &entry, sizeof(entry),
                     0x00000994, 0x00000AFC));
    std::uint16_t out = 0;
    assert(bus.read(access_kind::data_read, amd_reference_manufacturer_id_address,
                    &out, sizeof(out), 0x0000099C, 0x00000AFC));
    assert(out == amd_reference_manufacturer_id);
    assert(bus.amd_reference_manufacturer_read_count() == 1);
    assert(!bus.first_unresolved().has_value());

    std::uint16_t other = 0;
    assert(!bus.read(access_kind::data_read, amd_reference_manufacturer_id_address + 4,
                     &other, sizeof(other), 0x000009AC, 0x00000AFC));
    assert(bus.first_unresolved().has_value());
}

static void test_amd_reference_device_id_read_after_id_entry() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    const std::uint16_t entry = observed_flash_id_entry_value;
    assert(bus.write(observed_flash_id_entry_address, &entry, sizeof(entry),
                     0x00000994, 0x00000AFC));

    std::uint16_t manufacturer = 0;
    assert(bus.read(access_kind::data_read, amd_reference_manufacturer_id_address,
                    &manufacturer, sizeof(manufacturer), 0x0000099C, 0x00000AFC));
    assert(manufacturer == amd_reference_manufacturer_id);

    std::uint16_t device = 0;
    assert(bus.read(access_kind::data_read, amd_reference_device_id_address,
                    &device, sizeof(device), 0x000009A4, 0x00000AFC));
    assert(device == amd_reference_device_id);
    assert(bus.amd_reference_device_read_count() == 1);
    assert(!bus.first_unresolved().has_value());
}

static void test_amd_reference_device_id_read_is_exact_width() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint16_t entry = observed_flash_id_entry_value;
    assert(bus.write(observed_flash_id_entry_address, &entry, sizeof(entry),
                     0x00000994, 0x00000AFC));
    std::uint32_t wide = 0;
    assert(!bus.read(access_kind::data_read, amd_reference_device_id_address,
                     &wide, sizeof(wide), 0x000009A4, 0x00000AFC));
    assert(bus.first_unresolved().has_value());
}

static void test_flash_id_exit_f0_disables_autoselect_state() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    const std::uint16_t entry = observed_flash_id_entry_value;
    assert(bus.write(observed_flash_id_entry_address, &entry, sizeof(entry),
                     0x00000994, 0x00000AFC));
    assert(bus.flash_autoselect_active());

    std::uint16_t manufacturer = 0;
    assert(bus.read(access_kind::data_read, amd_reference_manufacturer_id_address,
                    &manufacturer, sizeof(manufacturer), 0x0000099C, 0x00000AFC));
    assert(manufacturer == amd_reference_manufacturer_id);

    const std::uint16_t exit = observed_flash_id_exit_value;
    assert(bus.write(observed_flash_id_exit_address, &exit, sizeof(exit),
                     0x000009DC, 0x00000AFC));
    assert(bus.observed_flash_id_exit_count() == 1);
    assert(!bus.flash_autoselect_active());
    assert(!bus.first_unresolved().has_value());

    std::uint16_t after = 0;
    assert(!bus.read(access_kind::data_read, amd_reference_manufacturer_id_address,
                     &after, sizeof(after), 0x000009E0, 0x00000AFC));
    assert(bus.first_unresolved().has_value());
}

static void test_flash_id_exit_allowlist_is_exact() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint16_t wrong = 0x00F1u;
    assert(!bus.write(observed_flash_id_exit_address, &wrong, sizeof(wrong),
                      0x000009DC, 0x00000AFC));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->value == wrong);
}

static void test_flash_unlock_cycle1_matches_observed_x16_transaction() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    const std::uint16_t unlock1 = observed_flash_unlock1_value;
    assert(bus.write(observed_flash_unlock1_address, &unlock1, sizeof(unlock1),
                     0x000009EC, 0x00000AFC));
    assert(bus.observed_flash_unlock1_count() == 1);
    assert(bus.flash_unlock_stage() == 1);
    assert(!bus.flash_autoselect_active());
    assert(!bus.first_unresolved().has_value());
}

static void test_flash_unlock_cycle1_allowlist_is_exact() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint16_t wrong = 0x00ABu;
    assert(!bus.write(observed_flash_unlock1_address, &wrong, sizeof(wrong),
                      0x000009EC, 0x00000AFC));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->address == observed_flash_unlock1_address);
    assert(bus.first_unresolved()->value == wrong);
}

static void test_flash_f0_resets_unlock_stage() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint16_t unlock1 = observed_flash_unlock1_value;
    assert(bus.write(observed_flash_unlock1_address, &unlock1, sizeof(unlock1),
                     0x000009EC, 0x00000AFC));
    assert(bus.flash_unlock_stage() == 1);
    const std::uint16_t reset = observed_flash_id_exit_value;
    assert(bus.write(observed_flash_id_exit_address, &reset, sizeof(reset),
                     0x000009DC, 0x00000AFC));
    assert(bus.flash_unlock_stage() == 0);
}

static void test_flash_unlock_cycle2_requires_stage1_and_matches_observed_transaction() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    const std::uint16_t unlock2 = observed_flash_unlock2_value;
    assert(!bus.write(observed_flash_unlock2_address, &unlock2, sizeof(unlock2),
                      0x000009F8, 0x00000AFC));
    assert(bus.first_unresolved().has_value());

    strict_bus staged(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                      candidate_sdram_base, 0x100);
    const std::uint16_t unlock1 = observed_flash_unlock1_value;
    assert(staged.write(observed_flash_unlock1_address, &unlock1, sizeof(unlock1),
                        0x000009EC, 0x00000AFC));
    assert(staged.flash_unlock_stage() == 1);
    assert(staged.write(observed_flash_unlock2_address, &unlock2, sizeof(unlock2),
                        0x000009F8, 0x00000AFC));
    assert(staged.observed_flash_unlock2_count() == 1);
    assert(staged.flash_unlock_stage() == 2);
    assert(!staged.first_unresolved().has_value());
}

static void test_flash_unlock_cycle2_allowlist_is_exact() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint16_t unlock1 = observed_flash_unlock1_value;
    assert(bus.write(observed_flash_unlock1_address, &unlock1, sizeof(unlock1),
                     0x000009EC, 0x00000AFC));
    const std::uint16_t wrong = 0x0056u;
    assert(!bus.write(observed_flash_unlock2_address, &wrong, sizeof(wrong),
                      0x000009F8, 0x00000AFC));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->address == observed_flash_unlock2_address);
    assert(bus.first_unresolved()->value == wrong);
}

static void test_unlock_sequence_enters_autoselect_only_after_stage2() {
    auto rom = valid_rom();

    strict_bus early(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                     candidate_sdram_base, 0x100);
    const std::uint16_t cmd = observed_flash_unlock_autoselect_value;
    assert(!early.write(observed_flash_unlock_autoselect_address, &cmd, sizeof(cmd),
                        0x00000A04, 0x00000AFC));
    assert(early.first_unresolved().has_value());

    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint16_t unlock1 = observed_flash_unlock1_value;
    const std::uint16_t unlock2 = observed_flash_unlock2_value;
    assert(bus.write(observed_flash_unlock1_address, &unlock1, sizeof(unlock1),
                     0x000009EC, 0x00000AFC));
    assert(bus.write(observed_flash_unlock2_address, &unlock2, sizeof(unlock2),
                     0x000009F8, 0x00000AFC));
    assert(bus.flash_unlock_stage() == 2);
    assert(!bus.flash_autoselect_active());

    assert(bus.write(observed_flash_unlock_autoselect_address, &cmd, sizeof(cmd),
                     0x00000A04, 0x00000AFC));
    assert(bus.observed_flash_unlock_autoselect_count() == 1);
    assert(bus.flash_unlock_stage() == 0);
    assert(bus.flash_autoselect_active());
    assert(!bus.first_unresolved().has_value());

    std::uint16_t manufacturer = 0;
    assert(bus.read(access_kind::data_read, amd_reference_manufacturer_id_address,
                    &manufacturer, sizeof(manufacturer), 0x00000A0C, 0x00000AFC));
    assert(manufacturer == amd_reference_manufacturer_id);
}

static void test_unlock_autoselect_command_allowlist_is_exact() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint16_t unlock1 = observed_flash_unlock1_value;
    const std::uint16_t unlock2 = observed_flash_unlock2_value;
    assert(bus.write(observed_flash_unlock1_address, &unlock1, sizeof(unlock1),
                     0x000009EC, 0x00000AFC));
    assert(bus.write(observed_flash_unlock2_address, &unlock2, sizeof(unlock2),
                     0x000009F8, 0x00000AFC));
    const std::uint16_t wrong = 0x0091u;
    assert(!bus.write(observed_flash_unlock_autoselect_address, &wrong, sizeof(wrong),
                      0x00000A04, 0x00000AFC));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->address == observed_flash_unlock_autoselect_address);
    assert(bus.first_unresolved()->value == wrong);
}

static void test_candidate_probe_window_reads_zero() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc, candidate_sdram_base, 0x100);
    std::uint32_t out = 0xFFFFFFFFu;
    assert(bus.read(access_kind::data_read, candidate_ram_probe_base, &out, sizeof(out), 0x00002664, 0x000024C4));
    assert(out == 0);
    assert(bus.candidate_ram_probe_read_count() == 1);
}

static void test_eight_step_candidate_probe_loop_is_exact_and_sparse() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc, candidate_sdram_base, 0x100);

    for (std::uint32_t i = 0; i < candidate_ram_probe_loop_windows; ++i) {
        const std::uint32_t base = candidate_ram_probe_base + candidate_ram_probe_stride * i;
        std::uint32_t out = 0xFFFFFFFFu;
        assert(bus.read(access_kind::data_read, base, &out, sizeof(out), 0x00002664, 0x000024C4));
        assert(out == 0);

        const std::uint32_t value = 0xA5A50000u | i;
        assert(bus.write(base + 12, &value, sizeof(value), 0x000026E0, 0x000024E4));
        out = 0;
        assert(bus.read(access_kind::data_read, base + 12, &out, sizeof(out), 0x000026E4, 0x000024E4));
        assert(out == value);
    }

    std::uint32_t gap = 0;
    assert(!bus.read(access_kind::data_read, candidate_ram_probe_base + 16,
                     &gap, sizeof(gap), 0x00002680, 0x000024C4));
    assert(bus.first_unresolved().has_value());
}

static void test_y_observed_bootstrap_copy_is_exact_gated_and_initialized_only() {
    static_assert(candidate_bootstrap_copy_source == 0x00000744u);
    static_assert(candidate_bootstrap_copy_base == 0x0A000000u);
    static_assert(candidate_bootstrap_copy_size == 0x108u);
    static_assert(candidate_bootstrap_copy_write_width == 4u);
    static_assert(candidate_bootstrap_copy_pc == 0x00002344u);
    static_assert(candidate_bootstrap_copy_lr == 0x000003C8u);

    auto rom = valid_rom();
    strict_bus before(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                      candidate_sdram_base, 0x100);
    std::uint32_t out = 0;
    assert(!before.read(access_kind::data_read, candidate_bootstrap_copy_base + 0x10u,
                        &out, sizeof(out), 0x00002414u, 0x00000384u));
    assert(before.first_unresolved().has_value());

    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint32_t probe_value = 0xAAAAAAAAu;
    assert(bus.write(candidate_ram_probe_base, &probe_value, sizeof(probe_value),
                     0x00002670u, 0x000024C4u));

    for (std::uint32_t i = 0; i < candidate_bootstrap_copy_size / 4u; ++i) {
        const std::uint32_t value = 0x10000000u | i;
        const std::uint32_t address = candidate_bootstrap_copy_base + i * 4u;
        assert(bus.write(address, &value, sizeof(value),
                         candidate_bootstrap_copy_pc, candidate_bootstrap_copy_lr));
    }
    assert(bus.candidate_bootstrap_copy_write_count() == 0x42u);
    assert(bus.candidate_bootstrap_copy_initialized_bytes() == candidate_bootstrap_copy_size);

    out = 0;
    assert(bus.read(access_kind::data_read, candidate_bootstrap_copy_base,
                    &out, sizeof(out), 0x00002350u, 0x000003C8u));
    assert(out == 0x10000000u);
    out = 0;
    assert(bus.read(access_kind::data_read,
                    candidate_bootstrap_copy_base + static_cast<std::uint32_t>(candidate_bootstrap_copy_size - 4u),
                    &out, sizeof(out), 0x00002350u, 0x000003C8u));
    assert(out == (0x10000000u | 0x41u));
    assert(bus.candidate_bootstrap_copy_read_count() == 2u);

    const std::uint32_t extra = 0xDEADBEEFu;
    assert(!bus.write(candidate_bootstrap_copy_base + static_cast<std::uint32_t>(candidate_bootstrap_copy_size),
                      &extra, sizeof(extra), candidate_bootstrap_copy_pc, candidate_bootstrap_copy_lr));
    assert(bus.first_unresolved().has_value());

    strict_bus wrong_pc(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                         candidate_sdram_base, 0x100);
    assert(!wrong_pc.write(candidate_bootstrap_copy_base + 0x10u, &extra, sizeof(extra),
                           candidate_bootstrap_copy_pc + 4u, candidate_bootstrap_copy_lr));
    assert(wrong_pc.first_unresolved().has_value());
}

static void test_ab_observed_post_copy_mutation_is_exact_and_requires_initialized_copy() {
    static_assert(candidate_bootstrap_post_copy_mutation_address == 0x0A000010u);
    static_assert(candidate_bootstrap_post_copy_mutation_width == 4u);
    static_assert(candidate_bootstrap_post_copy_mutation_pc == 0x000011D4u);
    static_assert(candidate_bootstrap_post_copy_mutation_lr == 0x000022C4u);
    static_assert(candidate_bootstrap_post_copy_mutation_instruction == 0xE5803008u);
    static_assert(candidate_bootstrap_post_copy_mutation_value == 0xB2800021u);

    auto rom = valid_rom();

    strict_bus before(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                      candidate_sdram_base, 0x100);
    const std::uint32_t observed = candidate_bootstrap_post_copy_mutation_value;
    assert(!before.write(candidate_bootstrap_post_copy_mutation_address,
                         &observed, sizeof(observed),
                         candidate_bootstrap_post_copy_mutation_pc,
                         candidate_bootstrap_post_copy_mutation_lr));
    assert(before.first_unresolved().has_value());

    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    for (std::uint32_t i = 0; i < candidate_bootstrap_copy_size / 4u; ++i) {
        const std::uint32_t value = 0x10000000u | i;
        const std::uint32_t address = candidate_bootstrap_copy_base + i * 4u;
        assert(bus.write(address, &value, sizeof(value),
                         candidate_bootstrap_copy_pc, candidate_bootstrap_copy_lr));
    }

    std::uint32_t out = 0;
    assert(bus.read(access_kind::data_read,
                    candidate_bootstrap_post_copy_mutation_address,
                    &out, sizeof(out), 0x000011C8u, candidate_bootstrap_nested_stack_push_lr));
    assert(out == 0x10000004u);

    assert(bus.write(candidate_bootstrap_post_copy_mutation_address,
                     &observed, sizeof(observed),
                     candidate_bootstrap_post_copy_mutation_pc,
                     candidate_bootstrap_post_copy_mutation_lr));
    assert(bus.candidate_bootstrap_post_copy_mutation_count() == 1u);

    out = 0;
    assert(bus.read(access_kind::data_read,
                    candidate_bootstrap_post_copy_mutation_address,
                    &out, sizeof(out), 0x000011D8u, candidate_bootstrap_post_copy_mutation_lr));
    assert(out == observed);

    strict_bus wrong_value(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                           candidate_sdram_base, 0x100);
    for (std::uint32_t i = 0; i < candidate_bootstrap_copy_size / 4u; ++i) {
        const std::uint32_t value = 0x20000000u | i;
        const std::uint32_t address = candidate_bootstrap_copy_base + i * 4u;
        assert(wrong_value.write(address, &value, sizeof(value),
                                 candidate_bootstrap_copy_pc, candidate_bootstrap_copy_lr));
    }
    const std::uint32_t bad = 0xDEADBEEFu;
    assert(!wrong_value.write(candidate_bootstrap_post_copy_mutation_address,
                              &bad, sizeof(bad),
                              candidate_bootstrap_post_copy_mutation_pc,
                              candidate_bootstrap_post_copy_mutation_lr));
    assert(wrong_value.first_unresolved().has_value());

    strict_bus wrong_pc(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    for (std::uint32_t i = 0; i < candidate_bootstrap_copy_size / 4u; ++i) {
        const std::uint32_t value = 0x30000000u | i;
        const std::uint32_t address = candidate_bootstrap_copy_base + i * 4u;
        assert(wrong_pc.write(address, &value, sizeof(value),
                              candidate_bootstrap_copy_pc, candidate_bootstrap_copy_lr));
    }
    assert(!wrong_pc.write(candidate_bootstrap_post_copy_mutation_address,
                           &observed, sizeof(observed),
                           candidate_bootstrap_post_copy_mutation_pc + 4u,
                           candidate_bootstrap_post_copy_mutation_lr));
    assert(wrong_pc.first_unresolved().has_value());
}

static void test_af_observed_record_loop_mutation_is_exact_and_initialized_only() {
    static_assert(candidate_bootstrap_record_table_base == 0x0A000008u);
    static_assert(candidate_bootstrap_record_count == 16u);
    static_assert(candidate_bootstrap_record_stride == 0x10u);
    static_assert(candidate_bootstrap_record_control_offset == 0x08u);
    static_assert(candidate_bootstrap_record_loop_mutation_pc == 0x00002220u);
    static_assert(candidate_bootstrap_record_loop_mutation_lr == 0x00002244u);
    static_assert(candidate_bootstrap_record_loop_mutation_instruction == 0xE5803008u);
    static_assert(candidate_bootstrap_record_loop_clear_mask == 0x00000060u);
    static_assert(candidate_bootstrap_record_loop_or_mask == 0x80000020u);

    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    const std::uint32_t record1_control =
        candidate_bootstrap_record_table_base
        + static_cast<std::uint32_t>(candidate_bootstrap_record_stride)
        + static_cast<std::uint32_t>(candidate_bootstrap_record_control_offset);
    const std::uint32_t old_value = 0x32800021u;
    assert(bus.write(record1_control, &old_value, sizeof(old_value),
                     candidate_bootstrap_copy_pc, candidate_bootstrap_copy_lr));

    const std::uint32_t expected =
        (old_value & ~candidate_bootstrap_record_loop_clear_mask)
        | candidate_bootstrap_record_loop_or_mask;
    assert(expected == 0xB2800021u);
    assert(bus.write(record1_control, &expected, sizeof(expected),
                     candidate_bootstrap_record_loop_mutation_pc,
                     candidate_bootstrap_record_loop_mutation_lr));
    assert(bus.candidate_bootstrap_record_loop_mutation_count() == 1u);

    std::uint32_t out = 0;
    assert(bus.read(access_kind::data_read, record1_control, &out, sizeof(out),
                    0x00002224u, candidate_bootstrap_record_loop_mutation_lr));
    assert(out == expected);

    strict_bus before(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                      candidate_sdram_base, 0x100);
    assert(!before.write(record1_control, &expected, sizeof(expected),
                         candidate_bootstrap_record_loop_mutation_pc,
                         candidate_bootstrap_record_loop_mutation_lr));
    assert(before.first_unresolved().has_value());

    strict_bus wrong_slot(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                          candidate_sdram_base, 0x100);
    const std::uint32_t bad_addr = candidate_bootstrap_record_table_base
        + static_cast<std::uint32_t>(candidate_bootstrap_record_stride)
        + 4u;
    assert(wrong_slot.write(bad_addr, &old_value, sizeof(old_value),
                            candidate_bootstrap_copy_pc, candidate_bootstrap_copy_lr));
    assert(!wrong_slot.write(bad_addr, &expected, sizeof(expected),
                             candidate_bootstrap_record_loop_mutation_pc,
                             candidate_bootstrap_record_loop_mutation_lr));
    assert(wrong_slot.first_unresolved().has_value());

    strict_bus low3(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                    candidate_sdram_base, 0x100);
    const std::uint32_t old_low3 = 0x31000023u;
    assert(low3.write(record1_control, &old_low3, sizeof(old_low3),
                      candidate_bootstrap_copy_pc, candidate_bootstrap_copy_lr));
    const std::uint32_t expected_low3 =
        (old_low3 & ~candidate_bootstrap_record_loop_clear_mask)
        | candidate_bootstrap_record_loop_or_mask;
    assert(expected_low3 == 0xB1000023u);
    assert(low3.write(record1_control, &expected_low3, sizeof(expected_low3),
                      candidate_bootstrap_record_loop_mutation_pc,
                      candidate_bootstrap_record_loop_mutation_lr));

    strict_bus low2(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                    candidate_sdram_base, 0x100);
    const std::uint32_t old_low2 = 0x32000022u;
    assert(low2.write(record1_control, &old_low2, sizeof(old_low2),
                      candidate_bootstrap_copy_pc, candidate_bootstrap_copy_lr));
    const std::uint32_t expected_low2 =
        (old_low2 & ~candidate_bootstrap_record_loop_clear_mask)
        | candidate_bootstrap_record_loop_or_mask;
    assert(expected_low2 == 0xB2000022u);
    assert(low2.write(record1_control, &expected_low2, sizeof(expected_low2),
                      candidate_bootstrap_record_loop_mutation_pc,
                      candidate_bootstrap_record_loop_mutation_lr));

    strict_bus wrong_transform(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                               candidate_sdram_base, 0x100);
    assert(wrong_transform.write(record1_control, &old_low3, sizeof(old_low3),
                                 candidate_bootstrap_copy_pc, candidate_bootstrap_copy_lr));
    const std::uint32_t bad_transform = expected_low3 ^ 0x00000100u;
    assert(!wrong_transform.write(record1_control, &bad_transform, sizeof(bad_transform),
                                  candidate_bootstrap_record_loop_mutation_pc,
                                  candidate_bootstrap_record_loop_mutation_lr));
    assert(wrong_transform.first_unresolved().has_value());
}

static void test_z_observed_first_stack_push_is_exact_gated_and_initialized_only() {
    static_assert(candidate_bootstrap_stack_top == 0x0A000FF0u);
    static_assert(candidate_bootstrap_stack_push_base == 0x0A000FD0u);
    static_assert(candidate_bootstrap_stack_push_size == 0x20u);
    static_assert(candidate_bootstrap_stack_write_width == 4u);
    static_assert(candidate_bootstrap_stack_push_pc == 0x000011A8u);
    static_assert(candidate_bootstrap_stack_push_lr == 0x000003C8u);
    static_assert(candidate_bootstrap_stack_push_instruction == 0xE92D47F0u);

    auto rom = valid_rom();
    strict_bus before(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                      candidate_sdram_base, 0x100);
    std::uint32_t out = 0;
    assert(!before.read(access_kind::data_read, candidate_bootstrap_stack_push_base,
                        &out, sizeof(out), 0x000011ACu, candidate_bootstrap_stack_push_lr));
    assert(before.first_unresolved().has_value());

    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    for (std::uint32_t i = 0; i < candidate_bootstrap_stack_push_size / 4u; ++i) {
        const std::uint32_t value = 0xA0000000u | i;
        const std::uint32_t address = candidate_bootstrap_stack_push_base + i * 4u;
        assert(bus.write(address, &value, sizeof(value),
                         candidate_bootstrap_stack_push_pc, candidate_bootstrap_stack_push_lr));
    }
    assert(bus.candidate_bootstrap_stack_write_count() == 8u);
    assert(bus.candidate_bootstrap_stack_initialized_bytes() == candidate_bootstrap_stack_push_size);

    out = 0;
    assert(bus.read(access_kind::data_read, candidate_bootstrap_stack_push_base,
                    &out, sizeof(out), 0x000011ACu, candidate_bootstrap_stack_push_lr));
    assert(out == 0xA0000000u);
    out = 0;
    assert(bus.read(access_kind::data_read, candidate_bootstrap_stack_push_base + 0x1Cu,
                    &out, sizeof(out), 0x000011ACu, candidate_bootstrap_stack_push_lr));
    assert(out == 0xA0000007u);
    assert(bus.candidate_bootstrap_stack_read_count() == 2u);

    const std::uint32_t extra = 0xDEADBEEFu;
    assert(!bus.write(candidate_bootstrap_stack_push_base - 4u, &extra, sizeof(extra),
                      candidate_bootstrap_stack_push_pc, candidate_bootstrap_stack_push_lr));
    assert(bus.first_unresolved().has_value());

    strict_bus wrong_pc(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                         candidate_sdram_base, 0x100);
    assert(!wrong_pc.write(candidate_bootstrap_stack_push_base, &extra, sizeof(extra),
                           candidate_bootstrap_stack_push_pc + 4u, candidate_bootstrap_stack_push_lr));
    assert(wrong_pc.first_unresolved().has_value());
}

static void test_aa_observed_second_stack_push_is_exact_gated_and_initialized_only() {
    static_assert(candidate_bootstrap_nested_stack_top == 0x0A000FBCu);
    static_assert(candidate_bootstrap_nested_stack_push_base == 0x0A000FACu);
    static_assert(candidate_bootstrap_nested_stack_push_size == 0x10u);
    static_assert(candidate_bootstrap_nested_stack_write_width == 4u);
    static_assert(candidate_bootstrap_nested_stack_push_pc == 0x000022A0u);
    static_assert(candidate_bootstrap_nested_stack_push_lr == 0x000011C8u);
    static_assert(candidate_bootstrap_nested_stack_push_instruction == 0xE92D4070u);

    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    for (std::uint32_t i = 0; i < candidate_bootstrap_nested_stack_push_size / 4u; ++i) {
        const std::uint32_t value = 0xB0000000u | i;
        const std::uint32_t address = candidate_bootstrap_nested_stack_push_base + i * 4u;
        assert(bus.write(address, &value, sizeof(value),
                         candidate_bootstrap_nested_stack_push_pc,
                         candidate_bootstrap_nested_stack_push_lr));
    }
    assert(bus.candidate_bootstrap_nested_stack_write_count() == 4u);
    assert(bus.candidate_bootstrap_nested_stack_initialized_bytes()
           == candidate_bootstrap_nested_stack_push_size);

    std::uint32_t out = 0;
    assert(bus.read(access_kind::data_read, candidate_bootstrap_nested_stack_push_base,
                    &out, sizeof(out), 0x000022A4u,
                    candidate_bootstrap_nested_stack_push_lr));
    assert(out == 0xB0000000u);
    assert(bus.candidate_bootstrap_nested_stack_read_count() == 1u);

    const std::uint32_t extra = 0xDEADBEEFu;
    assert(!bus.write(candidate_bootstrap_nested_stack_push_base - 4u,
                      &extra, sizeof(extra), candidate_bootstrap_nested_stack_push_pc,
                      candidate_bootstrap_nested_stack_push_lr));
    assert(bus.first_unresolved().has_value());

    strict_bus wrong_pc(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                         candidate_sdram_base, 0x100);
    assert(!wrong_pc.write(candidate_bootstrap_nested_stack_push_base,
                           &extra, sizeof(extra),
                           candidate_bootstrap_nested_stack_push_pc + 4u,
                           candidate_bootstrap_nested_stack_push_lr));
    assert(wrong_pc.first_unresolved().has_value());
}

static void test_af_observed_fourth_stack_push_reuses_exact_12byte_window() {
    static_assert(candidate_bootstrap_fourth_stack_top == 0x0A000FBCu);
    static_assert(candidate_bootstrap_fourth_stack_push_base == 0x0A000FB0u);
    static_assert(candidate_bootstrap_fourth_stack_push_size == 0x0Cu);
    static_assert(candidate_bootstrap_fourth_stack_push_pc == 0x00002258u);
    static_assert(candidate_bootstrap_fourth_stack_push_lr == 0x000011E8u);
    static_assert(candidate_bootstrap_fourth_stack_push_instruction == 0xE92D4030u);
    static_assert(candidate_bootstrap_fourth_stack_push_base
                  == candidate_bootstrap_third_stack_push_base);
    static_assert(candidate_bootstrap_fourth_stack_top
                  == candidate_bootstrap_third_stack_top);

    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    for (std::uint32_t i = 0; i < candidate_bootstrap_fourth_stack_push_size / 4u; ++i) {
        const std::uint32_t value = 0xC0000000u | i;
        const std::uint32_t address = candidate_bootstrap_fourth_stack_push_base + i * 4u;
        assert(bus.write(address, &value, sizeof(value),
                         candidate_bootstrap_fourth_stack_push_pc,
                         candidate_bootstrap_fourth_stack_push_lr));
    }
    assert(bus.candidate_bootstrap_nested_stack_write_count() == 3u);
    assert(bus.candidate_bootstrap_nested_stack_initialized_bytes()
           == candidate_bootstrap_fourth_stack_push_size);

    strict_bus wrong_lr(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    const std::uint32_t value = 0xDEADBEEFu;
    assert(!wrong_lr.write(candidate_bootstrap_fourth_stack_push_base,
                           &value, sizeof(value),
                           candidate_bootstrap_fourth_stack_push_pc,
                           candidate_bootstrap_fourth_stack_push_lr + 4u));
    assert(wrong_lr.first_unresolved().has_value());

    strict_bus outside(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                       candidate_sdram_base, 0x100);
    assert(!outside.write(candidate_bootstrap_fourth_stack_push_base - 4u,
                          &value, sizeof(value),
                          candidate_bootstrap_fourth_stack_push_pc,
                          candidate_bootstrap_fourth_stack_push_lr));
    assert(outside.first_unresolved().has_value());
}

static void test_ag_observed_fifth_stack_push_adds_exactly_one_new_word() {
    static_assert(candidate_bootstrap_fifth_stack_top == 0x0A000FBCu);
    static_assert(candidate_bootstrap_fifth_stack_push_base == 0x0A000FA8u);
    static_assert(candidate_bootstrap_fifth_stack_push_size == 0x14u);
    static_assert(candidate_bootstrap_fifth_stack_push_pc == 0x00001BB4u);
    static_assert(candidate_bootstrap_fifth_stack_push_lr == 0x00001200u);
    static_assert(candidate_bootstrap_fifth_stack_push_instruction == 0xE92D40F0u);
    static_assert(candidate_bootstrap_nested_stack_extension_base == 0x0A000FA8u);
    static_assert(candidate_bootstrap_nested_stack_extension_size == 4u);

    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    for (std::uint32_t i = 0; i < candidate_bootstrap_fifth_stack_push_size / 4u; ++i) {
        const std::uint32_t value = 0xD0000000u | i;
        const std::uint32_t address = candidate_bootstrap_fifth_stack_push_base + i * 4u;
        assert(bus.write(address, &value, sizeof(value),
                         candidate_bootstrap_fifth_stack_push_pc,
                         candidate_bootstrap_fifth_stack_push_lr));
    }
    assert(bus.candidate_bootstrap_nested_stack_write_count() == 4u);
    assert(bus.candidate_bootstrap_nested_stack_initialized_bytes()
           == candidate_bootstrap_nested_stack_push_size);

    for (std::uint32_t i = 0; i < candidate_bootstrap_fifth_stack_push_size / 4u; ++i) {
        const std::uint32_t address = candidate_bootstrap_fifth_stack_push_base + i * 4u;
        std::uint32_t out = 0;
        assert(bus.read(access_kind::data_read, address, &out, sizeof(out),
                        0x00001BB8u, candidate_bootstrap_fifth_stack_push_lr));
        assert(out == (0xD0000000u | i));
    }

    strict_bus old_callsite(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                            candidate_sdram_base, 0x100);
    const std::uint32_t value = 0x11223344u;
    assert(!old_callsite.write(candidate_bootstrap_nested_stack_extension_base,
                               &value, sizeof(value),
                               candidate_bootstrap_fourth_stack_push_pc,
                               candidate_bootstrap_fourth_stack_push_lr));
    assert(old_callsite.first_unresolved().has_value());

    strict_bus wrong_lr(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    assert(!wrong_lr.write(candidate_bootstrap_nested_stack_extension_base,
                           &value, sizeof(value),
                           candidate_bootstrap_fifth_stack_push_pc,
                           candidate_bootstrap_fifth_stack_push_lr + 4u));
    assert(wrong_lr.first_unresolved().has_value());

    strict_bus below(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                     candidate_sdram_base, 0x100);
    assert(!below.write(candidate_bootstrap_nested_stack_extension_base - 4u,
                        &value, sizeof(value),
                        candidate_bootstrap_fifth_stack_push_pc,
                        candidate_bootstrap_fifth_stack_push_lr));
    assert(below.first_unresolved().has_value());
}

static void test_ah_observed_sixth_stack_push_adds_exactly_two_new_words() {
    static_assert(candidate_bootstrap_sixth_stack_top == 0x0A000FA8u);
    static_assert(candidate_bootstrap_sixth_stack_push_base == 0x0A000FA0u);
    static_assert(candidate_bootstrap_sixth_stack_push_size == 0x08u);
    static_assert(candidate_bootstrap_sixth_stack_push_pc == 0x00001438u);
    static_assert(candidate_bootstrap_sixth_stack_push_lr == 0x00001BE0u);
    static_assert(candidate_bootstrap_sixth_stack_push_instruction == 0xE92D4010u);
    static_assert(candidate_bootstrap_deep_stack_base == 0x0A000FA0u);
    static_assert(candidate_bootstrap_deep_stack_size == 8u);

    auto rom = valid_rom();

    strict_bus before(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                      candidate_sdram_base, 0x100);
    std::uint32_t out = 0;
    assert(!before.read(access_kind::data_read,
                        candidate_bootstrap_deep_stack_base,
                        &out, sizeof(out), 0x0000143Cu,
                        candidate_bootstrap_sixth_stack_push_lr));
    assert(before.first_unresolved().has_value());

    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint32_t r4_value = 0x00000010u;
    const std::uint32_t lr_value = candidate_bootstrap_sixth_stack_push_lr;
    assert(bus.write(candidate_bootstrap_deep_stack_base,
                     &r4_value, sizeof(r4_value),
                     candidate_bootstrap_sixth_stack_push_pc,
                     candidate_bootstrap_sixth_stack_push_lr));
    assert(bus.write(candidate_bootstrap_deep_stack_base + 4u,
                     &lr_value, sizeof(lr_value),
                     candidate_bootstrap_sixth_stack_push_pc,
                     candidate_bootstrap_sixth_stack_push_lr));

    out = 0;
    assert(bus.read(access_kind::data_read,
                    candidate_bootstrap_deep_stack_base,
                    &out, sizeof(out), 0x0000143Cu,
                    candidate_bootstrap_sixth_stack_push_lr));
    assert(out == r4_value);
    out = 0;
    assert(bus.read(access_kind::data_read,
                    candidate_bootstrap_deep_stack_base + 4u,
                    &out, sizeof(out), 0x00001460u,
                    candidate_bootstrap_sixth_stack_push_lr));
    assert(out == lr_value);

    strict_bus old_ah(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                      candidate_sdram_base, 0x100);
    assert(!old_ah.write(candidate_bootstrap_deep_stack_base,
                         &r4_value, sizeof(r4_value),
                         candidate_bootstrap_fifth_stack_push_pc,
                         candidate_bootstrap_fifth_stack_push_lr));
    assert(old_ah.first_unresolved().has_value());

    strict_bus wrong_lr(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    assert(!wrong_lr.write(candidate_bootstrap_deep_stack_base,
                           &r4_value, sizeof(r4_value),
                           candidate_bootstrap_sixth_stack_push_pc,
                           candidate_bootstrap_sixth_stack_push_lr + 4u));
    assert(wrong_lr.first_unresolved().has_value());

    strict_bus below(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                     candidate_sdram_base, 0x100);
    assert(!below.write(candidate_bootstrap_deep_stack_base - 4u,
                        &r4_value, sizeof(r4_value),
                        candidate_bootstrap_sixth_stack_push_pc,
                        candidate_bootstrap_sixth_stack_push_lr));
    assert(below.first_unresolved().has_value());
}

static void test_ai_observed_seventh_stack_push_adds_exactly_six_new_words() {
    static_assert(candidate_bootstrap_seventh_stack_top == 0x0A000FA0u);
    static_assert(candidate_bootstrap_seventh_stack_push_base == 0x0A000F88u);
    static_assert(candidate_bootstrap_seventh_stack_push_size == 0x18u);
    static_assert(candidate_bootstrap_seventh_stack_push_pc == 0x00001328u);
    static_assert(candidate_bootstrap_seventh_stack_push_lr == 0x00001478u);
    static_assert(candidate_bootstrap_seventh_stack_push_instruction == 0xE92D41F0u);
    static_assert(candidate_bootstrap_region_stack_base == 0x0A000F88u);
    static_assert(candidate_bootstrap_region_stack_size == 24u);

    auto rom = valid_rom();

    strict_bus before(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                      candidate_sdram_base, 0x100);
    std::uint32_t out = 0;
    assert(!before.read(access_kind::data_read,
                        candidate_bootstrap_region_stack_base,
                        &out, sizeof(out), 0x0000132Cu,
                        candidate_bootstrap_seventh_stack_push_lr));
    assert(before.first_unresolved().has_value());

    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint32_t values[6] = {
        0x0A0000E8u, 0x11111111u, 0x22222222u,
        0x00000001u, 0x00000000u, candidate_bootstrap_seventh_stack_push_lr
    };
    for (std::uint32_t i = 0; i < 6u; ++i) {
        const std::uint32_t address = candidate_bootstrap_region_stack_base + i * 4u;
        assert(bus.write(address, &values[i], sizeof(values[i]),
                         candidate_bootstrap_seventh_stack_push_pc,
                         candidate_bootstrap_seventh_stack_push_lr));
    }

    for (std::uint32_t i = 0; i < 6u; ++i) {
        const std::uint32_t address = candidate_bootstrap_region_stack_base + i * 4u;
        out = 0;
        assert(bus.read(access_kind::data_read, address, &out, sizeof(out),
                        0x0000132Cu, candidate_bootstrap_seventh_stack_push_lr));
        assert(out == values[i]);
    }

    strict_bus old_ai(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                      candidate_sdram_base, 0x100);
    assert(!old_ai.write(candidate_bootstrap_region_stack_base,
                         &values[0], sizeof(values[0]),
                         candidate_bootstrap_sixth_stack_push_pc,
                         candidate_bootstrap_sixth_stack_push_lr));
    assert(old_ai.first_unresolved().has_value());

    strict_bus wrong_lr(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    assert(!wrong_lr.write(candidate_bootstrap_region_stack_base,
                           &values[0], sizeof(values[0]),
                           candidate_bootstrap_seventh_stack_push_pc,
                           candidate_bootstrap_seventh_stack_push_lr + 4u));
    assert(wrong_lr.first_unresolved().has_value());

    strict_bus below(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                     candidate_sdram_base, 0x100);
    assert(!below.write(candidate_bootstrap_region_stack_base - 4u,
                        &values[0], sizeof(values[0]),
                        candidate_bootstrap_seventh_stack_push_pc,
                        candidate_bootstrap_seventh_stack_push_lr));
    assert(below.first_unresolved().has_value());
}

static void test_aj_observed_ram_bank_probe_anchor_is_exact_read_only_seed() {
    static_assert(candidate_ram_bank_probe_anchor_address == 0x0A001100u);
    static_assert(candidate_ram_bank_probe_anchor_width == 4u);
    static_assert(candidate_ram_bank_probe_anchor_read_pc == 0x00001368u);
    static_assert(candidate_ram_bank_probe_anchor_read_lr == 0x00001348u);
    static_assert(candidate_ram_bank_probe_anchor_read_instruction == 0xE594C000u);
    static_assert(candidate_ram_bank_probe_anchor_seed == 0u);

    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    std::uint32_t out = 0xFFFFFFFFu;
    assert(bus.read(access_kind::data_read,
                    candidate_ram_bank_probe_anchor_address,
                    &out, sizeof(out),
                    candidate_ram_bank_probe_anchor_read_pc,
                    candidate_ram_bank_probe_anchor_read_lr));
    assert(out == candidate_ram_bank_probe_anchor_seed);
    assert(bus.candidate_ram_bank_probe_anchor_read_count() == 1u);

    strict_bus wrong_pc(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    out = 0;
    assert(!wrong_pc.read(access_kind::data_read,
                          candidate_ram_bank_probe_anchor_address,
                          &out, sizeof(out),
                          candidate_ram_bank_probe_anchor_read_pc + 4u,
                          candidate_ram_bank_probe_anchor_read_lr));
    assert(wrong_pc.first_unresolved().has_value());

    strict_bus wrong_lr(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    assert(!wrong_lr.read(access_kind::data_read,
                          candidate_ram_bank_probe_anchor_address,
                          &out, sizeof(out),
                          candidate_ram_bank_probe_anchor_read_pc,
                          candidate_ram_bank_probe_anchor_read_lr + 4u));
    assert(wrong_lr.first_unresolved().has_value());

    strict_bus neighbor(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    assert(!neighbor.read(access_kind::data_read,
                          candidate_ram_bank_probe_anchor_address + 4u,
                          &out, sizeof(out),
                          candidate_ram_bank_probe_anchor_read_pc,
                          candidate_ram_bank_probe_anchor_read_lr));
    assert(neighbor.first_unresolved().has_value());

    strict_bus write_closed(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                            candidate_sdram_base, 0x100);
    const std::uint32_t value = 0x12345678u;
    assert(!write_closed.write(candidate_ram_bank_probe_anchor_address,
                               &value, sizeof(value),
                               candidate_ram_bank_probe_anchor_read_pc,
                               candidate_ram_bank_probe_anchor_read_lr));
    assert(write_closed.first_unresolved().has_value());
}

static void test_ak_raw_handler_sparse_address_line_probe_is_exact_and_restores() {
    static_assert(candidate_ram_bank_sparse_probe_first_offset == 0x00004000u);
    static_assert(candidate_ram_bank_sparse_probe_limit == 0x01000000u);
    static_assert(candidate_ram_bank_sparse_probe_point_count == 10u);
    static_assert(candidate_ram_bank_sparse_probe_original_read_pc == 0x0000137Cu);
    static_assert(candidate_ram_bank_sparse_probe_test_write_pc == 0x00001380u);
    static_assert(candidate_ram_bank_sparse_probe_anchor_verify_pc == 0x00001384u);
    static_assert(candidate_ram_bank_sparse_probe_readback_pc == 0x00001390u);
    static_assert(candidate_ram_bank_sparse_probe_restore_pc == 0x000013A4u);
    static_assert(candidate_ram_bank_sparse_probe_test_value == 0xFFFFFFFFu);

    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    std::uint32_t anchor_word = 0xFFFFFFFFu;
    assert(bus.read(access_kind::data_read,
                    candidate_ram_bank_probe_anchor_address,
                    &anchor_word, sizeof(anchor_word),
                    candidate_ram_bank_probe_anchor_read_pc,
                    candidate_ram_bank_probe_anchor_read_lr));
    assert(anchor_word == 0u);

    std::uint32_t offset = candidate_ram_bank_sparse_probe_first_offset;
    for (std::size_t i = 0; i < candidate_ram_bank_sparse_probe_point_count; ++i) {
        const std::uint32_t address = candidate_ram_bank_probe_anchor_address + offset;
        std::uint32_t original = 0xFFFFFFFFu;
        assert(bus.read(access_kind::data_read, address, &original, sizeof(original),
                        candidate_ram_bank_sparse_probe_original_read_pc,
                        candidate_ram_bank_sparse_probe_lr));
        assert(original == 0u);
        const std::uint32_t test_value = candidate_ram_bank_sparse_probe_test_value;
        assert(bus.write(address, &test_value, sizeof(test_value),
                         candidate_ram_bank_sparse_probe_test_write_pc,
                         candidate_ram_bank_sparse_probe_lr));
        anchor_word = 0xFFFFFFFFu;
        assert(bus.read(access_kind::data_read,
                        candidate_ram_bank_probe_anchor_address,
                        &anchor_word, sizeof(anchor_word),
                        candidate_ram_bank_sparse_probe_anchor_verify_pc,
                        candidate_ram_bank_sparse_probe_lr));
        assert(anchor_word == 0u);
        std::uint32_t readback = 0;
        assert(bus.read(access_kind::data_read, address, &readback, sizeof(readback),
                        candidate_ram_bank_sparse_probe_readback_pc,
                        candidate_ram_bank_sparse_probe_lr));
        assert(readback == test_value);
        assert(bus.write(address, &original, sizeof(original),
                         candidate_ram_bank_sparse_probe_restore_pc,
                         candidate_ram_bank_sparse_probe_lr));
        offset <<= 1u;
    }

    assert(bus.candidate_ram_bank_sparse_probe_original_read_count() == 10u);
    assert(bus.candidate_ram_bank_sparse_probe_test_write_count() == 10u);
    assert(bus.candidate_ram_bank_sparse_probe_anchor_verify_read_count() == 10u);
    assert(bus.candidate_ram_bank_sparse_probe_readback_count() == 10u);
    assert(bus.candidate_ram_bank_sparse_probe_restore_write_count() == 10u);

    strict_bus gap(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    std::uint32_t out = 0;
    assert(!gap.read(access_kind::data_read,
                     candidate_ram_bank_probe_anchor_address + 0x2000u,
                     &out, sizeof(out),
                     candidate_ram_bank_sparse_probe_original_read_pc,
                     candidate_ram_bank_sparse_probe_lr));
    assert(gap.first_unresolved().has_value());

    strict_bus wrong_pc(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    assert(!wrong_pc.read(access_kind::data_read,
                          candidate_ram_bank_probe_anchor_address
                              + candidate_ram_bank_sparse_probe_first_offset,
                          &out, sizeof(out), 0x00001380u,
                          candidate_ram_bank_sparse_probe_lr));
    assert(wrong_pc.first_unresolved().has_value());

    strict_bus wrong_value(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                           candidate_sdram_base, 0x100);
    const std::uint32_t bad = 0x12345678u;
    assert(!wrong_value.write(candidate_ram_bank_probe_anchor_address
                                  + candidate_ram_bank_sparse_probe_first_offset,
                              &bad, sizeof(bad),
                              candidate_ram_bank_sparse_probe_test_write_pc,
                              candidate_ram_bank_sparse_probe_lr));
    assert(wrong_value.first_unresolved().has_value());
}

static void test_al_observed_local_frame_word_is_exact_and_does_not_widen() {
    static_assert(candidate_bootstrap_local_frame_word_address == 0x0A000FBCu);
    static_assert(candidate_bootstrap_local_frame_word_width == 4u);
    static_assert(candidate_bootstrap_local_frame_word_write_pc == 0x000012ACu);
    static_assert(candidate_bootstrap_local_frame_word_write_lr == 0x00001270u);
    static_assert(candidate_bootstrap_local_frame_word_instruction == 0xE58D0000u);
    static_assert(candidate_bootstrap_local_frame_word_value == 0x09FFF400u);

    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);
    const std::uint32_t value = candidate_bootstrap_local_frame_word_value;
    assert(bus.write(candidate_bootstrap_local_frame_word_address,
                     &value, sizeof(value),
                     candidate_bootstrap_local_frame_word_write_pc,
                     candidate_bootstrap_local_frame_word_write_lr));
    assert(bus.candidate_bootstrap_local_frame_word_write_count() == 1u);

    std::uint32_t out = 0;
    assert(bus.read(access_kind::data_read,
                    candidate_bootstrap_local_frame_word_address,
                    &out, sizeof(out), 0x000012B0u,
                    candidate_bootstrap_local_frame_word_write_lr));
    assert(out == value);
    assert(bus.candidate_bootstrap_local_frame_word_read_count() == 1u);

    strict_bus wrong_pc(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    assert(!wrong_pc.write(candidate_bootstrap_local_frame_word_address,
                           &value, sizeof(value),
                           candidate_bootstrap_local_frame_word_write_pc + 4u,
                           candidate_bootstrap_local_frame_word_write_lr));
    assert(wrong_pc.first_unresolved().has_value());

    strict_bus wrong_lr(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    assert(!wrong_lr.write(candidate_bootstrap_local_frame_word_address,
                           &value, sizeof(value),
                           candidate_bootstrap_local_frame_word_write_pc,
                           candidate_bootstrap_local_frame_word_write_lr + 4u));
    assert(wrong_lr.first_unresolved().has_value());

    strict_bus wrong_value(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                           candidate_sdram_base, 0x100);
    const std::uint32_t bad = 0x12345678u;
    assert(!wrong_value.write(candidate_bootstrap_local_frame_word_address,
                              &bad, sizeof(bad),
                              candidate_bootstrap_local_frame_word_write_pc,
                              candidate_bootstrap_local_frame_word_write_lr));
    assert(wrong_value.first_unresolved().has_value());

    strict_bus neighbor(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                        candidate_sdram_base, 0x100);
    assert(!neighbor.write(candidate_bootstrap_local_frame_word_address + 4u,
                           &value, sizeof(value),
                           candidate_bootstrap_local_frame_word_write_pc,
                           candidate_bootstrap_local_frame_word_write_lr));
    assert(neighbor.first_unresolved().has_value());
}

static void test_gap_before_post_probe_workspace_remains_unmapped() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc, candidate_sdram_base, 0x100);
    const std::uint32_t gap_address = candidate_post_probe_workspace_base - 4u;
    std::uint32_t out = 0;
    assert(!bus.read(access_kind::data_read, gap_address, &out, sizeof(out), 0x00000380, 0x0000241C));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->address == gap_address);
}

static void test_post_probe_workspace_is_zero_seeded_mutable_and_bounded() {
    static_assert(candidate_post_probe_workspace_base == 0x0A0001E0u);
    static_assert(candidate_post_probe_workspace_size == 0x20u);
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    std::uint32_t out = 0xFFFFFFFFu;
    assert(bus.read(access_kind::data_read,
                    candidate_post_probe_workspace_base + 8u,
                    &out, sizeof(out), 0x00000388, 0x0000241C));
    assert(out == 0);
    assert(bus.candidate_post_probe_workspace_read_count() == 1);

    const std::uint32_t value = 0x11223344u;
    assert(bus.write(candidate_post_probe_workspace_base + 0x1Cu,
                     &value, sizeof(value), 0x00000390, 0x0000241C));
    assert(bus.candidate_post_probe_workspace_write_count() == 1);
    out = 0;
    assert(bus.read(access_kind::data_read,
                    candidate_post_probe_workspace_base + 0x1Cu,
                    &out, sizeof(out), 0x00000394, 0x0000241C));
    assert(out == value);

    std::uint32_t neighbor = 0;
    assert(!bus.read(access_kind::data_read,
                     candidate_post_probe_workspace_base
                         + static_cast<std::uint32_t>(candidate_post_probe_workspace_size),
                     &neighbor, sizeof(neighbor), 0x00000398, 0x0000241C));
    assert(bus.first_unresolved().has_value());
}

static void test_low_vector_shadow_is_rom_seeded_mutable_and_exactly_36_bytes() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc,
                   candidate_sdram_base, 0x100);

    std::uint32_t initial = 0;
    std::memcpy(&initial, rom.data(), sizeof(initial));
    std::uint32_t out = 0;
    assert(bus.read(access_kind::data_read, low_vector_shadow_base,
                    &out, sizeof(out), 0x000012EC, 0x000003C8));
    assert(out == initial);

    const std::uint32_t replacement = 0x12345678u;
    assert(bus.write(low_vector_shadow_base, &replacement, sizeof(replacement),
                     0x00002344, 0x000003C8));
    assert(bus.low_vector_shadow_write_count() == 1);
    out = 0;
    assert(bus.read(access_kind::data_read, low_vector_shadow_base,
                    &out, sizeof(out), 0x00002348, 0x000003C8));
    assert(out == replacement);

    const std::uint32_t last = 0x89ABCDEFu;
    assert(bus.write(low_vector_shadow_base
                         + static_cast<std::uint32_t>(low_vector_shadow_size - sizeof(last)),
                     &last, sizeof(last), 0x00002344, 0x000003C8));

    assert(!bus.write(low_vector_shadow_base + static_cast<std::uint32_t>(low_vector_shadow_size),
                      &last, sizeof(last), 0x00002344, 0x000003C8));
    assert(bus.first_unresolved().has_value());
    assert(bus.first_unresolved()->address == low_vector_shadow_base + low_vector_shadow_size);
    assert(bus.first_unresolved()->cause == unresolved_cause::rom_write);
}

static void test_candidate_probe_window_is_mutable_and_bounded() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000, cold_reset_pc, candidate_sdram_base, 0x100);
    const std::uint32_t value = 0x55AA55AAu;
    assert(bus.write(candidate_ram_probe_base + 4, &value, sizeof(value), 0x00002670, 0x000024C4));
    assert(bus.candidate_ram_probe_write_count() == 1);
    std::uint32_t out = 0;
    assert(bus.read(access_kind::data_read, candidate_ram_probe_base + 4, &out, sizeof(out), 0x00002674, 0x000024C4));
    assert(out == value);
    std::uint32_t neighbor = 0;
    assert(!bus.read(access_kind::data_read, candidate_ram_probe_base + 16, &neighbor, sizeof(neighbor), 0x00002680, 0x000024C4));
    assert(bus.first_unresolved().has_value());
}
static void test_bus_records_unmapped_data_read() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000);
    std::uint16_t out = 0;
    assert(!bus.read(access_kind::data_read, 0x40000000, &out, sizeof(out), 0x50000140, 0x50000144));
    const auto &u = bus.first_unresolved();
    assert(u.has_value());
    assert(u->kind == access_kind::data_read);
    assert(u->width == 2);
    assert(u->address == 0x40000000);
    assert(u->pc == 0x50000140);
    assert(u->lr == 0x50000144);
    assert(u->count == 1);
    assert(u->cause == unresolved_cause::unmapped);
}

static void test_bus_records_unmapped_code_read() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000);
    std::uint32_t out = 0;
    assert(!bus.read(access_kind::code_read, 0x00000000, &out, sizeof(out), 0x50000150, 0x50000154));
    const auto &u = bus.first_unresolved();
    assert(u.has_value());
    assert(u->kind == access_kind::code_read);
    assert(u->address == 0x00000000);
    assert(u->pc == 0x50000150);
    assert(u->lr == 0x50000154);
}

static void test_bus_keeps_first_unresolved() {
    auto rom = valid_rom();
    strict_bus bus(rom.data(), rom.size(), 0x50000000);
    std::uint32_t out = 0;
    assert(!bus.read(access_kind::data_read, 0x11110000, &out, sizeof(out), 0x50000160, 0x50000164));
    assert(bus.first_unresolved()->address == 0x11110000);
    assert(bus.first_unresolved()->count == 1);
    assert(!bus.read(access_kind::code_read, 0x22220000, &out, sizeof(out), 0x50000170, 0x50000174));
    assert(bus.first_unresolved()->address == 0x11110000);
    assert(bus.first_unresolved()->kind == access_kind::data_read);
}

int main() {
    test_parse_valid_header();
    test_parse_rejects_short_header();
    test_parse_rejects_wrong_base();
    test_parse_rejects_zero_size();
    test_parse_rejects_size_beyond_file();
    test_parse_rejects_32bit_range_overflow();
    test_bus_reads_little_endian_inside_rom();
    test_bus_reset_alias_reads_same_rom_bytes();
    test_bus_rejects_cross_rom_end();
    test_bus_records_rom_write();
    test_candidate_sdram_write_then_read_is_tracked();
    test_candidate_sdram_uninitialized_read_stops();
    test_candidate_sdram_write_trace_records_exact_bus_transactions();
    test_exact_observed_mmio_write_is_the_only_allowlisted_mmio();
    test_observed_mmio_neighbor_is_not_mapped();
    test_exact_observed_flash_command_is_allowlisted_only_at_fl1_base();
    test_second_flash_window_is_not_generically_mapped();
    test_exact_observed_flash_id_entry_write_is_allowlisted();
    test_flash_id_entry_allowlist_is_exact();
    test_amd_reference_manufacturer_id_requires_id_entry();
    test_amd_reference_manufacturer_id_read_after_id_entry();
    test_amd_reference_device_id_read_after_id_entry();
    test_amd_reference_device_id_read_is_exact_width();
    test_flash_id_exit_f0_disables_autoselect_state();
    test_flash_id_exit_allowlist_is_exact();
    test_flash_unlock_cycle1_matches_observed_x16_transaction();
    test_flash_unlock_cycle1_allowlist_is_exact();
    test_flash_f0_resets_unlock_stage();
    test_flash_unlock_cycle2_requires_stage1_and_matches_observed_transaction();
    test_flash_unlock_cycle2_allowlist_is_exact();
    test_unlock_sequence_enters_autoselect_only_after_stage2();
    test_unlock_autoselect_command_allowlist_is_exact();
    test_candidate_probe_window_reads_zero();
    test_eight_step_candidate_probe_loop_is_exact_and_sparse();
    test_y_observed_bootstrap_copy_is_exact_gated_and_initialized_only();
    test_ab_observed_post_copy_mutation_is_exact_and_requires_initialized_copy();
    test_af_observed_record_loop_mutation_is_exact_and_initialized_only();
    test_z_observed_first_stack_push_is_exact_gated_and_initialized_only();
    test_aa_observed_second_stack_push_is_exact_gated_and_initialized_only();
    test_af_observed_fourth_stack_push_reuses_exact_12byte_window();
    test_ag_observed_fifth_stack_push_adds_exactly_one_new_word();
    test_ah_observed_sixth_stack_push_adds_exactly_two_new_words();
    test_ai_observed_seventh_stack_push_adds_exactly_six_new_words();
    test_aj_observed_ram_bank_probe_anchor_is_exact_read_only_seed();
    test_ak_raw_handler_sparse_address_line_probe_is_exact_and_restores();
    test_al_observed_local_frame_word_is_exact_and_does_not_widen();
    test_gap_before_post_probe_workspace_remains_unmapped();
    test_post_probe_workspace_is_zero_seeded_mutable_and_bounded();
    test_low_vector_shadow_is_rom_seeded_mutable_and_exactly_36_bytes();
    test_candidate_probe_window_is_mutable_and_bounded();
    test_bus_records_unmapped_data_read();
    test_bus_records_unmapped_code_read();
    test_bus_keeps_first_unresolved();
    std::cout << "rh29_machine_model_tests: PASS\n";
    return 0;
}
