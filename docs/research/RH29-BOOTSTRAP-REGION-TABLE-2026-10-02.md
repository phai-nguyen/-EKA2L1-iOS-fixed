# RH-29 V04.10 — recovered 16-entry bootstrap region table

Date: 2026-10-02  
Evidence base: device report `RH29_MACHINE1_AB.txt`, RH-29 service/flashing documentation, public Symbian bootstrap source.

## 1. What Z actually copied

The 66 words copied from ROM source `0x00000744` to `0x0A000000` are structured, not arbitrary data:

- word 0: `0x00000010` = 16
- word 1: `0x00000001`
- then 16 records, each exactly 4 words / 16 bytes.

The copied size is therefore:

`2 * 4 + 16 * 16 = 264 bytes = 0x108`

which exactly matches MACHINE1-Z device evidence.

## 2. Recovered records

The table below preserves raw firmware values. Field names beyond the first two are deliberately conservative.

| # | Field +0: base | Field +4: size | Field +8: control/flags candidate | Field +C |
|---:|---:|---:|---:|---:|
| 0 | 0x00000000 | 0x0000A000 | 0x32800021 | 0x00000000 |
| 1 | 0x0000A000 | 0x000F6000 | 0x32800021 | 0x00000000 |
| 2 | 0x00100000 | 0x00200000 | 0x32800021 | 0x00000000 |
| 3 | 0x00300000 | 0x00011000 | 0x32800021 | 0x00000000 |
| 4 | 0x00311000 | 0x000EF000 | 0x32800021 | 0x00000000 |
| 5 | 0x00400000 | 0x00B00000 | 0x32800021 | 0x00000000 |
| 6 | 0x02000000 | 0x00100000 | 0x32800021 | 0x00000000 |
| 7 | 0x02100000 | 0x00300000 | 0x32800021 | 0x00000000 |
| 8 | 0x00F00000 | 0x000F0000 | 0x32800021 | 0x00000000 |
| 9 | 0x0C000000 | 0x00200000 | 0x31000023 | 0x00000000 |
| 10 | 0x0D000000 | 0x01000000 | 0x31000023 | 0x00000000 |
| 11 | 0x02400000 | 0x00400000 | 0x31000023 | 0x00000000 |
| 12 | 0x02400000 | 0x00400000 | 0x33400023 | 0x00000000 |
| 13 | 0x08000000 | 0x00001000 | 0x30000023 | 0x00000000 |
| 14 | 0x0A000000 | 0x01000000 | 0x32000022 | 0x00000000 |
| 15 | 0x0C120000 | 0x00001000 | 0x30200023 | 0x00000000 |

## 3. Function roles recovered from device execution

### 0x12FC — address-in-record test

The executed routine uses the first two record words as an interval:

- `r3 = record pointer`
- load `[r3+0]`
- compare queried address
- load `[r3+4]`
- add base + size
- compare against the upper bound
- return boolean.

This is strong evidence that record fields +0/+4 are a base address and size.

### 0x22A0 — find record containing address

Observed behavior:

- `r5 = table + 8` -> first 16-byte record
- `r4 = [table]` -> record count, observed as 16
- calls the 0x12FC range test
- returns the matching record pointer when the query lies inside a record.

For query address 0, it returns record 0 at copied address `0x0A000008`.

### 0x11C8–0x11D4 — mutate field +8 of the selected record

After the record-lookup helper returns record 0:

- `0x11C8: LDR r3,[r0,#8]` reads raw field +8 = `0x32800021`
- `0x11CC: ORR ...` contributes immediate `0x80000020`
- `0x11D0: ORR ...` contributes immediate `0x20000000`
- result becomes `0xB2800021`
- `0x11D4: STR r3,[r0,#8]` writes it back to the same record.

Therefore field +8 is mutable control/attribute state. Its exact RH-29 meaning is not yet proven.

## 4. Hardware-map implications

### 4.1 0x0A000000 is now the strongest SDRAM candidate

Record 14 declares exactly:

- base `0x0A000000`
- size `0x01000000` = 16 MiB.

RH-29 service documentation states one 128-Mbit SDRAM, which is 16 MiB. The bootstrap also uses this region for:

- the copied 0x108-byte table,
- the first stack,
- the nested stack.

This is much stronger evidence for main SDRAM at `0x0A000000–0x0AFFFFFF` than the older MACHINE1 placeholder at `0x08000000`.

### 4.2 0x08000000 is only a 4-KiB firmware-declared region

Record 13 is:

`0x08000000 + 0x1000`

The firmware performs the five early writes already observed there. The recovered table now conflicts with the old model name `candidate_sdram_base=0x08000000` / 16 MiB. The safer interpretation is that `0x08000000` is an early hardware/register window candidate, not main SDRAM.

Do not rename/register-identify the device yet without direct WD2/UPP documentation.

### 4.3 0x0C150004 lies inside a larger firmware-declared region

Record 9 spans:

`0x0C000000–0x0C1FFFFF`

The observed halfword write address `0x0C150004` lies inside this span.

This supports only that the address belongs to a firmware-described hardware/memory region. It still does **not** identify `0x0C150004` as a remap register.

### 4.4 Flash ranges line up with independent RH-29 flashing evidence

Independent flashing logs identify:

- flash 0 around `0x00000000–0x00FFFFFF`
- flash 1 / second device around `0x02000000–0x027FFFFF`.

Multiple recovered records cover these same physical address families, strongly supporting that the table describes low-level physical regions/mappings rather than application-level data.

The exact purpose of every split/overlap is not yet known.

## 5. Field +8: what is known and what is not

Raw values:

- `0x32800021`
- `0x31000023`
- `0x33400023`
- `0x30000023`
- `0x32000022`
- `0x30200023`.

Their low two bits (`1/2/3`) resemble ARMv4/v5 first-level translation-descriptor type encodings (coarse/section/fine), and several values are aligned in ways that make this technically interesting.

However, there is not yet enough RH-29-specific evidence to claim that field +8 is a raw ARM PDE. In particular, the firmware mutates bit 31 in record 0, and the high fields may contain Nokia platform-specific control data.

Treat “raw ARM page-directory descriptor” as a hypothesis only.

## 6. Important false lead rejected

Modern public Symbian source defines the numeric value `0x80000020` as one ARMv6K/ARMv7 platform-specific memory-type mapping combination.

That is **not** sufficient to label the RH-29 `0x80000020` OR immediate:

- RH-29 uses ARM920T / ARMv4T-era hardware.
- the modern Symbian source explicitly scopes those mapping-5/6/7 definitions to later memory-type-remapping CPUs.

The numeric match is interesting but must not be imported as semantics into RH-29.

## 7. New model direction

Do **not** immediately turn every table entry into a permissive bus mapping.

Next architecture change should be evidence-gated:

1. Keep AC exact post-copy mutation and collect its device stop.
2. Add a structured table decoder/report so every subsequent mutation can be attributed to a record/field.
3. Prepare a candidate main-RAM backend for `0x0A000000–0x0AFFFFFF` with write-initialized-only reads.
4. Demote the old `0x08000000` “candidate SDRAM” label to a 4-KiB hardware-window candidate.
5. Before enabling broad 16-MiB RAM writes, correlate AC/next device accesses with record 14 and preferably find another WD2-family ROM/table for cross-device comparison.
6. Keep `0x0C150004` semantics unknown.

## 8. External source anchors

Public Symbian bootstrap source used for comparison:

- `kernel/eka/include/kernel/kernboot.h` — physical RAM/ROM bank structures and super-page boot fields.
- `kernel/eka/include/kernel/arm/bootdefs.h` — bootstrap parameters and memory-related definitions.
- `kernel/eka/include/kernel/arm/bootcpu.inc` — ARM bootstrap mapping encodings.
- `kernel/eka/kernel/arm/bootutils.s` — bootstrap memory-copy, RAM detection, mapping and page-table routines.

Independent RH-29 evidence:

- Nokia RH-29 service manual: UPP WD2, one 128-Mbit SDRAM, 128-Mbit + 64-Mbit flash.
- UFS/JAF logs: low flash windows at 0x00000000 and 0x02000000/0x027FFFFF.



## 9. MACHINE1-AC device result

AC validates the AB-derived post-copy mutation gate:

- `BOOTSTRAP_POST_COPY_MUTATION_ACCEPTED_COUNT=1`
- execution advances through `0x11D8` and `0x11DC`
- the next call enters `0x2228` with `LR=0x11E0`
- `0x2228 = E92D4030 = STMDB sp!, {r4,r5,lr}`
- SP before the instruction is `0x0A000FBC`
- the exact 12-byte footprint is therefore `0x0A000FB0–0x0A000FBB`
- first rejected transaction is a 32-bit write to `0x0A000FB0`.

Crucially, this 12-byte footprint is wholly inside the already evidenced AA/AB nested-stack window
`0x0A000FAC–0x0A000FBB`. This is new evidence for stack reuse, not evidence for a wider RAM mapping.

MACHINE1-AD therefore permits only the exact `PC=0x2228/LR=0x11E0` third STMDB push while reusing the
existing 16-byte nested-stack backing. It does not widen the mapped address range.

AC stopped after 1820 executed instructions. The CP15 evidence remains unchanged:
`MCR p15,0,r0,c1,c0,0` at `0x2DC8` with value `0x1272`. No new evidence identifies
`0x0C150004` as a remap register.


## 10. Correct-era EKA1/Symbian 6.1 validation

Research against the preserved Series 60 6.1 SDK (`ngagesdk/sdk`) gives a
contemporaneous `TRomHeader`, rather than relying on the later EKA2 layout.

`Epoc32/Include/e32rom.h` defines a 0x100-byte header whose relevant offsets are:

| Offset | EKA1/Symbian 6.1 field |
|---:|---|
| `+0x80` | `iVersion` |
| `+0x84` | `iTime` |
| `+0x8C` | `iRomBase` |
| `+0x90` | `iRomSize` |
| `+0x94` | `iRomRootDirectoryList` |
| `+0x98` | `iKernDataAddress` |
| `+0x9C` | `iKernStackAddress` |
| `+0xA0` | `iPrimaryFile` |
| `+0xA4` | `iSecondaryFile` |

MACHINE1-AP records a device read of physical low-ROM address `0x00000090`
at PC `0x1200`, returning `0x01170000`. With the correct-era header this
is no longer an anonymous word: it is `TRomHeader::iRomSize`.

The AO local-frame tail subsequently preserves the same `0x01170000` value at
`0x0A000FCC`. In the F18 routine, `LDR r4,[r0,#0x10]` with
`r0=0x0A000FBC` therefore recovers the ROM-size value.

This semantic upgrade is source-backed and does not add any new bus mapping.

## 11. EKA1 logical ROM base 0x50000000 is independently corroborated

The same Series 60 6.1 SDK contains `Shared/EPOC32/Tools/hpsym.pl`.
That tool sets:

```
my $rombase = 0x50000000;
```

and documents the argument as the logical-address offset from the physical ROM
address, with `0x50000000` as the default and zero for single-process builds.

This matches all of the following independent observations:

- RH-29 bootstrap currently executes the ROM at low physical addresses while
  CP15 C1 still shows the MMU disabled.
- `TRomHeader::iRomBase` parsed from this RH-29 image is `0x50000000`.
- historical N-Gage reverse engineering places the post-MMU ROM image at
  `0x50000000–0x57FFFFFF`.
- modern EKA2L1's preserved EKA1 constants also use
  `rom_eka1 = 0x50000000`.

Therefore `0x50000000` should be treated as the intended EKA1 logical ROM
mapping, not as evidence that early reset execution physically starts there.

## 12. Historical EKA1 virtual layout cross-check

Two independent preservation paths reproduce the old N-Gage/EKA1 layout:

1. the historical THC-derived N-Gage map preserved by Engemu; and
2. EKA2L1's explicit EKA1 constants.

They agree on key regions:

- page directory: `0x41000000`
- page-table info: `0x41080000`
- page tables: `0x42000000–0x423FFFFF`
- ROM: starts at `0x50000000`.

EKA2L1 commit `17c005e982921010f40cdb15b51f433d22cb65ee`
("kernel: Support page table injection for EKA1") further documents its EKA1
page-directory emulation:

- low descriptor value 1 = page table;
- low descriptor value 2 = section;
- page-table-info low six bits carry attributes.

This is useful architectural corroboration for why RH-29's low control values
1/2/3 are interesting. It is **not** proof that RH-29 field +8 is itself a raw
ARM first-level descriptor: the high bits do not behave as a simple physical
descriptor base, and the firmware mutates the field before later use.

## 13. Stronger classification of RH-29 low5 control classes

Later MACHINE1 device execution plus independent RH-29 UFS flashing logs now
allow a more precise, but still conservative, classification.

### low5 = 2

Record 14 is:

`0x0A000000, 0x01000000, 0x32000022, 0`

and this path reaches handler `0x1328`, whose executed instructions perform
the power-of-two sparse address-line RAM test. This is strong device evidence
that low5=2 selects a **RAM-bank detection path/class**.

### low5 = 1

The low5=1 records cover most of the low physical flash windows. Independent
RH-29 UFS logs identify:

- `Fl0: 0x00000000–0x00FFFFFF` — AMD 29BDS128J;
- `Fl1: 0x02000000–0x027FFFFF` — AMD 29BDS064J.

This gives strong correlation between low5=1 and a
**flash/ROM-like boot handling class**, but it is not a one-to-one physical
media label: the upper `0x02400000–0x027FFFFF` portion of the second flash
device also appears in low5=3 records.

### low5 = 3

These entries include `0x0C000000`, `0x0D000000`, `0x08000000`,
`0x0C120000`, and the overlapping `0x02400000–0x027FFFFF` region.
The safest current working description is **special/fixed/hardware mapping
path/class**. Calling it simply "MMIO" would be too strong.

The full high-bit packing of `0x32800021`, `0x31000023`,
`0x33400023`, `0x30000023`, `0x32000022`, and `0x30200023`
remains unresolved.

## 14. F18 path and the next evidence boundary

MACHINE1-AP reaches the F18 routine with:

- `SP=0x09FFF3FC` before `0xF18`;
- `0xF18: E92D47F0` exact eight-word push;
- `0xF1C: E24DD054` -> `SP=0x09FFF388`;
- `0xF20: E8901120` loads the existing local-frame words;
- `0xF24: E58DC024` attempts the first new write
  `0x09FFF3AC = 0`.

MACHINE1-AQ therefore admits only that exact F24 store. It does **not** map
the whole 0x54-byte stack reservation.

If AQ passes F24, the already visible ROM instructions predict the following
sequence, but these remain predictions until device execution confirms them:

- `F28` reads `0x0A000FC8` -> expected zero;
- `F2C` reads `0x0A000FCC` -> `0x01170000` (now identified as ROM size);
- `F30` reads physical ROM `0x8C` -> expected
  `TRomHeader::iRomBase = 0x50000000`;
- `F34` would then store that value at `SP+0x18 = 0x09FFF3A0`;
- `F3C` branches to `0x2AF8`.

No F34 stack word or 0x2AF8 behavior is admitted before the AQ device report.

## 15. Current safety/evidence boundary

The research now strongly predicts that F18 is assembling ROM/MMU bootstrap
metadata immediately before the call at `0x2AF8`. However, the exact role of
`0x2AF8` is deliberately unnamed until its executed body or bus accesses are
captured.

In particular:

- do not pre-map `0x41000000`, `0x42000000`, `0x50000000`, or
  `0x60000000` merely because the eventual EKA1 virtual layout is known;
- do not turn the 16-record physical table into permissive memory ranges;
- do not identify `0x0C150004` as a remap register;
- continue one exact device-observed access at a time.
