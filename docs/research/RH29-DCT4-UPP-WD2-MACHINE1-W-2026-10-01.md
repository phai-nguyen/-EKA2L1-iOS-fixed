# RH-29 / DCT4 / UPP-WD2 targeted research — MACHINE1-W decision — 2026-10-01

## Scope

This note records only evidence used to decide MACHINE1-W. It does not assign undocumented semantics to 0x08000000 or 0x0C150004.

## Exact RH-29 evidence

Nokia RH-29 service material identifies:
- UPP WD2 as the main processor ASIC;
- one 128 Mbit SDRAM;
- one 128 Mbit flash and one 64 Mbit flash;
- SDRAM component D310;
- flash0 D311 and flash1 D312.

Service references:
- https://manualmachine.com/nokia/ngageqd/201908-service-manual-01-rh29-gen/
- https://manualzilla.com/doc/6002733/6a---baseband-troubleshooting
- https://manualmachine.com/nokia/ngage/3834739-schematic/

RH-29 UFS/JAF logs provide the strongest address evidence found:
- flash0: 0x00000000-0x00FFFFFF;
- flash1: 0x02000000-0x027FFFFF on the 64 Mbit device;
- WD2 secondary loader: w3_2nd.fia;
- AMD algorithm loader: w3_amd.fia.

References:
- https://gsmforum.ru/threads/probleia-v-proshivke-nokia-n-gage-qd.60905/
- https://forum.gsmhosting.com/vbb/f148/qd-cant-flash-mcu-but-can-flash-ppm-why-211842-print/

A JAF RH-29 log after programming reports:
`First 16 bytes: C9 00 00 EA 70 01 00 EA 73 01 00 EA 76 01 00 EA`.
The first little-endian word is therefore `0xEA0000C9`, matching the MACHINE1 RH-29 ROM first word and the current address-zero ROM alias content.

Reference:
- https://bukupinta.blogspot.com/2009/10/nokia-n-gage-qd-rh-29-bluetooth-error.html

## Exact fingerprint search result

Targeted searches for these MACHINE1-V constants did not produce a credible RH-29/UPP-WD2 register definition:
- `0x7037F800`
- `0xB2950489`
- `0x20202112`
- `0x08000000`
- `0x0C150004`
- halfword write value `0x0080`

Therefore:
- `0x08000000` remains semantics-unknown;
- `0x0C150004` remains semantics-unknown;
- `0x0080` remains semantics-unknown;
- no evidence currently permits calling `0x0C150004` a remap register.

The fixed-offset magic-value pattern at 0x08000000 looks configuration-like, but that is only a pattern observation, not a hardware identity.

## Consequence for address zero

MACHINE1-T/V had inferred that returning `0xEA0000C9` on a later data read from address zero was probably wrong because it generated a huge copy count.

The RH-29 flashing evidence changes the confidence of that inference:
- flash0 is genuinely exposed from address zero in WD2 flashing-loader address space;
- the exact first flash word is genuinely `0xEA0000C9`.

This does **not** prove that the normal firmware CPU view can never remap address zero later. It does prove that replacing the current zero alias without an observed transition would be premature.

## MACHINE1-W decision

Choose **W-DIAG**, not W-REMAP.

W must not change hardware responses. It adds:
1. all observed guest bus callbacks in low page 0x00000000-0x00000FFF, including code read, data read and data write;
2. full R0-R12/CPSR/SP/LR trace for callsite PC 0x00000300-0x0000037F around LR 0x344;
3. raw ROM literal words at 0x00000B90-0x00000BC0;
4. instruction index immediately before/after PC 0x00000B68;
5. every condition-passed CP15 instruction encountered before the existing fail-closed CP15 barrier.

This preserves the existing MACHINE1-V memory model and lets the next device log answer whether any real state transition is visible before the later address-zero data read.
