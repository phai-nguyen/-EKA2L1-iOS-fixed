# CURRENT HANDOFF

Canonical handoff:
`docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-AG-2026-10-02.md`

Active repo/branch:
- repo: `phai-nguyen/-EKA2L1-iOS-fixed`
- branch: `ngage-machine1`
- target: Nokia N-Gage QD RH-29 V04.10

Latest device evidence:
- AF logic report is physically named `RH29_MACHINE1_AE(1).txt` because harness identity still says AE.
- budget 10000; executed 2122; stop `unresolved_access`.
- bootstrap copy: 66 write32 / 264 initialized bytes.
- post-copy first-record mutation accepted: 1.
- record-loop mutations accepted: 15.
- therefore all 16 record control words were processed.
- after the loop: `0x11E4 -> 0x2258`, LR=`0x11E8`.
- `0x2258: E92D4030 = STMDB sp!, {r4,r5,lr}`.
- entry SP=`0x0A000FBC`.
- exact footprint `0x0A000FB0..0x0A000FBB`.
- AF blocker: write32 `0x0A000FB0`, PC=`0x2258`, LR=`0x11E8`, cause=unmapped.

MACHINE1-AG:
- admits only the new exact callsite PC=`0x2258` / LR=`0x11E8` / instruction=`0xE92D4030`.
- reuses the already evidenced 12-byte nested-stack footprint; no memory range widened.
- wrong LR / address outside footprint remain fail-closed.
- code commits:
  - `6955147e73460ef5fd1f870269cc837522892fdc`
  - `b23d7b1935a78560b596e818d005d8fceda54745`
  - `90a06fcf2993100d5ee50f0f67359447e03efd66`

Build:
- run #110, ID `36964784995`: **SUCCESS**
- built code SHA: `90a06fcf2993100d5ee50f0f67359447e03efd66`
- run: `https://github.com/phai-nguyen/-EKA2L1-iOS-fixed/actions/runs/36964784995`
- artifact: `EKA2L1-NGAGE-MACHINE1-AE-IPA` (artifact ID `11209700361`)
- workflow/artifact/report still display AE identity; build logic is AG.
- user requested direct GitHub Actions links for every future build.

Next action:
1. Device-test build #110 with budget 10000.
2. Read the new TXT (may still be named `RH29_MACHINE1_AE.txt`).
3. Confirm fourth-stack callsite consumed 3 write32 and identify the next unresolved access.
4. Do not broad-map stack/RAM, do not expand low shadow blindly, and do not infer `0x0C150004` is a remap register.

Permanent evidence constraints:
- preserve Y FIQ R8-R12 banking fix;
- preserve Z exact 0x108 copy;
- preserve AA/AB/AC/AD exact stack/writeback gates;
- preserve AF exact record-loop handling;
- low vector shadow remains 36 bytes;
- `0x0C150004` semantics remain unknown.
