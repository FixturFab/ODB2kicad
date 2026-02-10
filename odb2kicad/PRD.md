# PRD: odb2kicad — ODB++ to KiCad PCB Converter

## 1. Project Overview

### 1.1 Goal
Build a standalone C++17 command-line tool that reads a pre-extracted ODB++ directory tree and writes a KiCad `.kicad_pcb` s-expression file. The tool must be WASM-portable (no platform-specific APIs, no filesystem beyond basic `<fstream>`/`<dirent.h>`).

### 1.2 Non-Goals
- Extracting `.tgz`/`.zip` ODB++ archives (input must be pre-extracted)
- Recovering editable text from rasterized ODB++ text (stroke-font text will be imported as line segments)
- Round-tripping (KiCad → ODB++ → KiCad identity)
- Handling inner copper layers beyond F.Cu/B.Cu in the initial implementation
- Zone reconstruction (copper fills will be imported as filled polygons)

### 1.3 Constraints
- C++17, zero external dependencies (no Boost, no wxWidgets)
- Must compile with both GCC/Clang (native) and Emscripten (WASM)
- Single-threaded (no pthreads requirement)

### 1.4 Repository Context
- All work is on the **`odb-to-kicad`** branch (branched from `master`)
- The converter lives in `/root/kicad-test/odb2kicad/` within the existing kicad-test repo
- Sample ODB++ test data is at `samples/odb-output/` (simple) and `samples/odb-kitchen-sink/` (complex)
- Reference KiCad PCB is at `samples/test.kicad_pcb`

---

## 2. Architecture

### 2.1 Directory Layout
```
odb2kicad/
├── CMakeLists.txt
├── PRD.md
├── src/
│   ├── main.cpp              # CLI entry point
│   ├── odb_parser/
│   │   ├── odb_parser.h      # Top-level parser: reads ODB++ tree → OdbDesign
│   │   ├── odb_parser.cpp
│   │   ├── info_parser.h/cpp       # misc/info
│   │   ├── matrix_parser.h/cpp     # matrix/matrix
│   │   ├── stephdr_parser.h/cpp    # steps/<step>/stephdr
│   │   ├── profile_parser.h/cpp    # steps/<step>/profile
│   │   ├── features_parser.h/cpp   # steps/<step>/layers/<name>/features
│   │   ├── components_parser.h/cpp # steps/<step>/layers/comp_+_*/components
│   │   ├── eda_parser.h/cpp        # steps/<step>/eda/data
│   │   ├── netlist_parser.h/cpp    # steps/<step>/netlists/cadnet/netlist
│   │   └── symbols.h/cpp           # Symbol name decoder (r<d>, rect<w>x<h>, etc.)
│   ├── model/
│   │   ├── odb_design.h       # In-memory ODB++ design model
│   │   └── kicad_pcb.h        # In-memory KiCad PCB model
│   ├── writer/
│   │   ├── kicad_writer.h     # KiCad s-expression writer
│   │   └── kicad_writer.cpp
│   └── util/
│       ├── string_utils.h/cpp # trim, split, uppercase, etc.
│       └── coord.h            # Coordinate transform helpers
└── test/
    └── test_simple.sh         # Smoke test against samples/odb-output
```

### 2.2 Data Flow
```
ODB++ directory tree
        │
        ▼
  ┌─────────────┐
  │  ODB Parser  │  Parse all files → OdbDesign (in-memory model)
  └──────┬──────┘
         │
         ▼
  ┌─────────────┐
  │  Transformer │  Apply coord transforms, remap nets/layers
  └──────┬──────┘
         │
         ▼
  ┌─────────────┐
  │ KiCad Writer │  Emit .kicad_pcb s-expression
  └─────────────┘
```

### 2.3 CLI Interface
```
Usage: odb2kicad <odb-directory> [output.kicad_pcb]
  If output is omitted, writes to stdout.
```

---

## 3. ODB++ Input Format Specification

All examples below are from `samples/odb-output/` (the simple 2-resistor test board) and `samples/odb-kitchen-sink/` (complex board with vias, arcs, copper fills).

### 3.1 `misc/info` — Project Metadata

**Path**: `<odb-root>/misc/info`

**Format**: One `KEY=VALUE` per line. No quoting, no escaping.

**Real example** (`samples/odb-output/misc/info`):
```
JOB_NAME=job
UNITS=MM
ODB_VERSION_MAJOR=8
ODB_VERSION_MINOR=1
ODB_SOURCE=KiCad EDA
CREATION_DATE=20260210.001419
SAVE_DATE=20260210.001419
SAVE_APP=KiCad EDA 9.99.0-unknown
```

**Relevant fields**:
| Field | Meaning | Used by converter |
|-------|---------|-------------------|
| `UNITS` | Global units (`MM` or `INCH`) | Yes — all coordinates |
| `JOB_NAME` | Job name | No |
| `ODB_VERSION_MAJOR/MINOR` | ODB++ spec version | No |
| `ODB_SOURCE` | Exporting application | No |

### 3.2 `matrix/matrix` — Layer Stack Definition

**Path**: `<odb-root>/matrix/matrix`

**Format**: Brace-delimited blocks — one `STEP {}` then multiple `LAYER {}`.

**Grammar**:
```
file      = step_block layer_block*
step_block = "STEP" "{" kv_pair* "}"
layer_block = "LAYER" "{" kv_pair* "}"
kv_pair   = KEY "=" VALUE NEWLINE
```

**Real example** (abbreviated from `samples/odb-output/matrix/matrix`):
```
STEP {
    COL=1
    NAME=PCB
}

LAYER {
    ROW=1
    CONTEXT=BOARD
    TYPE=COMPONENT
    NAME=COMP_+_TOP
    OLD_NAME=
    POLARITY=POSITIVE
    COLOR=0
}

LAYER {
    ROW=5
    CONTEXT=BOARD
    TYPE=SIGNAL
    NAME=F.CU
    OLD_NAME=
    POLARITY=POSITIVE
    COLOR=0
}

LAYER {
    ROW=6
    CONTEXT=BOARD
    TYPE=DIELECTRIC
    NAME=DIELECTRIC_1
    OLD_NAME=
    POLARITY=POSITIVE
    DIELECTRIC_TYPE=CORE
    COLOR=0
}

LAYER {
    ROW=32
    CONTEXT=BOARD
    TYPE=DRILL
    NAME=DRILL_PLATED_F.CU-B.CU
    OLD_NAME=
    POLARITY=POSITIVE
    START_NAME=F.CU
    END_NAME=B.CU
    COLOR=0
}
```

**Layer TYPE values** (observed):
| TYPE | Meaning | KiCad mapping |
|------|---------|---------------|
| `COMPONENT` | Component placement layer | Not a physical layer |
| `SILK_SCREEN` | Silkscreen | `F.SilkS` / `B.SilkS` |
| `SOLDER_PASTE` | Solder paste | `F.Paste` / `B.Paste` |
| `SOLDER_MASK` | Solder mask | `F.Mask` / `B.Mask` |
| `SIGNAL` | Copper signal | `F.Cu` / `B.Cu` / `In*.Cu` |
| `DIELECTRIC` | Dielectric core/prepreg | Not exported |
| `DRILL` | Drill layer | Via/pad drill info |
| `DOCUMENT` | Misc document layer | Various user layers |

**Layer CONTEXT values**: `BOARD` (manufacturing layers), `MISC` (auxiliary layers).

**Drill layers** have additional fields: `START_NAME`, `END_NAME` (the copper layers spanned).

### 3.3 `steps/<step>/stephdr` — Step Header

**Path**: `<odb-root>/steps/pcb/stephdr`

**Format**: `KEY=VALUE` pairs.

**Real example**:
```
LEFT_ACTIVE=0
AFFECTING_BOM=
RIGHT_ACTIVE=0
TOP_ACTIVE=0
Y_ORIGIN=0
AFFECTING_BOM_CHANGED=0
X_ORIGIN=0
Y_DATUM=0
X_DATUM=0
BOTTOM_ACTIVE=0
UNITS=MM
```

**Relevant fields**:
| Field | Meaning |
|-------|---------|
| `X_ORIGIN`, `Y_ORIGIN` | Coordinate origin offset |
| `X_DATUM`, `Y_DATUM` | Datum point |
| `UNITS` | Step-local units (overrides global if set) |

### 3.4 `steps/<step>/profile` — Board Outline

**Path**: `<odb-root>/steps/pcb/profile`

**Format**: Surface contour using `S`/`OB`/`OS`/`OE`/`SE` records.

**Grammar**:
```
file       = "UNITS=" unit NL header_comments "F" count NL features
features   = surface*
surface    = "S" polarity flags NL contour+ "SE" NL
contour    = "OB" x y direction NL segment* "OE" NL
segment    = "OS" x y NL
direction  = "I"  |  "H"          # I = Island (outer), H = Hole (inner cutout)
polarity   = "P"  |  "N"          # P = positive, N = negative
```

**Real example** (`samples/odb-output/steps/pcb/profile`) — simple rectangle:
```
UNITS=MM
#
#Num Features
#
F 1
#
#Layer features
#
S P 0
OB 130.0 -90.0 I
OS 130.0 -90.0
OS 130.0 -110.0
OS 90.0 -110.0
OS 90.0 -90.0
OS 130.0 -90.0
OE
SE
```

This defines a rectangle from (90,90) to (130,110) in KiCad coordinates (after Y-negation).

**Complex example** (`samples/odb-kitchen-sink/steps/pcb/profile`) — polygon with holes:
```
S P 0
OB 106.25 -29.40 I             # Outer boundary (Island)
OS 106.25 -29.40
OS 215.50 -49.50
...
OS 106.25 -29.40
OB 122.15 -109.95 H            # First hole
OS 122.15 -109.95
...
OS 122.15 -109.95
OB 224.40 -93.61 H             # Second hole (circular, approximated)
OS 224.40 -93.61
...
OS 224.40 -93.61
OE
SE
```

**Key points**:
- `OB ... I` = outer boundary (island)
- `OB ... H` = hole (cutout)
- First and last `OS` coordinates are the same (closed contour)
- Coordinates are in ODB++ space (Y-negated vs KiCad)
- Circular holes are approximated as many-segment polygons

### 3.5 `steps/<step>/layers/<name>/features` — Layer Features

**Path**: `<odb-root>/steps/pcb/layers/<layer-name>/features`

This is the primary data file. Each layer has one. It contains symbol definitions, attribute definitions, and feature records.

**Grammar**:
```
file           = "UNITS=" unit NL sections
sections       = feature_count symbols attributes strings features
feature_count  = "F" INTEGER NL
symbols        = ("$" INDEX " " symbol_name NL)*
attributes     = ("@" INDEX " " attr_name NL)*
strings        = ("&" INDEX " " text NL)*
features       = feature_record*
feature_record = pad | line | arc | surface
```

#### 3.5.1 Symbol Definitions (`$`)

Symbols define pad/aperture shapes. The name encodes geometry:

| Pattern | Meaning | Example | Decoded |
|---------|---------|---------|---------|
| `r<d>` | Round, diameter d (in µm×1000) | `r250.0` | Circle ⌀0.25mm |
| `rect<w>x<h>` | Rectangle w×h | `rect1025.0x1400.0` | Rect 1.025×1.4mm |
| `rect<w>x<h>xr<r>` | Rounded rect, corner radius r | `rect1025.0x1400.0xr250.0` | Rect 1.025×1.4mm, r=0.25mm |
| `oval<w>x<h>` | Oval (stadium) w×h | `oval800.0x1200.0` | Oval 0.8×1.2mm |
| `rect<w>x<h>xr<r>xr<r>xr<r>xr<r>` | Chamfered rect | *(not in test data)* | Per-corner radii |
| `donut_rc<w>x<h>x<lw>xr<r>` | Donut (ring) | `donut_rc40050.0x20050.0x50.0xr25.0` | Ring shape |

**Dimension encoding**: Values are in **thousandths of the unit** (i.e., when UNITS=MM, value `1025.0` means 1.025mm; effectively multiply by 0.001 to get mm). This is derived from KiCad's internal nanometer representation: a 1.025mm pad → 1025000nm → stored as `1025.0` in ODB++ (units of 1000nm = 1µm, but represented as fractional mm×1000).

**Real example** (`f.cu`):
```
$0 rect1025.0x1400.0xr250.0
$1 r250.0
```
Symbol 0 = rounded rectangle 1.025×1.4mm with 0.25mm corner radius.
Symbol 1 = round ⌀0.25mm (trace width).

**Kitchen-sink example** (`f.cu`):
```
$0 r600.0
$1 rect800.0x950.0xr200.0
$2 rect2032.0x2032.0
$3 r2540.0
$4 r2032.0
$5 rect2540.0x2540.0
```

#### 3.5.2 Attribute Definitions (`@`) and Text Strings (`&`)

```
@0 .pad_usage
@1 .smd

&0 R1
&1 R2
```

- `@n name` — defines attribute index `n` with name `name`
- `&n text` — defines text string index `n` with value `text`
- Feature records reference these by index in trailing `;` clause

**Common attributes**:
| Attribute | Values | Meaning |
|-----------|--------|---------|
| `.pad_usage` | `0` (toeprint) | Pad is a component pad |
| `.smd` | `1` (true) | SMD pad |
| `.string` | *(text index)* | Text string associated with feature |
| `.drill` | `0`, `2` | Drill type |
| `.geometry` | `0` (text index to geometry name) | Geometry reference |
| `.comp_mount_type` | `1` (SMD), `2` (TH) | Component mount type |

#### 3.5.3 Pad Record (`P`)

**Format**: `P x y sym_idx polarity dcode mirror angle [;attrs]`

**Fields**:
| Field | Type | Meaning |
|-------|------|---------|
| `x` | float | X position |
| `y` | float | Y position (ODB++ sign, negate for KiCad) |
| `sym_idx` | int | Index into `$` symbol table |
| `polarity` | `P`/`N` | Positive or negative |
| `dcode` | int | D-code (0 = standard) |
| `mirror` | int | Mirror flag (8 = normal) |
| `angle` | float | Rotation in degrees |
| `;attrs` | string | Attribute assignments: `;@idx=val,@idx2=val2,...` or `;@idx=val,attr_flag` |

**Real example** (`f.cu`):
```
P 120.91 -100.0 0 P 0 8 0.0 ;0=0,1
```
- Position: (120.91, -100.0)
- Symbol: `$0` = `rect1025.0x1400.0xr250.0` → rounded rect 1.025×1.4mm, r=0.25mm
- Rotation: 0°
- Attributes: `@0=0` (`.pad_usage`=toeprint), `1` (`.smd`=true)

#### 3.5.4 Line Record (`L`)

**Format**: `L x1 y1 x2 y2 sym_idx polarity dcode [;attrs]`

**Fields**:
| Field | Type | Meaning |
|-------|------|---------|
| `x1, y1` | float | Start point |
| `x2, y2` | float | End point |
| `sym_idx` | int | Symbol index (defines line width via round symbol) |
| `polarity` | `P`/`N` | Positive or negative |
| `dcode` | int | D-code |
| `;attrs` | string | Optional attributes |

**Real examples**:

Copper trace (`f.cu`):
```
L 100.91 -100.0 119.09 -100.0 1 P 0
```
- From (100.91, -100.0) to (119.09, -100.0)
- Symbol `$1` = `r250.0` → line width 0.25mm

Silkscreen line (`f.silkscreen`):
```
L 99.0 -100.62 99.0 -99.38 0 P 0
```
- Symbol `$0` = `r120.0` → line width 0.12mm

Text stroke (`f.silkscreen`):
```
L 99.83 -98.80 99.50 -98.33 1 P 0 ;0=0
```
- Symbol `$1` = `r150.0` → line width 0.15mm
- Attribute: `@0=0` (`.string`=`&0`="R1") — part of reference designator text

#### 3.5.5 Arc Record (`A`)

**Format**: `A xs ys xe ye xc yc sym_idx polarity dcode clockwise [;attrs]`

**Fields**:
| Field | Type | Meaning |
|-------|------|---------|
| `xs, ys` | float | Start point |
| `xe, ye` | float | End point |
| `xc, yc` | float | Center point |
| `sym_idx` | int | Symbol index (line width) |
| `polarity` | `P`/`N` | Positive or negative |
| `dcode` | int | D-code |
| `clockwise` | `Y`/`N` | Direction |

**Real example** (`samples/odb-kitchen-sink`, `b.fab/features`):
```
A 105.22 -54.28 108.0 -55.0 106.0 -57.0 2 P 0 Y
```
- Arc from (105.22, -54.28) to (108.0, -55.0), center (106.0, -57.0)
- Symbol `$2` → line width
- Clockwise = Y

#### 3.5.6 Surface Record (`S`)

**Format**:
```
S polarity dcode
OB x y direction
OS x y
...
OE
SE
```

Surfaces represent filled copper areas (zones, copper pours). Same contour grammar as profile (section 3.4).

**Real example** (`samples/odb-kitchen-sink`, `b.cu/features`):
```
S P 0
OB 232.71 -38.44 I
OS 232.71 -38.44
OS 232.78 -38.46
OS 232.82 -38.51
OS 232.83 -38.56
OS 232.83 -116.17
...
OE
```

**Key points**:
- `S P 0` starts a positive surface
- `OB ... I` = island (filled area), `OB ... H` = hole (void)
- Multiple `OB` blocks may appear within one surface
- Terminated by `SE`

### 3.6 `steps/<step>/layers/comp_+_<side>/components` — Component Placement

**Path**: `<odb-root>/steps/pcb/layers/comp_+_top/components` (and `comp_+_bot` for bottom)

**Grammar**:
```
file       = "UNITS=" unit NL attr_defs string_defs component*
component  = cmp_record property* terminal*
cmp_record = "CMP" pkg_idx x y rotation mirror refdes footprint [";attrs"] NL
property   = "PRP" key "'" value "'" NL
terminal   = "TOP" pin_idx x y rotation mirror net_idx subnet_idx pin_num NL
```

**Real example** (`samples/odb-output`, `comp_+_top/components`):
```
UNITS=MM

#
#Feature attribute names
#
@0 .comp_mount_type

#
#Feature attribute text strings
#
# CMP 0
CMP 0 100.0 -100.0 0 N R1 Resistor_SMD_R_0805_2012Metric ;0=1
PRP Datasheet ''
PRP Description ''
PRP Value '10k'
TOP 0 99.09 -100.0 0.0 N 3 0 1
TOP 1 100.91 -100.0 0.0 N 2 0 2
#
# CMP 1
CMP 0 120.0 -100.0 0 N R2 Resistor_SMD_R_0805_2012Metric ;0=1
PRP Datasheet ''
PRP Description ''
PRP Value '4.7k'
TOP 0 119.09 -100.0 0.0 N 2 1 1
TOP 1 120.91 -100.0 0.0 N 1 0 2
#
```

**CMP fields**:
| Field | Example | Meaning |
|-------|---------|---------|
| `pkg_idx` | `0` | Index into `eda/data` PKG table |
| `x` | `100.0` | Component center X |
| `y` | `-100.0` | Component center Y (negate for KiCad) |
| `rotation` | `0` | Rotation in degrees |
| `mirror` | `N` | `N`=normal, `M`=mirrored (bottom side) |
| `refdes` | `R1` | Reference designator |
| `footprint` | `Resistor_SMD_R_0805_2012Metric` | Footprint/package name |
| `;attrs` | `;0=1` | Attribute: `.comp_mount_type`=1 (SMD) |

**PRP fields**: Key-value properties. Value is single-quoted.

**TOP fields** (terminal/pin placement):
| Field | Example | Meaning |
|-------|---------|---------|
| `pin_idx` | `0` | Pin index within component |
| `x` | `99.09` | Pad center X (absolute) |
| `y` | `-100.0` | Pad center Y (absolute, negate for KiCad) |
| `rotation` | `0.0` | Pad rotation |
| `mirror` | `N` | Pad mirror |
| `net_idx` | `3` | Net number (index into netlist `$` table) |
| `subnet_idx` | `0` | Subnet index |
| `pin_num` | `1` | Pin number (as printed on component) |

**Kitchen-sink example** (through-hole component):
```
CMP 2 88.30 -68.90 90.0 N P2 _bornier2 ;0=2
PRP CustomField 'CustomValue'
PRP Value 'CONN_2'
TOP 0 88.30 -66.36 90.0 N 1 1 1
TOP 1 88.30 -71.44 90.0 N 0 3 2
```
- `.comp_mount_type`=2 → through-hole
- Rotation 90°
- Pins at absolute coordinates

### 3.7 `steps/<step>/eda/data` — EDA Package & Net Data

**Path**: `<odb-root>/steps/pcb/eda/data`

**Grammar**:
```
file     = header units layer_list attr_defs string_defs net_block* pkg_block*
header   = "HDR" app_info NL
units    = "UNITS=" unit NL
layer_list = "LYR" layer_name+ NL
net_block  = "NET" net_name NL subnet* feature_id*
subnet     = "SNT" type [layer pin_idx subnet_idx] NL
feature_id = "FID" layer_type layer_idx feature_idx NL
pkg_block  = "PKG" name xmax ymin xmin ymax rotation";" NL
               courtyard? pin*
courtyard  = "CT" NL contour "CE" NL
pin        = "PIN" id side x y rotation electrical shape NL courtyard
contour    = ("OB" x y dir NL ("OS" x y NL)* "OE" NL)+
```

**Real example** (`samples/odb-output`, `eda/data`):
```
HDR KiCad EDA 9.99.0-unknown
UNITS=MM
LYR f.cu f.mask f.paste

NET $NONE$
#NET 1
NET GND
SNT TOP T 1 1
FID C 0 0
FID C 1 0
FID C 2 0
#NET 2
NET SIG
SNT TOP T 0 1
FID C 0 2
FID C 1 1
FID C 2 1
SNT TOP T 1 0
FID C 0 3
FID C 1 2
FID C 2 2
SNT TRC
FID C 0 1
#NET 3
NET VCC
SNT TOP T 0 0
FID C 0 4
FID C 1 3
FID C 2 3
```

**NET record**: `NET <name>` — net names. `$NONE$` = unconnected net (maps to KiCad net 0 "").

**SNT record**: Subnet definition.
- `SNT TOP T <comp_idx> <pin_idx>` — pad on top side, component and pin index
- `SNT TRC` — trace (copper connection)
- `SNT VIA` — via connection
- `SNT PLN S E <idx>` — plane (copper pour)

**FID record**: `FID <layer_type> <layer_idx> <feature_idx>` — cross-reference to feature on a layer.
- `C` = copper layer, `H` = drill layer
- `layer_idx` = index into the `LYR` list (0-based)
- `feature_idx` = feature index within that layer's features file

**PKG record**: `PKG <name> <xmax> <ymin> <xmin> <ymax> <rotation>;`

Package definitions follow with courtyard outlines (`CT`/`OB`/`OS`/`OE`/`CE`) and pin definitions.

**PIN record**: `PIN <id> <side> <x> <y> <rotation> <electrical> <shape>`
- `side` = `S` (SMD)
- `x, y` = pin offset relative to package center
- `electrical` = `E` (electrical)
- `shape` = `S` (standard)

**Real example** (PKG from `eda/data`):
```
PKG R_0805_2012Metric 1.82 -1.43 -2.50 1.43 0.70;
CT
OB -1.43 -0.47 I
OS -1.43 -0.47
...
OE
CE
PIN 1 S -0.91 0.0 0 E S
CT
...
CE
PIN 2 S 0.91 0.0 0 E S
CT
...
CE
```

### 3.8 `steps/<step>/netlists/cadnet/netlist` — Netlist

**Path**: `<odb-root>/steps/pcb/netlists/cadnet/netlist`

**Grammar**:
```
file     = header net_def* comment_section point*
header   = "H" params NL
net_def  = "$" net_idx net_name NL
point    = net_idx via_flag x y layer pad_w pad_h flags NL
```

**Real example** (`samples/odb-output`):
```
H optimize n staggered n
$1 GND
$2 VCC
$3 SIG
#
#Netlist points
#
1 0 120.91 -100.0 T 1.02 1.40 e s
2 0 99.09 -100.0 T 1.02 1.40 e s
3 0 100.91 -100.0 T 1.02 1.40 e s
3 0 119.09 -100.0 T 1.02 1.40 e s
```

**Net definition**: `$<idx> <name>` — net index and name. Index 0 is implicit unconnected net.

**Net points**: Each line is a pad belonging to a net.
| Field | Example | Meaning |
|-------|---------|---------|
| `net_idx` | `1` | Net number |
| `via_flag` | `0` | Via count or via diameter |
| `x` | `120.91` | Pad X position |
| `y` | `-100.0` | Pad Y position |
| `layer` | `T` | Layer code: `T`=top, `B`=bottom |
| `pad_w` | `1.02` | Pad width |
| `pad_h` | `1.40` | Pad height |
| `flags` | `e s` | `e`=electrical, `s`=signal |

**Kitchen-sink example** (via netlist points):
```
0 0.30 120.0 -102.0 B e c staggered 0 0 0 v
```
- `v` flag at end indicates this is a via point
- `0.30` = via drill diameter
- `B` = both layers (through-hole via)

### 3.9 Symbol Name Decoding

Symbol names encode pad geometry. The parser must decode these names to extract dimensions.

**Decoding rules** (dimensions in thousandths of the unit, e.g., `1025.0` → 1.025mm):

| Pattern | Regex | Fields |
|---------|-------|--------|
| Round | `r(\d+\.?\d*)` | diameter |
| Rectangle | `rect(\d+\.?\d*)x(\d+\.?\d*)` | width, height |
| Rounded rect | `rect(\d+\.?\d*)x(\d+\.?\d*)xr(\d+\.?\d*)` | width, height, corner_radius |
| Oval | `oval(\d+\.?\d*)x(\d+\.?\d*)` | width, height |
| Square | `s(\d+\.?\d*)` | side |
| Donut round-cornered | `donut_rc(\d+\.?\d*)x(\d+\.?\d*)x(\d+\.?\d*)xr(\d+\.?\d*)` | outer_w, outer_h, line_w, corner_r |

**Converting dimensions to mm**: Divide by 1000.
- `r250.0` → diameter = 250.0/1000 = 0.25mm
- `rect1025.0x1400.0xr250.0` → 1.025mm × 1.400mm, corner radius 0.250mm

**Rounded rect ratio** (for KiCad `roundrect_rratio`):
- `rratio = corner_radius / min(width, height)`
- Example: `rect1025.0x1400.0xr250.0` → 250.0 / 1025.0 = 0.243902

### 3.10 `steps/<step>/layers/<name>/attrlist` — Layer Attributes

**Path**: `<odb-root>/steps/pcb/layers/<layer-name>/attrlist`

**Format**: `KEY=VALUE` pairs (layer properties like dielectric, copper weight).

**Examples**:
```
# f.cu/attrlist
.layer_dielectric=0.03
.copper_weight=1.0

# dielectric_1/attrlist
.layer_dielectric=1.51
.dielectric_constant=4.50
.loss_tangent=0.02
.material=fr4
```

These are informational for the converter; board thickness is computed from dielectric stack.

---

## 4. KiCad Output Format Specification

### 4.1 `.kicad_pcb` S-Expression Structure

The output must follow KiCad 8 format (version `20240108`). Structure:

```
(kicad_pcb (version 20240108) (generator "odb2kicad") (generator_version "1.0")
  (general
    (thickness <board_thickness>)
    (legacy_teardrops no)
  )
  (paper "A4")
  (layers
    <layer_definitions>
  )
  (setup
    (pad_to_mask_clearance 0)
    (allow_soldermask_bridges_in_footprints no)
    (pcbplotparams
      (layerselection 0x00010fc_ffffffff)
      (plot_on_all_layers_selection 0x0000000_00000000)
    )
  )
  <net_definitions>
  <footprints>
  <segments>
  <graphics>
)
```

### 4.2 Layer Definitions

KiCad layers have numeric IDs and canonical names:

| ID | Canonical | Alias | ODB++ NAME |
|----|-----------|-------|------------|
| 0 | `F.Cu` | — | `F.CU` |
| 31 | `B.Cu` | — | `B.CU` |
| 32 | `B.Adhes` | `B.Adhesive` | `B.ADHESIVE` |
| 33 | `F.Adhes` | `F.Adhesive` | `F.ADHESIVE` |
| 34 | `B.Paste` | — | `B.PASTE` |
| 35 | `F.Paste` | — | `F.PASTE` |
| 36 | `B.SilkS` | `B.Silkscreen` | `B.SILKSCREEN` |
| 37 | `F.SilkS` | `F.Silkscreen` | `F.SILKSCREEN` |
| 38 | `B.Mask` | — | `B.MASK` |
| 39 | `F.Mask` | — | `F.MASK` |
| 40 | `Dwgs.User` | `User.Drawings` | `USER.DRAWINGS` |
| 41 | `Cmts.User` | `User.Comments` | `USER.COMMENTS` |
| 42 | `Eco1.User` | `User.Eco1` | `USER.ECO1` |
| 43 | `Eco2.User` | `User.Eco2` | `USER.ECO2` |
| 44 | `Edge.Cuts` | — | `EDGE.CUTS` |
| 45 | `Margin` | — | `MARGIN` |
| 46 | `B.CrtYd` | `B.Courtyard` | `B.COURTYARD` |
| 47 | `F.CrtYd` | `F.Courtyard` | `F.COURTYARD` |
| 48 | `B.Fab` | — | `B.FAB` |
| 49 | `F.Fab` | — | `F.FAB` |

**Layer type** in s-expression: `signal` for Cu layers, `user` for all others.

**S-expression format**:
```
(layers
  (0 "F.Cu" signal)
  (31 "B.Cu" signal)
  (32 "B.Adhes" user "B.Adhesive")
  (33 "F.Adhes" user "F.Adhesive")
  (34 "B.Paste" user)
  ...
  (44 "Edge.Cuts" user)
)
```

Layers with aliases (like `B.Adhes` / `B.Adhesive`) include the alias as a 4th element. Only emit layers that appear in the ODB++ matrix.

### 4.3 Net Definitions

```
(net 0 "")
(net 1 "GND")
(net 2 "VCC")
(net 3 "SIG")
```

Net 0 is always the unconnected net with empty name. Remaining nets are numbered sequentially. The ordering may differ from ODB++ — match by **name**.

### 4.4 Footprints

```
(footprint "<library>:<package>" (layer "<layer>")
  (tstamp <uuid>)
  (at <x> <y> [<angle>])
  (property "Reference" "<refdes>")
  (property "Value" "<value>")
  (attr smd|through_hole)
  (fp_text reference "<refdes>" (at <rx> <ry>) (layer "F.SilkS")
    (effects (font (size 1 1) (thickness 0.15)))
  )
  (fp_text value "<value>" (at <vx> <vy>) (layer "F.Fab")
    (effects (font (size 1 1) (thickness 0.15)))
  )
  <fp_lines>
  <pads>
)
```

**Field mapping**:
| KiCad field | Source |
|-------------|--------|
| `library:package` | Components `footprint` field. If no `:`, use `imported:<footprint>` |
| `layer` | `F.Cu` for top, `B.Cu` for bottom (based on component layer) |
| `tstamp` | Generate deterministic UUID from component index |
| `at x y angle` | From CMP record (Y negated, angle negated) |
| `property Reference` | CMP `refdes` |
| `property Value` | PRP `Value` |
| `attr` | `smd` if `.comp_mount_type`=1, `through_hole` if =2 |

**Pad s-expression**:
```
(pad "<pin_num>" smd|thru_hole roundrect|circle|rect|oval
  (at <x> <y>)
  (size <w> <h>)
  (layers <layer_list>)
  (roundrect_rratio <ratio>)
  (net <net_id> "<net_name>")
)
```

Pad position is **relative to footprint origin**. Computed as:
```
pad_local_x = (absolute_pad_x - component_x) * cos(comp_angle) + (absolute_pad_y - component_y) * sin(comp_angle)
pad_local_y = -(absolute_pad_x - component_x) * sin(comp_angle) + (absolute_pad_y - component_y) * cos(comp_angle)
```
(Because ODB++ stores absolute coordinates while KiCad uses component-relative.)

**Pad layers**:
- SMD pad: `"F.Cu" "F.Paste" "F.Mask"` (or `B.*` for bottom)
- Through-hole pad: `"*.Cu" "*.Mask"`

**Pad type mapping from symbol name**:
| Symbol pattern | KiCad pad shape |
|----------------|-----------------|
| `r<d>` | `circle` |
| `rect<w>x<h>` | `rect` |
| `rect<w>x<h>xr<r>` | `roundrect` |
| `oval<w>x<h>` | `oval` |

### 4.5 Segments (Copper Traces)

```
(segment (start <x1> <y1>) (end <x2> <y2>) (width <w>) (layer "<layer>") (net <net_id>))
```

Derived from `L` (line) records on copper layers (`F.CU`, `B.CU`).

- Coordinates: Y-negated from ODB++
- Width: From the symbol referenced by the line's `sym_idx`, decoded as `r<d>` → d/1000
- Layer: Mapped from ODB++ layer name to KiCad canonical name
- Net: Determined by matching pad positions — a trace's endpoints should touch pads, inheriting their net. If no pad match, use net 0.

**Real example mapping**:

ODB++ (`f.cu/features`):
```
L 100.91 -100.0 119.09 -100.0 1 P 0
```
Symbol `$1` = `r250.0` → width 0.25mm.

KiCad output:
```
(segment (start 100.9125 100) (end 119.0875 100) (width 0.25) (layer "F.Cu") (net 3))
```
Note: Y negated (-100.0 → 100), and actual KiCad values use the pad positions (100.9125 vs ODB's 100.91 due to rounding).

### 4.6 Graphic Items (Board Outline, Silkscreen)

**Board outline** (from profile or edge.cuts layer):
```
(gr_rect (start <x1> <y1>) (end <x2> <y2>)
  (stroke (width 0.05) (type default)) (fill none) (layer "Edge.Cuts"))
```

For rectangular profiles, emit `gr_rect`. For polygonal profiles, emit `gr_poly` or individual `gr_line` segments.

**General graphic line**:
```
(gr_line (start <x1> <y1>) (end <x2> <y2>)
  (stroke (width <w>) (type solid)) (layer "<layer>"))
```

**General graphic arc**:
```
(gr_arc (start <xs> <ys>) (mid <xm> <ym>) (end <xe> <ye>)
  (stroke (width <w>) (type solid)) (layer "<layer>"))
```

KiCad uses midpoint representation for arcs (not center). The midpoint must be computed from ODB++'s center+start+end+direction format.

### 4.7 Vias

```
(via (at <x> <y>) (size <via_size>) (drill <drill_size>)
  (layers "F.Cu" "B.Cu") (net <net_id>))
```

Vias are identified from:
1. The drill layer features (`drill_plated_f.cu-b.cu/features`) — gives drill diameter
2. Copper layer features with `.geometry` attribute referencing a via geometry name
3. `eda/data` `SNT VIA` records — gives net assignment

**Via identification heuristic**: A pad that appears on the drill layer AND on both copper layers at the same (x,y) position is a via.

### 4.8 Filled Zones (Copper Pours)

```
(zone (net <id>) (net_name "<name>") (layer "<layer>")
  (fill (thermal_gap 0.5) (thermal_bridge_width 0.5))
  (polygon
    (pts
      (xy <x1> <y1>) (xy <x2> <y2>) ...
    )
  )
  (filled_polygon
    (layer "<layer>")
    (pts
      (xy <x1> <y1>) (xy <x2> <y2>) ...
    )
  )
)
```

Derived from `S` (surface) records on copper layers. The outline points from `OB`/`OS`/`OE` become the polygon vertices with Y-negated.

---

## 5. Coordinate & Data Mapping

### 5.1 Coordinate Transform: ODB++ → KiCad

| Axis | Transform | Reason |
|------|-----------|--------|
| X | Identity | Same direction |
| Y | **Negate** | ODB++ Y-down, KiCad Y-down in file but semantically inverted |

Specifically: KiCad's coordinate system has Y increasing downward in the file, and ODB++ stores Y as negative values for points below the origin. So `y_kicad = -y_odb`.

**Verification from test data**:
- ODB++ profile: `OB 130.0 -90.0` → KiCad: `(start 130 90)` ✓
- ODB++ component: `CMP 0 100.0 -100.0` → KiCad: `(at 100 100)` ✓
- ODB++ pad: `P 120.91 -100.0` → KiCad pad at y=100 ✓

### 5.2 Rotation

| Context | Transform |
|---------|-----------|
| Component rotation | Use as-is (same convention in both formats for top-side) |
| Component rotation (bottom) | Negate (mirror reverses rotation sense) |
| Pad rotation (relative) | Subtract component rotation |

### 5.3 Layer Name Mapping

ODB++ stores layer names in **UPPERCASE**. KiCad uses **mixed-case canonical names**.

```cpp
std::string mapLayerName(const std::string& odbName) {
    // Convert to canonical KiCad name
    // ODB++: "F.CU" → KiCad: "F.Cu"
    // ODB++: "F.SILKSCREEN" → KiCad: "F.SilkS"
    // etc.
}
```

Full mapping table in Section 4.2.

### 5.4 Net Index Remapping

ODB++ and KiCad may number nets differently. Match by **name**:

| ODB++ `eda/data` | ODB++ `netlist` | KiCad |
|-------------------|-----------------|-------|
| NET $NONE$ (idx 0) | — | `(net 0 "")` |
| NET GND (idx 1) | $1 GND | `(net 1 "GND")` |
| NET SIG (idx 2) | $3 SIG | `(net 3 "SIG")` |
| NET VCC (idx 3) | $2 VCC | `(net 2 "VCC")` |

Note the different ordering! The `eda/data` NET ordering is GND=1, SIG=2, VCC=3, but `netlist` has GND=1, VCC=2, SIG=3, and KiCad has GND=1, VCC=2, SIG=3. The converter assigns KiCad net indices sequentially based on alphabetical order or encounter order, then maps by name.

**Implementation**: Build a map from ODB++ net name → KiCad net index. Use the `eda/data` NET records as the authoritative source (they match components). In the `TOP` record, the `net_idx` references the `eda/data` NET ordering.

### 5.5 Symbol Dimension Scaling

Symbol dimensions are in **thousandths of the unit** (when UNITS=MM, values are in µm):
```
actual_mm = symbol_value / 1000.0
```

Examples:
- `r250.0` → 0.250mm diameter
- `rect1025.0x1400.0` → 1.025mm × 1.400mm

### 5.6 Pad Position: Absolute → Relative

ODB++ stores pad positions in absolute board coordinates. KiCad stores them relative to the footprint origin.

```
// ODB++ absolute pad position: (pad_abs_x, pad_abs_y)
// ODB++ component position: (comp_x, comp_y) with rotation comp_angle (degrees)

// Step 1: translate to component-relative
dx = pad_abs_x - comp_x
dy = pad_abs_y - comp_y

// Step 2: unrotate by component angle
angle_rad = comp_angle * PI / 180.0
pad_rel_x =  dx * cos(angle_rad) + dy * sin(angle_rad)
pad_rel_y = -dx * sin(angle_rad) + dy * cos(angle_rad)

// Step 3: Y-negate happens at output time for the whole footprint,
// so pad_rel stays in ODB++ coordinate space internally
```

**Verification**:
- Component R1 at (100.0, -100.0), rotation 0°
- Pad 1 (pin "1") at absolute (99.09, -100.0)
- Relative: (99.09 - 100.0, -100.0 - (-100.0)) = (-0.91, 0.0)
- KiCad output: `(pad "1" ... (at -0.9125 0))` ✓ (slight precision difference)

---

## 6. Implementation Stages

### Stage A: Skeleton + Board Outline

**Goal**: Parse enough to emit a valid `.kicad_pcb` with board outline and layer stack.

**Tasks**:
1. CMake build system (`CMakeLists.txt`)
2. CLI `main.cpp`: parse args, call pipeline
3. Parse `misc/info` → units
4. Parse `matrix/matrix` → layer stack
5. Parse `steps/pcb/profile` → board outline contour
6. Parse `steps/pcb/stephdr` → coordinate origin
7. Writer: emit `(kicad_pcb ...)` with layers, setup, outline

**Acceptance criteria**:
- `odb2kicad samples/odb-output out.kicad_pcb` produces a valid file
- KiCad can open the file without errors
- Board outline matches: rectangle from (90,90) to (130,110) in KiCad coordinates
- Layer stack includes F.Cu, B.Cu, silkscreen, mask, paste, Edge.Cuts

### Stage B: Nets + Components + Pads

**Goal**: Import components with correct placement, pads with net assignments.

**Tasks**:
1. Parse `eda/data` → package definitions, net names, pin-to-net mapping
2. Parse `comp_+_top/components` → component placement
3. Parse `netlists/cadnet/netlist` → net definitions (backup)
4. Symbol name decoder → pad shapes
5. Writer: emit `(net ...)`, `(footprint ...)` with `(pad ...)` records
6. Coordinate transforms: absolute→relative pad positions, Y-negation

**Acceptance criteria for simple test** (`samples/odb-output`):
- 3 nets emitted: GND, VCC, SIG (plus net 0)
- 2 footprints: R1 at (100,100), R2 at (120,100) — both `Resistor_SMD:R_0805_2012Metric`
- R1 properties: Reference="R1", Value="10k"
- R2 properties: Reference="R2", Value="4.7k"
- Each footprint has 2 pads, `roundrect` shape, size 1.025×1.4mm
- R1 pad 1 net=VCC, pad 2 net=SIG
- R2 pad 1 net=SIG, pad 2 net=GND
- `attr smd` on both footprints

### Stage C: Copper Traces + Vias

**Goal**: Import copper traces and vias.

**Tasks**:
1. Parse `features` on copper layers → `L` records as segments
2. Parse drill layer features → via positions and drill sizes
3. Cross-reference: match via positions on copper layers with drill layer
4. Net assignment for traces (endpoint matching to pads)
5. Writer: emit `(segment ...)` and `(via ...)`

**Acceptance criteria for simple test**:
- 1 segment: from (100.91, 100) to (119.09, 100), width 0.25mm, layer F.Cu, net SIG

**Acceptance criteria for kitchen-sink test** (`samples/odb-kitchen-sink`):
- Vias present with correct drill sizes
- Traces on both F.Cu and B.Cu
- Net assignment correct for all traces touching pads

### Stage D: Zones + Graphics + Polish

**Goal**: Import copper fills, silkscreen graphics, arcs; handle edge cases.

**Tasks**:
1. Surface records on copper layers → `(zone ...)` with filled polygons
2. Silkscreen lines → `(fp_line ...)` within footprints or `(gr_line ...)` at board level
3. Arc records → `(gr_arc ...)` with midpoint computation
4. Board outline: support polygonal profiles with holes
5. Handle bottom-side components (`comp_+_bot`)
6. Through-hole pad layer assignment (`"*.Cu" "*.Mask"`)

**Acceptance criteria**:
- Kitchen-sink board copper fills imported as zones
- Silkscreen lines visible on correct layers
- Arcs rendered correctly (midpoint calculation verified)
- Board with cutouts has correct outline

---

## 7. Test Specifications

### 7.1 Simple Test Case

**Input**: `samples/odb-output/` (pre-extracted ODB++ from KiCad test.kicad_pcb)

**Reference**: `samples/test.kicad_pcb`

**Command**:
```bash
./odb2kicad ../samples/odb-output ../samples/test-roundtrip.kicad_pcb
```

**Structural equivalence checks** (the output doesn't need to be byte-identical, but must match structurally):

| Property | Expected Value |
|----------|----------------|
| Format version | `20240108` |
| Board thickness | `1.6` |
| Number of nets | 4 (including net 0) |
| Net names | `""`, `"GND"`, `"SIG"`, `"VCC"` |
| Number of footprints | 2 |
| Footprint 1 refdes | `R1` |
| Footprint 1 position | `(at 100 100)` |
| Footprint 1 value | `10k` |
| Footprint 1 pad 1 net | `VCC` |
| Footprint 1 pad 1 shape | `roundrect` |
| Footprint 1 pad 1 size | `1.025 1.4` |
| Footprint 1 pad 1 rratio | `~0.2439` |
| Footprint 2 refdes | `R2` |
| Footprint 2 position | `(at 120 100)` |
| Footprint 2 value | `4.7k` |
| Footprint 2 pad 2 net | `GND` |
| Number of segments | 1 |
| Segment start | `(100.91 100)` (approx) |
| Segment end | `(119.09 100)` (approx) |
| Segment width | `0.25` |
| Segment layer | `F.Cu` |
| Segment net | `SIG` (net 3) |
| Board outline | Rectangle (90,90)→(130,110) |

### 7.2 Kitchen-Sink Test Case

**Input**: `samples/odb-kitchen-sink/` (complex board with vias, TH components, copper fills, arcs)

**Structural checks**:

| Property | Expected |
|----------|----------|
| Number of nets | ≥2 (net 0 + "A") |
| Components | R1 (SMD), D1 (TH), P2 (TH, 90° rotation) |
| R1 position | `(125.20 90.90)` |
| D1 position | `(105.85 65.35)` |
| P2 position | `(88.30 68.90)` with angle 90 |
| Vias | 3 vias at (120,-102), (118,-102), (120,-103.5) → KiCad (120,102), (118,102), (120,103.5) |
| Via drill | 0.3mm (from netlist) |
| Copper fills | At least 1 zone on B.Cu |
| Traces | Multiple segments on F.Cu and B.Cu |
| Board outline | Polygon with 3 holes |

### 7.3 Automated Smoke Test

`test/test_simple.sh`:
```bash
#!/bin/bash
set -e
BUILD_DIR="$(dirname "$0")/../build"
SAMPLES="$(dirname "$0")/../../samples"

"${BUILD_DIR}/odb2kicad" "${SAMPLES}/odb-output" /tmp/test-out.kicad_pcb

# Check file is valid s-expression (basic bracket matching)
OPEN=$(grep -o '(' /tmp/test-out.kicad_pcb | wc -l)
CLOSE=$(grep -o ')' /tmp/test-out.kicad_pcb | wc -l)
[ "$OPEN" -eq "$CLOSE" ] || { echo "FAIL: mismatched brackets"; exit 1; }

# Check key content
grep -q 'kicad_pcb' /tmp/test-out.kicad_pcb || { echo "FAIL: no kicad_pcb header"; exit 1; }
grep -q '"GND"' /tmp/test-out.kicad_pcb || { echo "FAIL: no GND net"; exit 1; }
grep -q '"VCC"' /tmp/test-out.kicad_pcb || { echo "FAIL: no VCC net"; exit 1; }
grep -q '"SIG"' /tmp/test-out.kicad_pcb || { echo "FAIL: no SIG net"; exit 1; }
grep -q '"R1"' /tmp/test-out.kicad_pcb || { echo "FAIL: no R1 component"; exit 1; }
grep -q '"R2"' /tmp/test-out.kicad_pcb || { echo "FAIL: no R2 component"; exit 1; }
grep -q 'segment' /tmp/test-out.kicad_pcb || { echo "FAIL: no segments"; exit 1; }
grep -q 'Edge.Cuts' /tmp/test-out.kicad_pcb || { echo "FAIL: no Edge.Cuts"; exit 1; }

echo "PASS: all smoke tests passed"
```

---

## 8. Known Limitations & Design Decisions

### 8.1 Limitations
1. **Text is rasterized**: ODB++ exports text as individual line strokes. The converter imports these as line segments — the text is visually correct but not editable as text objects in KiCad.
2. **Custom pad shapes become polygons**: Non-standard pad shapes (complex courtyard outlines in `eda/data`) are approximated. Standard shapes (round, rect, roundrect, oval) are mapped directly.
3. **Zone outlines are lost**: ODB++ only exports copper fill polygons, not the original zone outlines with rules. Zones are imported as filled polygons without thermal relief or clearance rules.
4. **No archive extraction**: Input must be a pre-extracted directory. Use `tar xzf` or `unzip` before running.
5. **Two copper layers only** (initial): Only F.Cu and B.Cu are handled. Inner copper layers are parsed but not mapped to KiCad inner layers in the first implementation.
6. **No netlist-driven DRC**: The converter does not verify connectivity. It trusts the ODB++ data.
7. **Precision**: ODB++ coordinates may have rounding differences vs the original KiCad design (e.g., 100.91 vs 100.9125). The converter uses the ODB++ values as-is.

### 8.2 Design Decisions
1. **Net indexing**: Use the `eda/data` NET records as the primary net source (they cross-reference to components). The `netlists/cadnet/netlist` is used as a secondary source for net names and as a backup.
2. **Coordinate system**: All internal processing uses ODB++ coordinates. Y-negation happens only at output time in the writer.
3. **UUID generation**: Footprint `tstamp` UUIDs are generated deterministically from the component index to ensure reproducible output.
4. **Footprint library prefix**: If the ODB++ footprint name doesn't contain `:`, prefix with `imported:` (e.g., `imported:R_0805_2012Metric`).
5. **Layer filtering**: Only emit layers that have actual features. Don't emit empty layers in the output.
6. **Board thickness**: Default to 1.6mm. Can be computed from dielectric layer attributes if available.
