# CURRENT HANDOFF

Active branch: `ngage-machine1`

Latest device result:
- User-supplied AF-logic report is named `RH29_MACHINE1_AE(1).txt` because the iOS harness identity still says AE.
- Instruction budget: 10,000; executed: 2,122; stop reason: unresolved_access.
- Z bootstrap copy remains exact: 66 x 32-bit writes / 264 initialized bytes at `0x0A000000..0x0A000107`.
- AB exact first-record mutation remains accepted once at `0x0A000010`.
- AF record loop accepted **15** mutations. Together with the separate first-record mutation, all **16** copied record control words were processed.
- The trace proves low5=3 and low5=2 forms execute through the same firmware helper; examples include `0x30000023 -> 0xB0000023`, `0x32000022 -> 0xB2000022`, and `0x30200023 -> 0xB0200023`.
- After the record loop returns, PC `0x11E4` calls `0x2258`; LR becomes `0x11E8`.
- At PC `0x2258`, instruction `0xE92D4030` = `STMDB sp!, {r4,r5,lr}`. Entry SP is `0x0A000FBC`; first unresolved write is `0x0A000FB0`.
- This is exactly the same 12-byte footprint `0x0A000FB0..0x0A000FBB` and same instruction already evidenced for the earlier callsite at PC `0x2228`/LR `0x11E0`.
- CP15 observation remains unchanged: PC `0x2DC8`, instruction `0xEE010F10`, value `0x1272`, emulated. Do not infer remap from `0x0C150004`.

MACHINE1-AG implementation:
- Adds only one new exact stack callsite: PC `0x2258`, LR `0x11E8`, instruction `0xE92D4030`.
- Reuses the existing 12-byte nested-stack footprint `0x0A000FB0..0x0A000FBB`; no address range is widened.
- Existing width/alignment/range gates remain fail-closed.
- Added controller tests proving the exact callsite passes while wrong LR and address below the footprint fail.
- Relevant commits:
  - `6955147e73460ef5fd1f870269cc837522892fdc` — define fourth-stack callsite constants.
  - `b23d7b1935a78560b596e818d005d8fceda54745` — admit exact PC/LR over existing footprint.
  - `90a06fcf2993100d5ee50f0f67359447e03efd66` — controller tests.
- Workflow/harness identity may still display AE; code logic at the final source commit is AG.
- Expected proof on device: nested-stack write count should increase by 3 if the full `STMDB` completes, then the next unresolved access must be analyzed before any further widening.

Safety/evidence constraints:
- Preserve Y FIQ banking fix.
- Preserve Z exact 0x108-byte copy.
- Preserve AA/AB/AC/AD exact stack/writeback gates.
- Do not create a generic 4 KiB stack page or broad RAM mapping.
- `0x0C150004` semantics remain unknown; do not label it a remap register.
