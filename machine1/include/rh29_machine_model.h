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

    // MACHINE1-Y device evidence after fixing Dyncom FIQ banking: SVC R11 is
    // preserved as 0x744, LDR [R1] returns 0x10, and the bootstrap derives
    // R2=0x42 words before entering the copy loop at 0x2340. The loop copies
    // 66 x 32-bit words (0x108 bytes) from 0x744 to 0x0A000000. MACHINE1-Z
    // admits only the exact STR at PC=0x2344/LR=0x3C8 inside that footprint.
    // Reads are allowed only for bytes already initialized by those writes, so
    // the earlier sparse RAM-probe gaps remain unmapped and behavior is not
    // broadened into a contiguous RAM-bank claim.
    static constexpr std::uint32_t candidate_bootstrap_copy_source = 0x00000744u;
    static constexpr std::uint32_t candidate_bootstrap_copy_base = candidate_ram_probe_base;
    static constexpr std::size_t candidate_bootstrap_copy_size = 0x108u;
    static constexpr std::size_t candidate_bootstrap_copy_write_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_bootstrap_copy_pc = 0x00002344u;
    static constexpr std::uint32_t candidate_bootstrap_copy_lr = 0x000003C8u;

    // MACHINE1-Z device evidence: after the exact 0x108-byte bootstrap copy
    // completes, firmware loads base 0x0A000000 and at PC=0x3AC computes
    // SP=base+0xFF0. It then enters PC=0x11A8 with instruction 0xE92D47F0
    // (STMDB sp!, {r4-r10,lr}), whose first bus write is 0x0A000FD0.
    // MACHINE1-AA admits only this observed 32-byte first push footprint,
    // gated by PC/LR/32-bit width. Readback is initialized-only; this is not
    // evidence for a generic 4-KiB RAM page or a larger stack mapping.
    static constexpr std::uint32_t candidate_bootstrap_stack_top =
        candidate_ram_probe_base + 0x00000FF0u;
    static constexpr std::size_t candidate_bootstrap_stack_push_size = 0x20u;
    static constexpr std::uint32_t candidate_bootstrap_stack_push_base =
        candidate_bootstrap_stack_top - static_cast<std::uint32_t>(candidate_bootstrap_stack_push_size);
    static constexpr std::size_t candidate_bootstrap_stack_write_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_bootstrap_stack_push_pc = 0x000011A8u;
    static constexpr std::uint32_t candidate_bootstrap_stack_push_lr = 0x000003C8u;
    static constexpr std::uint32_t candidate_bootstrap_stack_push_instruction = 0xE92D47F0u;

    // MACHINE1-AA device evidence: after the first 32-byte push, PC=0x11AC
    // executes SUB sp,sp,#0x14, producing SP=0x0A000FBC. The call at PC=0x11C4
    // enters PC=0x22A0 with LR=0x11C8 and instruction 0xE92D4070
    // (STMDB sp!, {r4-r6,lr}). Its first unresolved write is 0x0A000FAC.
    // MACHINE1-AB admits only that exact 16-byte second push footprint.
    static constexpr std::uint32_t candidate_bootstrap_nested_stack_top = 0x0A000FBCu;
    static constexpr std::size_t candidate_bootstrap_nested_stack_push_size = 0x10u;
    static constexpr std::uint32_t candidate_bootstrap_nested_stack_push_base =
        candidate_bootstrap_nested_stack_top
            - static_cast<std::uint32_t>(candidate_bootstrap_nested_stack_push_size);
    static constexpr std::size_t candidate_bootstrap_nested_stack_write_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_bootstrap_nested_stack_push_pc = 0x000022A0u;
    static constexpr std::uint32_t candidate_bootstrap_nested_stack_push_lr = 0x000011C8u;
    static constexpr std::uint32_t candidate_bootstrap_nested_stack_push_instruction = 0xE92D4070u;

    // MACHINE1-AC device evidence: after the exact post-copy mutation at
    // 0x0A000010 succeeds, firmware returns to 0x11D8 and calls 0x2228.
    // At PC=0x2228/LR=0x11E0, instruction 0xE92D4030 is
    // STMDB sp!, {r4,r5,lr}. SP starts at 0x0A000FBC, so the exact 12-byte
    // footprint is 0x0A000FB0..0x0A000FBB. This footprint is wholly inside
    // the already evidenced AA/AB 16-byte nested-stack window. MACHINE1-AD
    // therefore admits this exact third push only; it does not widen memory.
    static constexpr std::uint32_t candidate_bootstrap_third_stack_top = 0x0A000FBCu;
    static constexpr std::size_t candidate_bootstrap_third_stack_push_size = 0x0Cu;
    static constexpr std::uint32_t candidate_bootstrap_third_stack_push_base =
        candidate_bootstrap_third_stack_top
            - static_cast<std::uint32_t>(candidate_bootstrap_third_stack_push_size);
    static constexpr std::uint32_t candidate_bootstrap_third_stack_push_pc = 0x00002228u;
    static constexpr std::uint32_t candidate_bootstrap_third_stack_push_lr = 0x000011E0u;
    static constexpr std::uint32_t candidate_bootstrap_third_stack_push_instruction = 0xE92D4030u;
    static_assert(candidate_bootstrap_third_stack_push_base >= candidate_bootstrap_nested_stack_push_base
        && candidate_bootstrap_third_stack_top <= candidate_bootstrap_nested_stack_top,
        "MACHINE1-AD third stack push must remain inside the evidenced nested-stack window");

    // MACHINE1-AF device evidence: after all 16 record control words are
    // processed, PC=0x11E4 calls 0x2258 with LR=0x11E8. The first instruction
    // there is again 0xE92D4030 (STMDB sp!, {r4,r5,lr}) with SP=0x0A000FBC.
    // Its first unresolved write is 0x0A000FB0, exactly the same 12-byte
    // footprint already evidenced at 0x2228. MACHINE1-AG admits only this new
    // callsite over that existing footprint; it does not widen stack memory.
    static constexpr std::uint32_t candidate_bootstrap_fourth_stack_top =
        candidate_bootstrap_third_stack_top;
    static constexpr std::size_t candidate_bootstrap_fourth_stack_push_size =
        candidate_bootstrap_third_stack_push_size;
    static constexpr std::uint32_t candidate_bootstrap_fourth_stack_push_base =
        candidate_bootstrap_third_stack_push_base;
    static constexpr std::uint32_t candidate_bootstrap_fourth_stack_push_pc = 0x00002258u;
    static constexpr std::uint32_t candidate_bootstrap_fourth_stack_push_lr = 0x000011E8u;
    static constexpr std::uint32_t candidate_bootstrap_fourth_stack_push_instruction = 0xE92D4030u;
    static_assert(candidate_bootstrap_fourth_stack_push_base >= candidate_bootstrap_nested_stack_push_base
        && candidate_bootstrap_fourth_stack_top <= candidate_bootstrap_nested_stack_top,
        "MACHINE1-AG fourth stack push must reuse the evidenced nested-stack window");

    // MACHINE1-AG device evidence: after returning from 0x2258, PC=0x11FC
    // calls 0x1BB4 with LR=0x1200. Instruction 0xE92D40F0 is
    // STMDB sp!, {r4-r7,lr}: 5 x 32-bit words = 20 bytes. With SP=0x0A000FBC,
    // the exact footprint is 0x0A000FA8..0x0A000FBB. The upper 16 bytes are
    // already backed by the nested-stack window 0x0A000FAC..0x0A000FBB.
    // MACHINE1-AH adds only the newly evidenced low word 0x0A000FA8..0x0A000FAB
    // for this exact callsite; older callsites remain unable to access it.
    static constexpr std::uint32_t candidate_bootstrap_fifth_stack_top = 0x0A000FBCu;
    static constexpr std::size_t candidate_bootstrap_fifth_stack_push_size = 0x14u;
    static constexpr std::uint32_t candidate_bootstrap_fifth_stack_push_base =
        candidate_bootstrap_fifth_stack_top
            - static_cast<std::uint32_t>(candidate_bootstrap_fifth_stack_push_size);
    static constexpr std::uint32_t candidate_bootstrap_fifth_stack_push_pc = 0x00001BB4u;
    static constexpr std::uint32_t candidate_bootstrap_fifth_stack_push_lr = 0x00001200u;
    static constexpr std::uint32_t candidate_bootstrap_fifth_stack_push_instruction = 0xE92D40F0u;
    static constexpr std::uint32_t candidate_bootstrap_nested_stack_extension_base =
        candidate_bootstrap_fifth_stack_push_base;
    static constexpr std::size_t candidate_bootstrap_nested_stack_extension_size =
        candidate_bootstrap_nested_stack_push_base
            - candidate_bootstrap_fifth_stack_push_base;
    static_assert(candidate_bootstrap_nested_stack_extension_size == sizeof(std::uint32_t),
        "MACHINE1-AH must add exactly one new stack word");
    static_assert(candidate_bootstrap_fifth_stack_top == candidate_bootstrap_nested_stack_top,
        "MACHINE1-AH fifth push must share the evidenced nested-stack top");

    // MACHINE1-AH device evidence: helper 0x1BB4 returns into a second helper
    // at PC=0x1438/LR=0x1BE0. Instruction 0xE92D4010 is
    // STMDB sp!, {r4,lr}. Entry SP is 0x0A000FA8, so the exact 8-byte
    // footprint is 0x0A000FA0..0x0A000FA7. This is wholly below the AH
    // one-word extension and all older stack windows. MACHINE1-AI therefore
    // adds only these two newly evidenced words for this exact callsite.
    static constexpr std::uint32_t candidate_bootstrap_sixth_stack_top =
        candidate_bootstrap_fifth_stack_push_base;
    static constexpr std::size_t candidate_bootstrap_sixth_stack_push_size = 0x08u;
    static constexpr std::uint32_t candidate_bootstrap_sixth_stack_push_base =
        candidate_bootstrap_sixth_stack_top
            - static_cast<std::uint32_t>(candidate_bootstrap_sixth_stack_push_size);
    static constexpr std::uint32_t candidate_bootstrap_sixth_stack_push_pc = 0x00001438u;
    static constexpr std::uint32_t candidate_bootstrap_sixth_stack_push_lr = 0x00001BE0u;
    static constexpr std::uint32_t candidate_bootstrap_sixth_stack_push_instruction = 0xE92D4010u;
    static constexpr std::uint32_t candidate_bootstrap_deep_stack_base =
        candidate_bootstrap_sixth_stack_push_base;
    static constexpr std::size_t candidate_bootstrap_deep_stack_size =
        candidate_bootstrap_sixth_stack_push_size;
    static_assert(candidate_bootstrap_deep_stack_size == 2u * sizeof(std::uint32_t),
        "MACHINE1-AI must add exactly two new stack words");
    static_assert(candidate_bootstrap_sixth_stack_top == candidate_bootstrap_nested_stack_extension_base,
        "MACHINE1-AI deep stack must stop exactly below the AH extension");

    // MACHINE1-AI device evidence: the low5=2 record path reaches PC=0x1474,
    // then BLs to 0x1328 with LR=0x1478. The first instruction there is
    // 0xE92D41F0 = STMDB sp!, {r4-r8,lr}. Entry SP is 0x0A000FA0, so the
    // exact 24-byte footprint is 0x0A000F88..0x0A000F9F. This is wholly
    // below the AI deep-stack window. MACHINE1-AJ adds only these six words
    // for this exact callsite; no generic stack/RAM page is created.
    static constexpr std::uint32_t candidate_bootstrap_seventh_stack_top =
        candidate_bootstrap_sixth_stack_push_base;
    static constexpr std::size_t candidate_bootstrap_seventh_stack_push_size = 0x18u;
    static constexpr std::uint32_t candidate_bootstrap_seventh_stack_push_base =
        candidate_bootstrap_seventh_stack_top
            - static_cast<std::uint32_t>(candidate_bootstrap_seventh_stack_push_size);
    static constexpr std::uint32_t candidate_bootstrap_seventh_stack_push_pc = 0x00001328u;
    static constexpr std::uint32_t candidate_bootstrap_seventh_stack_push_lr = 0x00001478u;
    static constexpr std::uint32_t candidate_bootstrap_seventh_stack_push_instruction = 0xE92D41F0u;
    static constexpr std::uint32_t candidate_bootstrap_region_stack_base =
        candidate_bootstrap_seventh_stack_push_base;
    static constexpr std::size_t candidate_bootstrap_region_stack_size =
        candidate_bootstrap_seventh_stack_push_size;
    static_assert(candidate_bootstrap_region_stack_size == 6u * sizeof(std::uint32_t),
        "MACHINE1-AJ must add exactly six new stack words");
    static_assert(candidate_bootstrap_seventh_stack_top == candidate_bootstrap_deep_stack_base,
        "MACHINE1-AJ region stack must stop exactly below the AI deep stack");

    // MACHINE1-AJ device evidence: after the exact 0x1328 stack push succeeds,
    // the low5=2 handler loads record base=0x0A000000 and size=0x01000000,
    // computes r4=0x0A001100, then executes LDR r12,[r4] at PC=0x1368.
    // Public Symbian bootstrap source corroborates this general pattern as a
    // RAM-bank address-configuration probe that saves an original word before
    // destructive alias tests. The power-on value of RAM is not observable in
    // our report, so MACHINE1-AK supplies a clearly synthetic zero seed ONLY
    // for this first exact read. No surrounding RAM range and no write path is
    // opened yet; the next device transaction must provide the next evidence.
    static constexpr std::uint32_t candidate_ram_bank_probe_anchor_address = 0x0A001100u;
    static constexpr std::size_t candidate_ram_bank_probe_anchor_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_ram_bank_probe_anchor_read_pc = 0x00001368u;
    static constexpr std::uint32_t candidate_ram_bank_probe_anchor_read_lr = 0x00001348u;
    static constexpr std::uint32_t candidate_ram_bank_probe_anchor_read_instruction = 0xE594C000u;
    static constexpr std::uint32_t candidate_ram_bank_probe_anchor_seed = 0x00000000u;

    // MACHINE1-AK device evidence plus the raw 0x1328 handler dump prove the
    // next phase is a power-of-two address-line probe. R5 starts at 0x4000,
    // doubles until it reaches record size 0x01000000, and accesses
    // anchor+R5 via PC 0x137C/0x1380/0x1390/0x13A4. MACHINE1-AL models only
    // those ten sparse 32-bit words as independent synthetic-zero RAM probe
    // points. It does NOT map the 16-MiB interval or any gap between points.
    static constexpr std::uint32_t candidate_ram_bank_sparse_probe_first_offset = 0x00004000u;
    static constexpr std::uint32_t candidate_ram_bank_sparse_probe_limit = 0x01000000u;
    static constexpr std::size_t candidate_ram_bank_sparse_probe_point_count = 10u;
    static constexpr std::size_t candidate_ram_bank_sparse_probe_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_ram_bank_sparse_probe_lr = 0x00001348u;
    static constexpr std::uint32_t candidate_ram_bank_sparse_probe_original_read_pc = 0x0000137Cu;
    static constexpr std::uint32_t candidate_ram_bank_sparse_probe_test_write_pc = 0x00001380u;
    static constexpr std::uint32_t candidate_ram_bank_sparse_probe_anchor_verify_pc = 0x00001384u;
    static constexpr std::uint32_t candidate_ram_bank_sparse_probe_readback_pc = 0x00001390u;
    static constexpr std::uint32_t candidate_ram_bank_sparse_probe_restore_pc = 0x000013A4u;
    static constexpr std::uint32_t candidate_ram_bank_sparse_probe_test_value =
        ~candidate_ram_bank_probe_anchor_seed;
    static_assert((candidate_ram_bank_sparse_probe_first_offset
                   << candidate_ram_bank_sparse_probe_point_count)
                      == candidate_ram_bank_sparse_probe_limit,
        "MACHINE1-AL sparse points must cover powers 0x4000..0x800000 only");

    // MACHINE1-AL device evidence: all ten sparse address-line probe points
    // completed with exact 10/10/10/10/10 read/write/verify/readback/restore
    // counts. Execution then returns to the caller path. At PC=0x12A8,
    // instruction 0xE2440B03 derives R0=0x09FFF400 from R4=0x0A000000;
    // PC=0x12AC/LR=0x1270 executes 0xE58D0000 (STR r0,[sp]) with
    // SP=0x0A000FBC. MACHINE1-AM admits only this observed 32-bit local-frame
    // word, exact callsite and exact value. It does not expose the remaining
    // 0x14-byte reserved frame or any adjacent stack/RAM range.
    static constexpr std::uint32_t candidate_bootstrap_local_frame_word_address = 0x0A000FBCu;
    static constexpr std::size_t candidate_bootstrap_local_frame_word_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_bootstrap_local_frame_word_write_pc = 0x000012ACu;
    static constexpr std::uint32_t candidate_bootstrap_local_frame_word_write_lr = 0x00001270u;
    static constexpr std::uint32_t candidate_bootstrap_local_frame_word_instruction = 0xE58D0000u;
    static constexpr std::uint32_t candidate_bootstrap_local_frame_word_value = 0x09FFF400u;
    static_assert(candidate_bootstrap_local_frame_word_address == candidate_bootstrap_nested_stack_top,
        "MACHINE1-AM local-frame word must start exactly at the prior nested-stack top");

    // MACHINE1-AM device evidence consumed by MACHINE1-AN: after storing 0x09FFF400 in the local frame,
    // PC=0x12B4 calls helper 0x12EC with LR=0x12B8. That helper derives a
    // 0x108-byte copy length from the existing 0x0A000000 table and branches
    // into the same 32-bit copy loop at PC=0x2344. The first failed write is
    // value 0x10 to 0x09FFF400. MACHINE1-AN admits only the exact 0x108-byte
    // relocation target, exact loop PC/LR, and only values matching the already
    // initialized source table word-for-word. No surrounding RAM is mapped.
    static constexpr std::uint32_t candidate_bootstrap_relocation_source = candidate_bootstrap_copy_base;
    static constexpr std::uint32_t candidate_bootstrap_relocation_base = 0x09FFF400u;
    static constexpr std::size_t candidate_bootstrap_relocation_size = candidate_bootstrap_copy_size;
    static constexpr std::size_t candidate_bootstrap_relocation_write_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_bootstrap_relocation_pc = 0x00002344u;
    static constexpr std::uint32_t candidate_bootstrap_relocation_lr = 0x000012B8u;
    static constexpr std::uint32_t candidate_bootstrap_relocation_instruction = 0xE4803004u;
    static_assert(candidate_bootstrap_relocation_base + candidate_bootstrap_relocation_size == 0x09FFF508u,
        "MACHINE1-AN relocation window must stay exactly 0x108 bytes");

    // MACHINE1-AN device evidence consumed by MACHINE1-AO: the relocation completes all 66 words and
    // returns to PC=0x12B8. The first new unresolved access is the first of
    // four explicit stores that fill the remaining reserved local-frame words:
    //   0x12B8 STR r10,[sp,#4]  -> 0x0A000FC0 = 0x00000080
    //   0x12BC STR r9, [sp,#8]  -> 0x0A000FC4 = 0x00000000
    //   0x12C0 STR r8, [sp,#12] -> 0x0A000FC8 = 0x00000000
    //   0x12C4 STR r7, [sp,#16] -> 0x0A000FCC = 0x01170000
    // The first tuple is directly device-observed; the remaining three are
    // deterministic from the adjacent raw ROM instructions plus the same AN
    // register snapshot. MACHINE1-AO admits only these four exact tuples and
    // initialized-only readback. It does not map 0x0A000FBC..0x0A000FCF as a
    // generic stack range.
    static constexpr std::size_t candidate_bootstrap_local_frame_tail_word_count = 4u;
    static constexpr std::size_t candidate_bootstrap_local_frame_tail_word_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_bootstrap_local_frame_tail_lr = 0x000012B8u;
    static constexpr std::array<std::uint32_t, candidate_bootstrap_local_frame_tail_word_count>
        candidate_bootstrap_local_frame_tail_addresses{
            0x0A000FC0u, 0x0A000FC4u, 0x0A000FC8u, 0x0A000FCCu
        };
    static constexpr std::array<std::uint32_t, candidate_bootstrap_local_frame_tail_word_count>
        candidate_bootstrap_local_frame_tail_pcs{
            0x000012B8u, 0x000012BCu, 0x000012C0u, 0x000012C4u
        };
    static constexpr std::array<std::uint32_t, candidate_bootstrap_local_frame_tail_word_count>
        candidate_bootstrap_local_frame_tail_instructions{
            0xE58DA004u, 0xE58D9008u, 0xE58D800Cu, 0xE58D7010u
        };
    static constexpr std::array<std::uint32_t, candidate_bootstrap_local_frame_tail_word_count>
        candidate_bootstrap_local_frame_tail_values{
            0x00000080u, 0x00000000u, 0x00000000u, 0x01170000u
        };
    static_assert(candidate_bootstrap_local_frame_tail_addresses.front()
                      == candidate_bootstrap_local_frame_word_address + 4u
                  && candidate_bootstrap_local_frame_tail_addresses.back() + 4u
                      == candidate_bootstrap_stack_push_base,
        "MACHINE1-AO tail must fill only the exact reserved words before the first push window");

    // MACHINE1-AO device evidence consumed by MACHINE1-AP: all four local-frame
    // tail stores complete and execution reaches instruction 3014 before the new push. The path then loads 0x09FFF400 from [sp], subtracts
    // four to 0x09FFF3FC, and calls the small stack-switch helper at 0x3FC with
    // R1=0xF18. That helper installs SP=0x09FFF3FC and LR=0xF18, then returns to
    // PC=0xF18. Instruction 0xE92D47F0 is STMDB sp!,{r4-r10,lr}, so its exact
    // 32-byte footprint is 0x09FFF3DC..0x09FFF3FB. The first word
    // 0x09FFF3DC=0x0A000000 is directly unresolved on device; the remaining
    // seven values are deterministic from the same AO register snapshot and
    // register list. MACHINE1-AP admits only these eight exact tuples and
    // initialized-only readback. PC=0xF1C and all lower stack space remain
    // fail-closed until separately observed.
    static constexpr std::size_t candidate_bootstrap_relocated_stack_word_count = 8u;
    static constexpr std::size_t candidate_bootstrap_relocated_stack_word_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_bootstrap_relocated_stack_top = 0x09FFF3FCu;
    static constexpr std::uint32_t candidate_bootstrap_relocated_stack_base = 0x09FFF3DCu;
    static constexpr std::uint32_t candidate_bootstrap_relocated_stack_pc = 0x00000F18u;
    static constexpr std::uint32_t candidate_bootstrap_relocated_stack_lr = 0x00000F18u;
    static constexpr std::uint32_t candidate_bootstrap_relocated_stack_instruction = 0xE92D47F0u;
    static constexpr std::array<std::uint32_t, candidate_bootstrap_relocated_stack_word_count>
        candidate_bootstrap_relocated_stack_addresses{
            0x09FFF3DCu, 0x09FFF3E0u, 0x09FFF3E4u, 0x09FFF3E8u,
            0x09FFF3ECu, 0x09FFF3F0u, 0x09FFF3F4u, 0x09FFF3F8u
        };
    static constexpr std::array<std::uint32_t, candidate_bootstrap_relocated_stack_word_count>
        candidate_bootstrap_relocated_stack_values{
            0x0A000000u, 0x0A000000u, 0x00000000u, 0x01170000u,
            0x00000000u, 0x00000000u, 0x00000080u, 0x00000F18u
        };
    static_assert(candidate_bootstrap_relocated_stack_addresses.front()
                      == candidate_bootstrap_relocated_stack_base
                  && candidate_bootstrap_relocated_stack_addresses.back() + 4u
                      == candidate_bootstrap_relocated_stack_top
                  && candidate_bootstrap_relocated_stack_top
                         - candidate_bootstrap_relocated_stack_base
                      == 0x20u,
        "MACHINE1-AP relocated stack must remain exactly eight 32-bit words");

    // MACHINE1-AP device evidence consumed by MACHINE1-AQ: the exact relocated
    // eight-word push completes and execution reaches instruction 3017; then PC=0xF1C executes SUB sp,sp,#0x54,
    // producing SP=0x09FFF388. PC=0xF20 loads r5/r8/r12 from the already
    // initialized 0x0A000FBC local-frame words. The next bus transaction is
    // PC=0xF24, instruction 0xE58DC024 (STR r12,[sp,#0x24]), which writes
    // value 0x00000000 to 0x09FFF3AC. MACHINE1-AQ admits only this exact
    // observed word and initialized-only readback. The remaining 0x54-byte
    // local frame stays unmapped until device evidence reaches it.
    static constexpr std::uint32_t candidate_bootstrap_relocated_local_word_address = 0x09FFF3ACu;
    static constexpr std::size_t candidate_bootstrap_relocated_local_word_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_bootstrap_relocated_local_word_pc = 0x00000F24u;
    static constexpr std::uint32_t candidate_bootstrap_relocated_local_word_lr = 0x00000F18u;
    static constexpr std::uint32_t candidate_bootstrap_relocated_local_word_instruction = 0xE58DC024u;
    static constexpr std::uint32_t candidate_bootstrap_relocated_local_word_value = 0x00000000u;
    static_assert(candidate_bootstrap_relocated_local_word_address
                      == candidate_bootstrap_relocated_stack_base - 0x30u,
        "MACHINE1-AQ local word must stay at the exact AP-observed sp+0x24 address");

    // MACHINE1-AQ device evidence consumed by MACHINE1-AR: after the exact
    // sp+0x24 zero store, execution reaches instruction 3021; F28/F2C consume
    // the initialized local-frame tail.
    // F30 executes LDR r12,[r8,#0x0C] with r8=0x80 and performs an observed
    // physical read at 0x0000008C returning 0x50000000. Correct-era EKA1
    // TRomHeader places iRomBase at +0x8C. F34 then executes
    // 0xE58DC018 (STR r12,[sp,#0x18]) with SP=0x09FFF388 and attempts the
    // exact write 0x50000000 -> 0x09FFF3A0. MACHINE1-AR admits only this
    // observed word and initialized-only readback; the rest of the 0x54-byte
    // local frame remains unmapped.
    static constexpr std::uint32_t candidate_bootstrap_relocated_rom_base_word_address = 0x09FFF3A0u;
    static constexpr std::size_t candidate_bootstrap_relocated_rom_base_word_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_bootstrap_relocated_rom_base_word_pc = 0x00000F34u;
    static constexpr std::uint32_t candidate_bootstrap_relocated_rom_base_word_lr = 0x00000F18u;
    static constexpr std::uint32_t candidate_bootstrap_relocated_rom_base_word_instruction = 0xE58DC018u;
    static constexpr std::uint32_t candidate_bootstrap_relocated_rom_base_word_value = 0x50000000u;
    static_assert(candidate_bootstrap_relocated_rom_base_word_address
                      == candidate_bootstrap_relocated_local_word_address - 0x0Cu,
        "MACHINE1-AR ROM-base word must stay at the exact AQ-observed sp+0x18 address");

    // MACHINE1-AR device evidence consumed by MACHINE1-AS: after the exact
    // F34 ROM-base store, execution reaches instruction 3026; F38 moves r8 to
    // r0 and F3C calls helper 0x2AF8.
    // The helper executes MOV r0,#0x40000000 then returns through LR=0xF40.
    // F40 executes 0xE58D0014 (STR r0,[sp,#0x14]) with SP=0x09FFF388,
    // attempting the exact write 0x40000000 -> 0x09FFF39C. MACHINE1-AS
    // admits only this observed word and initialized-only readback. No MMU,
    // page-table, or wider local-frame meaning is assigned without evidence.
    static constexpr std::uint32_t candidate_bootstrap_relocated_helper_word_address = 0x09FFF39Cu;
    static constexpr std::size_t candidate_bootstrap_relocated_helper_word_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_bootstrap_relocated_helper_word_pc = 0x00000F40u;
    static constexpr std::uint32_t candidate_bootstrap_relocated_helper_word_lr = 0x00000F40u;
    static constexpr std::uint32_t candidate_bootstrap_relocated_helper_word_instruction = 0xE58D0014u;
    static constexpr std::uint32_t candidate_bootstrap_relocated_helper_word_value = 0x40000000u;
    static_assert(candidate_bootstrap_relocated_helper_word_address
                      == candidate_bootstrap_relocated_rom_base_word_address - 0x04u,
        "MACHINE1-AS helper word must stay at the exact AR-observed sp+0x14 address");

    // MACHINE1-AB device evidence: after the nested push returns, firmware
    // reads the already-copied word at 0x0A000010, ORs control bits, and
    // writes 0xB2800021 back with STR r3,[r0,#8] at PC=0x11D4/LR=0x22C4.
    // MACHINE1-AC permits only this exact in-place mutation and only after the
    // four destination bytes have been initialized by the Z copy. No new
    // address range is mapped.
    static constexpr std::uint32_t candidate_bootstrap_post_copy_mutation_address =
        candidate_bootstrap_copy_base + 0x10u;
    static constexpr std::size_t candidate_bootstrap_post_copy_mutation_width = sizeof(std::uint32_t);
    static constexpr std::uint32_t candidate_bootstrap_post_copy_mutation_pc = 0x000011D4u;
    static constexpr std::uint32_t candidate_bootstrap_post_copy_mutation_lr = 0x000022C4u;
    static constexpr std::uint32_t candidate_bootstrap_post_copy_mutation_instruction = 0xE5803008u;
    static constexpr std::uint32_t candidate_bootstrap_post_copy_mutation_value = 0xB2800021u;

    // MACHINE1-AE device evidence: helper 0x21F4..0x2220 reaches control
    // 0x31000023 (low5=3), computes 0xB1000023 and stores at PC=0x2220.
    // The copied firmware table contains only low5 values 1, 2 and 3.
    // MACHINE1-AF extends the exact transform gate to that observed set only;
    // all address/initialization/PC/LR/value guards remain fail-closed and no
    // memory range is widened.
    static constexpr std::uint32_t candidate_bootstrap_record_table_base =
        candidate_bootstrap_copy_base + 0x08u;
    static constexpr std::size_t candidate_bootstrap_record_count = 16u;
    static constexpr std::size_t candidate_bootstrap_record_stride = 0x10u;
    static constexpr std::size_t candidate_bootstrap_record_control_offset = 0x08u;
    static constexpr std::uint32_t candidate_bootstrap_record_loop_mutation_pc = 0x00002220u;
    static constexpr std::uint32_t candidate_bootstrap_record_loop_mutation_lr = 0x00002244u;
    static constexpr std::uint32_t candidate_bootstrap_record_loop_mutation_instruction = 0xE5803008u;
    static constexpr std::uint32_t candidate_bootstrap_record_loop_clear_mask = 0x00000060u;
    static constexpr std::uint32_t candidate_bootstrap_record_loop_or_mask = 0x80000020u;
    static_assert(candidate_bootstrap_record_control_offset < candidate_bootstrap_record_stride
        && candidate_bootstrap_record_table_base
            + static_cast<std::uint32_t>(candidate_bootstrap_record_stride * candidate_bootstrap_record_count)
            <= candidate_bootstrap_copy_base + candidate_bootstrap_copy_size,
        "MACHINE1-AF record-control loop must stay inside the copied 0x108-byte table");

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
        std::uint64_t candidate_bootstrap_copy_read_count() const;
        std::uint64_t candidate_bootstrap_copy_write_count() const;
        std::size_t candidate_bootstrap_copy_initialized_bytes() const;
        std::uint64_t candidate_bootstrap_post_copy_mutation_count() const;
        std::uint64_t candidate_bootstrap_record_loop_mutation_count() const;
        std::uint64_t candidate_bootstrap_stack_read_count() const;
        std::uint64_t candidate_bootstrap_stack_write_count() const;
        std::size_t candidate_bootstrap_stack_initialized_bytes() const;
        std::uint64_t candidate_bootstrap_nested_stack_read_count() const;
        std::uint64_t candidate_bootstrap_nested_stack_write_count() const;
        std::size_t candidate_bootstrap_nested_stack_initialized_bytes() const;
        std::uint64_t candidate_ram_bank_probe_anchor_read_count() const;
        std::uint64_t candidate_ram_bank_sparse_probe_original_read_count() const;
        std::uint64_t candidate_ram_bank_sparse_probe_test_write_count() const;
        std::uint64_t candidate_ram_bank_sparse_probe_anchor_verify_read_count() const;
        std::uint64_t candidate_ram_bank_sparse_probe_readback_count() const;
        std::uint64_t candidate_ram_bank_sparse_probe_restore_write_count() const;
        std::uint64_t candidate_bootstrap_local_frame_word_read_count() const;
        std::uint64_t candidate_bootstrap_local_frame_word_write_count() const;
        std::uint64_t candidate_bootstrap_relocation_read_count() const;
        std::uint64_t candidate_bootstrap_relocation_write_count() const;
        std::size_t candidate_bootstrap_relocation_initialized_bytes() const;
        std::uint64_t candidate_bootstrap_local_frame_tail_read_count() const;
        std::uint64_t candidate_bootstrap_local_frame_tail_write_count() const;
        std::size_t candidate_bootstrap_local_frame_tail_initialized_words() const;
        std::uint64_t candidate_bootstrap_relocated_stack_read_count() const;
        std::uint64_t candidate_bootstrap_relocated_stack_write_count() const;
        std::size_t candidate_bootstrap_relocated_stack_initialized_words() const;
        std::uint64_t candidate_bootstrap_relocated_local_word_read_count() const;
        std::uint64_t candidate_bootstrap_relocated_local_word_write_count() const;
        std::uint64_t candidate_bootstrap_relocated_rom_base_word_read_count() const;
        std::uint64_t candidate_bootstrap_relocated_rom_base_word_write_count() const;
        std::uint64_t candidate_bootstrap_relocated_helper_word_read_count() const;
        std::uint64_t candidate_bootstrap_relocated_helper_word_write_count() const;
        std::uint64_t candidate_post_probe_workspace_read_count() const;
        std::uint64_t candidate_post_probe_workspace_write_count() const;
        std::uint64_t low_vector_shadow_read_count() const;
        std::uint64_t low_vector_shadow_write_count() const;

    private:
        bool range_inside_rom_mapping(std::uint32_t address, std::size_t width, std::size_t &offset) const;
        bool range_inside_ram(std::uint32_t address, std::size_t width, std::size_t &offset) const;
        bool range_inside_candidate_ram_probe(std::uint32_t address, std::size_t width,
                                              std::size_t &offset) const;
        bool range_inside_candidate_bootstrap_copy(std::uint32_t address, std::size_t width,
                                                   std::size_t &offset) const;
        bool range_inside_candidate_bootstrap_relocation(std::uint32_t address, std::size_t width,
                                                         std::size_t &offset) const;
        bool range_inside_candidate_bootstrap_stack(std::uint32_t address, std::size_t width,
                                                    std::size_t &offset) const;
        bool range_inside_candidate_bootstrap_nested_stack(std::uint32_t address, std::size_t width,
                                                           std::size_t &offset) const;
        bool range_inside_candidate_bootstrap_nested_stack_extension(std::uint32_t address,
                                                                     std::size_t width,
                                                                     std::size_t &offset) const;
        bool range_inside_candidate_bootstrap_deep_stack(std::uint32_t address,
                                                         std::size_t width,
                                                         std::size_t &offset) const;
        bool range_inside_candidate_bootstrap_region_stack(std::uint32_t address,
                                                           std::size_t width,
                                                           std::size_t &offset) const;
        bool candidate_ram_bank_sparse_probe_point_index(std::uint32_t address,
                                                          std::size_t &index) const;
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
        std::vector<std::uint8_t> candidate_bootstrap_copy_data_{};
        std::vector<std::uint8_t> candidate_bootstrap_copy_initialized_{};
        std::uint64_t candidate_bootstrap_copy_read_count_ = 0;
        std::uint64_t candidate_bootstrap_copy_write_count_ = 0;
        std::size_t candidate_bootstrap_copy_initialized_bytes_ = 0;
        std::uint64_t candidate_bootstrap_post_copy_mutation_count_ = 0;
        std::uint64_t candidate_bootstrap_record_loop_mutation_count_ = 0;
        std::vector<std::uint8_t> candidate_bootstrap_stack_data_{};
        std::vector<std::uint8_t> candidate_bootstrap_stack_initialized_{};
        std::uint64_t candidate_bootstrap_stack_read_count_ = 0;
        std::uint64_t candidate_bootstrap_stack_write_count_ = 0;
        std::size_t candidate_bootstrap_stack_initialized_bytes_ = 0;
        std::vector<std::uint8_t> candidate_bootstrap_nested_stack_data_{};
        std::vector<std::uint8_t> candidate_bootstrap_nested_stack_initialized_{};
        std::vector<std::uint8_t> candidate_bootstrap_nested_stack_extension_data_{};
        std::vector<std::uint8_t> candidate_bootstrap_nested_stack_extension_initialized_{};
        std::vector<std::uint8_t> candidate_bootstrap_deep_stack_data_{};
        std::vector<std::uint8_t> candidate_bootstrap_deep_stack_initialized_{};
        std::vector<std::uint8_t> candidate_bootstrap_region_stack_data_{};
        std::vector<std::uint8_t> candidate_bootstrap_region_stack_initialized_{};
        std::uint64_t candidate_bootstrap_nested_stack_read_count_ = 0;
        std::uint64_t candidate_bootstrap_nested_stack_write_count_ = 0;
        std::size_t candidate_bootstrap_nested_stack_initialized_bytes_ = 0;
        std::uint64_t candidate_ram_bank_probe_anchor_read_count_ = 0;
        std::array<std::uint32_t, candidate_ram_bank_sparse_probe_point_count>
            candidate_ram_bank_sparse_probe_words_{};
        std::uint64_t candidate_ram_bank_sparse_probe_original_read_count_ = 0;
        std::uint64_t candidate_ram_bank_sparse_probe_test_write_count_ = 0;
        std::uint64_t candidate_ram_bank_sparse_probe_anchor_verify_read_count_ = 0;
        std::uint64_t candidate_ram_bank_sparse_probe_readback_count_ = 0;
        std::uint64_t candidate_ram_bank_sparse_probe_restore_write_count_ = 0;
        std::uint32_t candidate_bootstrap_local_frame_word_data_ = 0;
        bool candidate_bootstrap_local_frame_word_initialized_ = false;
        std::uint64_t candidate_bootstrap_local_frame_word_read_count_ = 0;
        std::uint64_t candidate_bootstrap_local_frame_word_write_count_ = 0;
        std::vector<std::uint8_t> candidate_bootstrap_relocation_data_{};
        std::vector<std::uint8_t> candidate_bootstrap_relocation_initialized_{};
        std::uint64_t candidate_bootstrap_relocation_read_count_ = 0;
        std::uint64_t candidate_bootstrap_relocation_write_count_ = 0;
        std::size_t candidate_bootstrap_relocation_initialized_bytes_ = 0;
        std::array<std::uint32_t, candidate_bootstrap_local_frame_tail_word_count>
            candidate_bootstrap_local_frame_tail_data_{};
        std::array<bool, candidate_bootstrap_local_frame_tail_word_count>
            candidate_bootstrap_local_frame_tail_initialized_{};
        std::uint64_t candidate_bootstrap_local_frame_tail_read_count_ = 0;
        std::uint64_t candidate_bootstrap_local_frame_tail_write_count_ = 0;
        std::size_t candidate_bootstrap_local_frame_tail_initialized_words_ = 0;
        std::array<std::uint32_t, candidate_bootstrap_relocated_stack_word_count>
            candidate_bootstrap_relocated_stack_data_{};
        std::array<bool, candidate_bootstrap_relocated_stack_word_count>
            candidate_bootstrap_relocated_stack_initialized_{};
        std::uint64_t candidate_bootstrap_relocated_stack_read_count_ = 0;
        std::uint64_t candidate_bootstrap_relocated_stack_write_count_ = 0;
        std::size_t candidate_bootstrap_relocated_stack_initialized_words_ = 0;
        std::uint32_t candidate_bootstrap_relocated_local_word_data_ = 0;
        bool candidate_bootstrap_relocated_local_word_initialized_ = false;
        std::uint64_t candidate_bootstrap_relocated_local_word_read_count_ = 0;
        std::uint64_t candidate_bootstrap_relocated_local_word_write_count_ = 0;
        std::uint32_t candidate_bootstrap_relocated_rom_base_word_data_ = 0;
        bool candidate_bootstrap_relocated_rom_base_word_initialized_ = false;
        std::uint64_t candidate_bootstrap_relocated_rom_base_word_read_count_ = 0;
        std::uint64_t candidate_bootstrap_relocated_rom_base_word_write_count_ = 0;
        std::uint32_t candidate_bootstrap_relocated_helper_word_data_ = 0;
        bool candidate_bootstrap_relocated_helper_word_initialized_ = false;
        std::uint64_t candidate_bootstrap_relocated_helper_word_read_count_ = 0;
        std::uint64_t candidate_bootstrap_relocated_helper_word_write_count_ = 0;
        std::vector<std::uint8_t> candidate_post_probe_workspace_data_{};
        std::uint64_t candidate_post_probe_workspace_read_count_ = 0;
        std::uint64_t candidate_post_probe_workspace_write_count_ = 0;
        std::vector<std::uint8_t> low_vector_shadow_data_{};
        std::uint64_t low_vector_shadow_read_count_ = 0;
        std::uint64_t low_vector_shadow_write_count_ = 0;
        std::optional<unresolved_access> first_unresolved_{};
    };
}
