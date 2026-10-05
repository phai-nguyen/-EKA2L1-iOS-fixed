# MACHINE1-BE — continuation from BD device evidence

Branch: `ngage-machine1`. Baseline BD: `15f531cf20194b3feb3a06244f1e33eeb4a36616`.
The old AQ handoff is historical, not the current checkpoint.

## Device evidence

Full input: [RH29_MACHINE1_BD.txt](../evidence/RH29_MACHINE1_BD.txt).
SHA-256: `1602fcebfd4c4146ba3cae17034734828c76bafab5af54e3ea56d170be1fffa5`.
BD copy10 completed 4 writes / 16 initialized bytes. Executed instructions: 3479.
Stop: `unresolved_access`, data_write32 `0x0D000000 -> 0x09FFF5B0`,
PC `0x2344`, LR `0x15FC`, instruction `0xE4803004`.

## Proven instruction chain

A32 entries 43–48: `MOV r3,r4,LSL #4; ADD r3,r3,#8; ADD r0,r7,r3;
MOV r3,r5,LSL #4; ADD r3,r3,#8; ADD r1,r6,r3`.
Observed index r4/r5=10; final snapshot r6=`0x09FFF400`, r7=`0x09FFF508`.
Thus offset=`0xA8`, destination r0=`0x09FFF5B0`, source r1=`0x09FFF4A8`.
Entries 57–58 set r2=`0x10` and BL the copy helper at `0x2334`.
Entries 59–61 round byte size up and divide by four, producing r2=4 words.
Entry 62: `E4913004`, `LDR r3,[r1],#4`, reads `0x0D000000` and advances
r1 to `0x09FFF4AC`. Entry 63: `E4803004`, `STR r3,[r0],#4`, attempts
the exact unresolved store. Post-stop r0=`0x09FFF5B4` reflects writeback;
it is not the address of the failed transaction.

This is a record copy into bootstrap working memory. `0x0D000000` is copied
data, not the hardware address being accessed. No page-table or hardware
register meaning is assigned to the copied control word.

## BE admission and evidence boundaries

BE adds only the eleventh 16-byte copy: initialized relocation source
`0x09FFF4A8..0x09FFF4B7` to destination `0x09FFF5B0..0x09FFF5BF`.
Writes require width32, alignment, PC `0x2344`, LR `0x15FC`, and exact value
match against initialized source. Readback requires initialized destination.
The first value is directly observed by BD. The 16-byte call size and four-word
copy loop are device-observed instruction semantics. Remaining word values
are obtained from initialized source, never hardcoded predictions.

Preserve every prior gate AA..BD, Y FIQ banking, Z exact 0x108 copy,
36-byte low-vector shadow and exact sparse RAM probes. No broad RAM, stack,
page-table, logical ROM or EKA1 RAM VA mappings. `0x0C150004` meaning stays unknown.

## Next device test

Install MACHINE1-BE IPA, select RH-29 V04.10, run budget 10000, export the full
`RH29_MACHINE1_BE.txt`. Confirm `BOOTSTRAP_CALLEE_COPY11_WRITE_COUNT=4` and
`BOOTSTRAP_CALLEE_COPY11_INITIALIZED_BYTES=16`, then investigate the new exact
unresolved transaction. Do not admit copy12 before that device evidence.
