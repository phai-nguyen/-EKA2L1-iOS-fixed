#include "rh29_machine_runner.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>

using namespace eka2l1::machine::rh29;

static void require_contains(const std::string &text, const std::string &needle) {
    if (text.find(needle) == std::string::npos) {
        std::cerr << "missing: " << needle << "\n" << text << std::endl;
        std::abort();
    }
}

static probe_result sample_result() {
    probe_result result{};
    result.header.restart_vector = 0x50000100;
    result.header.rom_base = 0x50000000;
    result.header.rom_size = 0x01170000;
    result.header.kern_data_address = 0x80001000;
    result.header.kern_limit = 0x80002000;
    result.instruction_budget = 10000;
    result.executed_instructions = 37;
    result.stop_reason = probe_stop_reason::unresolved_access;
    for (std::size_t i = 0; i < result.registers.r.size(); ++i) {
        result.registers.r[i] = static_cast<std::uint32_t>(0x1000 + i);
    }
    result.registers.sp = 0x20001000;
    result.registers.lr = 0x50001234;
    result.registers.pc = 0x50005678;
    result.registers.cpsr = 0x000000D3;
    result.unresolved = unresolved_access{access_kind::data_write, 4, 0x40000010,
                                          0x50005678, 0x50001234, 0xAABBCCDD, 1};
    return result;
}

static void test_report_contains_probe_identity_and_rom_header() {
    const auto text = format_report(sample_result());
    require_contains(text, "RH29_MACHINE1_A");
    require_contains(text, "ROM_BASE=0x50000000");
    require_contains(text, "ROM_SIZE=0x01170000");
    require_contains(text, "RESTART_VECTOR=0x50000100");
    require_contains(text, "KERN_DATA_ADDRESS=0x80001000");
    require_contains(text, "KERN_LIMIT=0x80002000");
}

static void test_report_contains_budget_stop_and_registers() {
    const auto text = format_report(sample_result());
    require_contains(text, "INSTRUCTION_BUDGET=10000");
    require_contains(text, "EXECUTED_INSTRUCTIONS=37");
    require_contains(text, "STOP_REASON=unresolved_access");
    require_contains(text, "PC=0x50005678");
    require_contains(text, "LR=0x50001234");
    require_contains(text, "SP=0x20001000");
    require_contains(text, "CPSR=0x000000D3");
    for (int i = 0; i <= 12; ++i) {
        char name[16];
        std::snprintf(name, sizeof(name), "R%d=0x", i);
        require_contains(text, name);
    }
}

static void test_report_contains_unresolved_access() {
    const auto text = format_report(sample_result());
    require_contains(text, "UNRESOLVED_KIND=data_write");
    require_contains(text, "UNRESOLVED_WIDTH_BITS=32");
    require_contains(text, "UNRESOLVED_ADDRESS=0x40000010");
    require_contains(text, "UNRESOLVED_PC=0x50005678");
    require_contains(text, "UNRESOLVED_LR=0x50001234");
    require_contains(text, "UNRESOLVED_VALUE=0x00000000AABBCCDD");
    require_contains(text, "UNRESOLVED_COUNT=1");
}

static void test_report_contains_cpu_exception() {
    auto result = sample_result();
    result.stop_reason = probe_stop_reason::cpu_exception;
    result.unresolved.reset();
    result.exception = cpu_exception_info{4, 0xDEADBEEF, false};
    const auto text = format_report(result);
    require_contains(text, "STOP_REASON=cpu_exception");
    require_contains(text, "EXCEPTION_TYPE=4");
    require_contains(text, "EXCEPTION_DATA=0xDEADBEEF");
    require_contains(text, "SYSTEM_CALL=0");
}

static void test_report_contains_error_detail() {
    probe_result result{};
    result.stop_reason = probe_stop_reason::invalid_rom;
    result.detail = "unexpected ROM base";
    const auto text = format_report(result);
    require_contains(text, "STOP_REASON=invalid_rom");
    require_contains(text, "DETAIL=unexpected ROM base");
}

int main() {
    test_report_contains_probe_identity_and_rom_header();
    test_report_contains_budget_stop_and_registers();
    test_report_contains_unresolved_access();
    test_report_contains_cpu_exception();
    test_report_contains_error_detail();
    std::cout << "rh29_report_tests: PASS\n";
    return 0;
}
