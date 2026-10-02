# CURRENT HANDOFF

Active repo/branch:
- repo: `phai-nguyen/-EKA2L1-iOS-fixed`
- branch: `ngage-machine1`
- target: Nokia N-Gage QD RH-29 V04.10

Latest device report — AJ logic:
- uploaded filename: `RH29_MACHINE1_AE(6).txt`
- harness/report identity still says AE; actual logic is AJ by run/SHA and execution evidence.
- budget: 10000
- executed: **2515**
- stop: `unresolved_access`.

AJ device-PASS:
- exact seventh push at `PC=0x1328/LR=0x1478`,
  `E92D41F0 = STMDB sp!, {r4-r8,lr}`, succeeds.
- SP advances from `0x0A000FA0` to `0x0A000F88`.
- execution proceeds through `0x132C..0x1368`; AJ stack is not the blocker.

New RH-29 low5=2 handler evidence:
- record pointer entering handler: `0x0A0000E8` (record #14).
- `0x1338: E8980090` loads record base/size:
  - R4 = `0x0A000000`
  - R7 = `0x01000000` (16 MiB).
- `0x1330` sets R5=`0x4000`; `0x1334` sets R6=0.
- `0x1344` calls `0x2AA8`; that helper returns R0=`0x1000`.
- `0x1348` adds `0x100`, producing R0=`0x1100`.
- `0x134C` word-aligns it.
- `0x1350` adds record base, producing R4=`0x0A001100`.
- `0x1368: E594C000 = LDR r12,[r4]` performs the first unresolved access.

Current blocker:
- kind: data_read
- width: 32
- address: `0x0A001100`
- PC: `0x00001368`
- LR: `0x00001348`
- cause: unmapped.
- report unresolved value 0 is not device RAM content; it is the unresolved placeholder.

Public Symbian-source corroboration:
- later ARM bootstrap source contains `FindRamBankWidth`, `FindRamBankConfig`,
  and `FindRamBankAddrMap` routines that save original RAM words, perform
  destructive RAM/address-line probes, then restore original contents.
- This strongly corroborates that RH-29 `0x1328` is a RAM-bank/address
  configuration probe, but it is not instruction-identical proof for this
  EKA1/RH-29 firmware.
- Do not assign semantics to `0x0C150004`; still unknown.
- CP15 remains the observed `0x1272` MMU-off write.

MACHINE1-AK implementation:
- exact first probe read only:
  - address `0x0A001100`
  - width32
  - PC `0x1368`
  - LR `0x1348`
  - observed instruction `0xE594C000`.
- returns a **synthetic zero seed**, explicitly labelled as a probe hypothesis,
  not observed device RAM content.
- no neighboring address is mapped.
- no write path is opened.
- wrong PC/LR/address fail closed.
- no 16 MiB RAM mapping is introduced.
- additionally dumps raw ROM words `0x1328..0x13BC` into the next report so
  the full handler can be reverse-engineered without stepping one instruction
  per build.

AK commits:
- `d445a1508456bd979a03d7ba66fb0de7dd96c1e9` — define exact probe anchor.
- `c4670a433734cddce9259fd0d633a2e633dec60e` — exact read-only synthetic seed gate.
- `ed922afde3613fe092f350550c468ce2c488b7ac` — fail-closed model tests.
- `3e61bdb65f51e067880f833794337296cc9fd9a0` — stage raw handler dump structure.
- `8f3700f20c731c4e2c73ef31e4d3db05de902100` — populate/report raw handler + anchor count.
- `1769fb97c09ceac0b7744edaac6f8a4f7cf0f7bd` — report unit tests.
- `691002ef2d2501a96f14cc8935a1b6ff166e19da` — staging preservation tests.

Build:
- final code run: #126
- run ID: `36994082135`
- code SHA: `691002ef2d2501a96f14cc8935a1b6ff166e19da`
- URL: `https://github.com/phai-nguyen/-EKA2L1-iOS-fixed/actions/runs/36994082135`
- workflow name may still say MACHINE1-AE; logic is AK.

Next:
1. Wait for #126 final result; if fail, inspect/fix.
2. If PASS, device-test artifact with budget 10000.
3. New report should show `RAM_BANK_PROBE_ANCHOR_READ_COUNT=1` and the full
   raw `RAM_BANK_HANDLER_DUMP_00...` sequence.
4. Follow the next exact transaction; do not broad-map `0x0A000000..0x0AFFFFFF`
   merely because the record size is 16 MiB.
5. Use the raw handler dump to determine whether the next phase is destructive
   RAM alias/address-line probing and model only the minimal semantics justified.

Permanent constraints:
- preserve Y FIQ R8-R12 banking fix;
- preserve Z exact 0x108-byte copy;
- preserve AA/AB/AC/AD/AF/AG/AH/AI/AJ exact gates;
- low vector shadow remains 36 bytes;
- no broad RAM/stack mapping without direct evidence;
- do not infer `0x0C150004` is a remap register;
- keep Share report UI directly below Run probe.
