# CURRENT HANDOFF

Canonical previous handoff:
`docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-AG-2026-10-02.md`

Active repo/branch:
- repo: `phai-nguyen/-EKA2L1-iOS-fixed`
- branch: `ngage-machine1`
- target: Nokia N-Gage QD RH-29 V04.10

Latest device report:
- uploaded filename: `RH29_MACHINE1_AE(4).txt`
- report identity still says `RH29_MACHINE1_AE`; identify newer logic by GitHub run/SHA and execution evidence.
- budget: 10000
- executed: **2257**
- stop: `unresolved_access`.

AH device-PASS evidence:
- nested-stack write count is **14**, up from AG's 10.
- This matches AH's fifth push at `PC=0x1BB4/LR=0x1200`:
  - `0xE92D40F0 = STMDB sp!, {r4-r7,lr}`
  - entry SP `0x0A000FBC`
  - resulting SP `0x0A000FA8`
  - upper four words land in existing nested backing `0x0A000FAC..0x0A000FBB`
  - new low word `0x0A000FA8..0x0A000FAB` is therefore accepted.
- execution continues through `0x1BB8..0x1BDC`; AH is not the current blocker.

New AI device evidence:
- at `0x1BDC`, instruction `0xEBFFFE15` calls `0x1438`; LR becomes `0x1BE0`.
- `0x1438: E92D4010 = STMDB sp!, {r4,lr}`.
- entry SP: `0x0A000FA8`.
- exact push size: 8 bytes.
- exact new footprint: `0x0A000FA0..0x0A000FA7`.
- first unresolved transaction:
  - kind: data_write
  - width: 32
  - address: `0x0A000FA0`
  - PC: `0x00001438`
  - LR: `0x00001BE0`
  - value: `0x00000010`
  - cause: unmapped.
- CP15 remains unchanged:
  - PC `0x2DC8`
  - instruction `0xEE010F10`
  - value `0x1272`
  - emulated with MMU-off policy.
- No new evidence changes the unknown semantics of `0x0C150004`.

MACHINE1-AI implementation:
- exact sixth push:
  - top `0x0A000FA8`
  - base `0x0A000FA0`
  - size 8
  - PC `0x1438`
  - LR `0x1BE0`
  - instruction `0xE92D4010`.
- adds a dedicated **8-byte deep-stack backing only at `0x0A000FA0..0x0A000FA7`**.
- initialized-only reads.
- only exact AI callsite can initialize/write the two words.
- AH/AG/older callsites cannot access the new backing.
- address below `0xFA0` remains fail-closed.
- no broad stack/RAM mapping.

AI commits:
- `895c4fb68a771d32092dea8432177d06e9a71542` — exact sixth-push constants.
- `03e9e0bb7e50a989a7e771f2aa3c632d7e781c9a` — exact 8-byte deep-stack backing/gate.
- `34ddb8cd22b900542feb813bf97e784b2b7831ac` — initialized-only + negative unit tests.

Build:
- run #116
- ID `36983862084`
- code SHA `34ddb8cd22b900542feb813bf97e784b2b7831ac`
- workflow display name may still say `Build NGAGE-MACHINE1-AE`; logic is MACHINE1-AI.
- URL: `https://github.com/phai-nguyen/-EKA2L1-iOS-fixed/actions/runs/36983862084`

Next action:
1. Wait for #116 final result.
2. If PASS, device-test its IPA with budget 10000.
3. Share next TXT; filename may still say AE.
4. Confirm execution passes `0x1438`, then follow only the next exact unresolved transaction.
5. Do not widen stack below `0x0A000FA0` without new device evidence.

Permanent constraints:
- preserve Y FIQ banking fix;
- preserve Z exact 0x108-byte copy;
- preserve AA/AB/AC/AD/AF/AG/AH exact evidence gates;
- low vector shadow remains 36 bytes;
- no broad RAM/stack mapping;
- do not infer `0x0C150004` is a remap register;
- Share report UI remains directly below Run probe.
