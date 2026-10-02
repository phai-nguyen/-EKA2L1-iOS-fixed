# CURRENT HANDOFF

Canonical previous handoff:
`docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-AG-2026-10-02.md`

Active repo/branch:
- repo: `phai-nguyen/-EKA2L1-iOS-fixed`
- branch: `ngage-machine1`
- target: Nokia N-Gage QD RH-29 V04.10

Latest device report — AI logic:
- uploaded filename: `RH29_MACHINE1_AE(5).txt`
- report identity still says AE; identify actual logic by GitHub run/SHA and execution evidence.
- budget: 10000
- executed: **2501**
- stop: `unresolved_access`.

AI device-PASS evidence:
- execution passes the exact sixth-stack push at `PC=0x1438/LR=0x1BE0`.
- after `0x1438: E92D4010 = STMDB sp!, {r4,lr}`, SP becomes `0x0A000FA0`.
- firmware later executes `0x14A4: E8BD8010` and returns to `0x1BE0`, proving the 8-byte AI backing is both written and read back successfully.
- the helper repeats over successive copied records; AI is not the current blocker.

New firmware dispatch evidence:
- `0x1438` is a per-record helper.
- `0x1440` loads control from record +8.
- `0x1444` extracts low5 with `AND ... #0x1F`.
- `0x1448` subtracts 1.
- `0x144C` compares against 3.
- `0x1450` is an indexed conditional PC load/jump-table dispatch.
- observed low5=3 records return through `0x14A4`.
- observed low5=2 control `0xB2000022` follows a distinct path:
  - `0x1468: E3100101` tests a high control bit;
  - `0x146C: 18BD8010` is a conditional early return;
  - because the condition is not taken for `0xB2000022`, firmware executes
    `0x1470: E1A00004`, then
    `0x1474: EBFFFFAB` -> call `0x1328`, LR=`0x1478`.
- This is strong evidence that low5 values represent distinct firmware region/control classes, but do not assign semantic labels (RAM/MMIO/ROM) as a proven fact yet.

New AJ blocker:
- `0x1328: E92D41F0 = STMDB sp!, {r4-r8,lr}`.
- entry SP: `0x0A000FA0`.
- exact push size: 24 bytes / 6 words.
- exact footprint: `0x0A000F88..0x0A000F9F`.
- first unresolved transaction:
  - kind: data_write
  - width: 32
  - address: `0x0A000F88`
  - PC: `0x00001328`
  - LR: `0x00001478`
  - value: `0x0A0000E8` (matches R4 at entry)
  - cause: unmapped.
- CP15 remains unchanged:
  - PC `0x2DC8`
  - instruction `0xEE010F10`
  - value `0x1272`
  - emulated, MMU-off policy.
- No new evidence changes the unknown semantics of `0x0C150004`.

MACHINE1-AJ implementation:
- exact seventh push:
  - top `0x0A000FA0`
  - base `0x0A000F88`
  - size 24
  - PC `0x1328`
  - LR `0x1478`
  - instruction `0xE92D41F0`.
- adds a dedicated **24-byte region-handler stack backing only at `0x0A000F88..0x0A000F9F`**.
- initialized-only reads.
- only exact AJ callsite can initialize/write the six words.
- AI/AH/AG/older callsites cannot access the new backing.
- address below `0xF88` remains fail-closed.
- no broad stack/RAM mapping.

AJ commits:
- `1bae28233ae9be4fb421c189341f241f8c7c49b7` — exact seventh-push constants.
- `8b0013ae90c32f31459169eeb22a17f4305c9006` — exact 24-byte backing/write gate/readback.
- `c160d361d080f33af07e234fe9304f4081769613` — initialized-only + negative unit tests.

Build:
- run #119
- ID `36989602111`
- code SHA `c160d361d080f33af07e234fe9304f4081769613`
- workflow display name may still say `Build NGAGE-MACHINE1-AE`; logic is MACHINE1-AJ.
- URL: `https://github.com/phai-nguyen/-EKA2L1-iOS-fixed/actions/runs/36989602111`

Next action:
1. Wait for #119 final result.
2. If PASS, device-test its IPA with budget 10000.
3. Share next TXT; filename may still say AE.
4. Confirm execution passes `0x1328`; follow only the next exact unresolved transaction.
5. Do not widen below `0x0A000F88` without new device evidence.
6. Continue reverse-engineering the `0x1438` low5 dispatch and `0x1328` handler from observed code, not guessed hardware semantics.

Permanent constraints:
- preserve Y FIQ banking fix;
- preserve Z exact 0x108-byte copy;
- preserve AA/AB/AC/AD/AF/AG/AH/AI exact evidence gates;
- low vector shadow remains 36 bytes;
- no broad RAM/stack mapping;
- do not infer `0x0C150004` is a remap register;
- Share report UI remains directly below Run probe.
