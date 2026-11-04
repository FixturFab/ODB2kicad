# Component rotation: which reading is right

**Short answer: 0.2.0's.** An ODB++ `CMP` rotation is clockwise, seen from the top, on both
sides; odb2kicad 0.2.0 and later write it as KiCad's counter-clockwise footprint angle
(`360 - rot`). The converter Studio vendors writes every footprint at 0 deg instead. Both put
every pad in the same place, so positions match while angles agree only for the parts that sit
at 0 deg. That is the 14-42 percent agreement seen on real boards: it is the share of parts at
0 deg on those boards, not a disagreement about geometry.

Public samples and made-up boards only; no real ODB++ file was read.

## What each converter does

| | Studio's vendored WASM (`v2026.07.13-string-escape`) | 0.2.0 and later |
|---|---|---|
| footprint `(at x y a)` | `a = 0` for every part | `a = 360 - CMP rot` |
| pads | placed at their absolute board offsets in a 0 deg footprint | the offsets turned back into the footprint frame: their library positions |
| pad angle | not set from the flash (a turned non-square pad comes back unturned) | from the copper flash (orient 0-9), else the TOP record |

Both WASM builds run on the public samples (`convertOdbArchive`), compared with
`test/compare_rotation.py <studio.kicad_pcb> <0.2.0.kicad_pcb>`:

| sample | parts | footprint angle agrees | pad positions agree | parts at 0 deg |
|---|---|---|---|---|
| odb-rotation | 8 | 2 (25%) | 8 | 2 |
| odb-kitchen-sink | 3 | 2 (67%) | 3 | 2 |
| odb-output | 2 | 2 (100%) | 2 | 2 |
| odb-dnp | 2 | 2 (100%) | 2 | 2 |

Agreement equals the parts-at-0-deg count on every sample. Studio's angles are all 0.

## Why the clockwise reading is right

1. **The spec.** ODB++ (v8.1, components file, `CMP` record): `<rot>` is the rotation in degrees,
   clockwise, applied about the component origin; `<mirror>` `M` mirrors the component (bottom
   side) before it is rotated. Orient codes 0-3 / 8 of a `P` record turn clockwise too.
2. **The job agrees with itself.** Every component has its package pins in `eda/data` (package
   frame) and its placed pins in `TOP` records (board frame). The right reading maps one onto
   the other. `test/rotation_check.py` tries clockwise, counter-clockwise and 0 deg per part:

   | job | parts | clockwise fits | counter-clockwise fits | 0 deg fits |
   |---|---|---|---|---|
   | designodb_rigidflex (Cadence-style, not KiCad), rotated parts | 451 | **417** | 147 | 32 |
   | designodb_rigidflex, parts at 0 deg | 158 | 143 | 143 | 143 |
   | odb-rotation (kicad-cli 9), rotated top parts | 3 | **3** | 0 | 0 |
   | odb-rotation, rotated bottom parts | 3 | 3 (package already flipped) | 0 | 0 |
   | odb-kitchen-sink, rotated | 1 | **1** | 0 | 0 |

   The counter-clockwise fits on rigid-flex are symmetric parts (180 deg, two-pin passives),
   which fit either way. The 34 rotated and 15 unrotated rigid-flex parts that fit no reading
   have toeprint numbers that do not follow the package's pin order (matched by nearest pin
   instead, clockwise fits 428 of 451); 83 parts (the bottom side among them) carry no
   toeprints to check.
3. **KiCad round trip.** `samples/odb-rotation` is a kicad-cli 9.0.9 export of parts at
   0/30/45/90/270 deg, top and bottom. 0.2.0 gives back the source angles and pads at their
   library positions (`test_simple.sh` checks this); Studio's gives 0 deg for all.

## What it means for Studio

- Pad copper, nets and positions from Studio's converter are right; the footprint angle is not,
  for every rotated part. Anything that reads the footprint angle (placement/pick-and-place
  data, footprint swaps or library matching, courtyard and silk drawn in the footprint frame,
  fixture probe orientation) is wrong by the part's rotation.
- A rotated non-square pad also comes back unturned in Studio's build (fixed in 0.2.0).
- Bottom parts: kicad-cli writes the bottom package already flipped (its `M` must not be applied
  again); other producers follow the spec. The converter decides per part from the package pins
  (see `what-doesnt-work.md`).

**Recommendation (default):** move Studio to the 0.2.0-or-later reading (0.3.0).
Nothing else needs to change for positions; any stored per-part angle from the old build should
be recomputed rather than trusted.
