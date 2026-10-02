# CURRENT HANDOFF

Canonical previous handoff:
`docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-AG-2026-10-02.md`

Active repo/branch:
- repo: `phai-nguyen/-EKA2L1-iOS-fixed`
- branch: `ngage-machine1`
- target: Nokia N-Gage QD RH-29 V04.10

Latest device evidence — AG logic:
- User report filename: `RH29_MACHINE1_AE(3).txt` because harness identity still says AE.
- budget 10000; executed **2246**; stop `unresolved_access`.
- AG exact callsite at `PC=0x2258/LR=0x11E8` succeeded:
  - nested-stack write count increased from 7 to **10**, exactly +3 write32.
  - nested-stack read count is **10**.
- after returning from `0x2258`, firmware executes `0x11E8..0x11FC`, then:
  - `0x11FC: EB00026C` calls `0x1BB4`
  - LR becomes `0x1200`
  - `0x1BB4: E92D40F0 = STMDB sp!, {r4-r7,lr}`
  - entry SP=`0x0A000FBC`
  - exact push footprint=`0x0A000FA8..0x0A000FBB` (20 bytes)
  - first rejected write is `0x0A000FA8`, PC=`0x1BB4`, LR=`0x1200`, write32 value 0.
- Existing nested-stack backing is `0x0A000FAC..0x0A000FBB` (16 bytes), so this new 20-byte push needs only **one new 4-byte word** at `0x0A000FA8..0x0A000FAB`.
- CP15 remains unchanged: `PC=0x2DC8`, instruction `0xEE010F10`, value `0x1272`, emulated; no new remap evidence.

MACHINE1-AH implementation:
- Adds exact fifth-push constants:
  - top `0x0A000FBC`
  - base `0x0A000FA8`
  - size `0x14` = 20 bytes
  - PC `0x1BB4`
  - LR `0x1200`
  - instruction `0xE92D40F0`
- Adds only a **4-byte initialized-only extension backing** at `0x0A000FA8..0x0A000FAB`.
- For the same AH callsite, the remaining four words are stored in the existing nested-stack backing `0x0A000FAC..0x0A000FBB`.
- Older AB/AD/AG callsites cannot access the new extension word.
- Reads from the extension are allowed only after its bytes have been initialized by the exact AH push, matching existing initialized-only stack policy.
- Negative tests:
  - old AG callsite writing `0xFA8` fails;
  - wrong LR at `0xFA8` fails;
  - address below `0xFA8` fails.
- No broad stack/RAM page is mapped.

AH commits:
- `eea8fdbc78dd7d26cba68d64f98d23d7af89745f` — define exact 20-byte push + one-word extension.
- `4e30edefb18c0e6118743214aabd4094dbf7c9b1` — exact extension write/read + reuse nested backing.
- `1eb107233d9cb8573251d7f0d43c3ee615128d3c` — controller tests.

Build:
- run #113, ID `36980268012`
- code SHA `1eb107233d9cb8573251d7f0d43c3ee615128d3c`
- workflow label still `Build NGAGE-MACHINE1-AE`; logic is AH.
- URL: `https://github.com/phai-nguyen/-EKA2L1-iOS-fixed/actions/runs/36980268012`

Next action:
1. Wait for #113 final result.
2. If PASS, device-test exact artifact with budget 10000.
3. New TXT may still be named AE; identify by build #113 / code SHA.
4. Confirm AH passes `0x1BB4` and inspect the next unresolved transaction.
5. Do not widen below `0x0A000FA8` unless a new device access proves it.

Permanent constraints:
- preserve Y FIQ banking fix;
- preserve Z exact 0x108-byte copy;
- preserve AA/AB/AC/AD/AF/AG exact evidence gates;
- low vector shadow remains 36 bytes;
- no broad RAM/stack mapping;
- `0x0C150004` semantics remain unknown; do not infer remap.
