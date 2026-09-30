#include "rh29_machine_model.h"

#include <array>
#include <cassert>
#include <cstdint>
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
    test_bus_records_unmapped_data_read();
    test_bus_records_unmapped_code_read();
    test_bus_keeps_first_unresolved();
    std::cout << "rh29_machine_model_tests: PASS\n";
    return 0;
}
