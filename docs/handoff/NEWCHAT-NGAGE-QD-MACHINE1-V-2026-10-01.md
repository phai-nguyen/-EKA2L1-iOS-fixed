# NEWCHAT — N-Gage QD RH-29 MACHINE1-V — 2026-10-01

Full handoff also saved in ChatGPT Library as:
`NEWCHAT-NGAGE-QD-MACHINE1-V-2026-10-01.md`.

## Current state
- Active repo: `phai-nguyen/-EKA2L1-iOS-fixed`
- Branch: `ngage-machine1`
- Pre-handoff HEAD: `45fe1a3286cdb80daba9766658ddaf12f8d25217`
- Upstream pin: `EKA2L1/EKA2L1@c5ae62d9c667f6e7060d76be58a2614122a629a4`
- MACHINE1-V build #56 / run `36850283187`: SUCCESS
- V artifact SHA256: `abd682c0fdfad7795fe0d5e49c788fd66d43f0768e5863c239bb87202efb99df`
- Device log `RH29_MACHINE1_V.txt` received and fully read.
- Budget 10,000; executed 1545; still stops on write32 `0x00000020` at PC `0x2344`, LR `0x3C8`, cause `rom_write`.

## V exact early setup fingerprint
```asm
0xB24 MOV  r0,#0x08000000
0xB28 LDR  r1,[pc,#0x68]   ; 0x7037F800
0xB2C STR  r1,[r0]

0xB30 -> r0=0x08000004
0xB34 -> r1=0xB2950489
0xB38 STR

0xB3C -> r0=0x08000008
0xB40 MOV r1,#0x37
0xB44 STR

0xB48 -> r0=0x08000010
0xB4C -> r1=0x20202112
0xB50 STR

0xB54 -> r0=0x08000014
0xB58 -> r1=0x20202112
0xB5C STR

0xB60 -> r0=0x0C150004
0xB64 MOV  r1,#0x80
0xB68 STRH r1,[r0]
0xB6C MOV  pc,lr
```
LR through this setup routine is `0x344`.

## Key interpretation
- The five writes at `0x08000000/+4/+8/+10/+14` are hard-coded early setup writes.
- The current semantic label "candidate SDRAM" for 0x08000000 is now suspect; the pattern looks more register/config-like, but there is still no direct datasheet/source proof.
- `0x0C150004 = 0x0080` is an exact hard-coded halfword operation, but its function is still unknown. Do NOT call it a remap register without corroboration.
- `0x0A000000` remains the stronger candidate RAM-bank probe address because later trace structurally matches public Symbian `FindRamBankWidth/Config/AddrMap`.
- Synthetic ROM alias at low address 0 is likely wrong for later data access: at PC 0x12EC firmware reads address 0, gets ROM opcode `0xEA0000C9`, transforms it to `0xA0000C98`, then to bogus copy count `0x28000326`, causing the low-memory copy to run beyond 0x1C and fail at 0x20.

## Do not do
- Do not extend low shadow blindly to 0x20/0x24/...
- Do not map all low memory writable.
- Do not alias low 0 to 0x08000000 or 0x0A000000 without evidence.
- Do not treat 0x0C150004 as remap trigger without source.
- Do not call 0x0A000000 internal SRAM.

## Next action
1. Targeted source research only: RH-29 / DCT4 / UPP-WD2, exact constants `0x7037F800`, `0xB2950489`, `0x20202112`, address `0x0C150004`.
2. Determine whether 0x08000000 is RAM or an MMIO/config block and what bit/value 0x80 at 0x0C150004 actually controls.
3. If no source proof appears, build **MACHINE1-W-DIAG** only:
   - trace literal pool around B90–BC0;
   - trace callsite around LR 0x344;
   - trace all low-page code/data accesses 0x00000000–0x00000FFF with sequence index;
   - no mapping behavior change.
4. Only build W-REMAP when trigger/backing/range are evidence-backed.

## New-chat command
```
Tiếp tục dự án N-Gage QD RH-29 MACHINE1 từ docs/handoff/NEWCHAT-NGAGE-QD-MACHINE1-V-2026-10-01.md trong repo phai-nguyen/-EKA2L1-iOS-fixed, nhánh ngage-machine1. MACHINE1-V đã có device log; không tự suy diễn 0x0C150004 là remap register. Tiếp tục targeted RH-29/DCT4/UPP-WD2 research rồi mới quyết định MACHINE1-W.
```
