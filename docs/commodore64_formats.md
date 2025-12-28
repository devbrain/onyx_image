# Commodore 64 Image Formats

This document describes the Commodore 64 image formats supported by onyx_image.

## Table of Contents

1. [C64 Graphics Overview](#c64-graphics-overview)
2. [Color Palette](#color-palette)
3. [Memory Layout](#memory-layout)
4. [Bitmap Organization](#bitmap-organization)
5. [Koala Format](#koala-format)
6. [Doodle Format](#doodle-format)
7. [DrazLace Format](#drazlace-format)
8. [InterPaint Format](#interpaint-format)
9. [Amica Paint Format](#amica-paint-format)
10. [FunPaint II Format](#funpaint-ii-format)
11. [C64 HiRes Format](#c64-hires-format)
12. [Run Paint Format](#run-paint-format)

---

## C64 Graphics Overview

The Commodore 64 uses the VIC-II (Video Interface Chip II) for graphics. It supports two main bitmap modes:

### Display Modes

| Mode | Resolution | Colors per Cell | Cell Size | Effective Colors |
|------|------------|-----------------|-----------|------------------|
| Hi-Res | 320×200 | 2 | 8×8 | 2 per 8×8 cell |
| Multicolor | 160×200 | 4 | 4×8 | 4 per 4×8 cell (doubled pixels) |

In multicolor mode, horizontal resolution is halved (160 pixels), but each pixel is displayed as 2 physical pixels wide, resulting in a 320×200 display with "fat" pixels.

### Byte Order

The C64 uses a **little-endian** byte order (MOS 6502 CPU). Load addresses are stored as low byte first.

---

## Color Palette

The C64 has a fixed 16-color palette:

| Index | Color | RGB Value |
|-------|-------|-----------|
| 0 | Black | `#000000` |
| 1 | White | `#FFFFFF` |
| 2 | Red | `#68372B` |
| 3 | Cyan | `#70A4B2` |
| 4 | Purple | `#6F3D86` |
| 5 | Green | `#588D43` |
| 6 | Blue | `#352879` |
| 7 | Yellow | `#B8C76F` |
| 8 | Orange | `#6F4F25` |
| 9 | Brown | `#433900` |
| 10 | Light Red | `#9A6759` |
| 11 | Dark Gray | `#444444` |
| 12 | Gray | `#6C6C6C` |
| 13 | Light Green | `#9AD284` |
| 14 | Light Blue | `#6C5EB5` |
| 15 | Light Gray | `#959595` |

*Note: These are the VICE emulator default colors. Various palettes exist (Pepto, Colodore, etc.) with slightly different RGB values.*

---

## Memory Layout

C64 bitmap graphics use three memory regions:

| Region | Size | Description |
|--------|------|-------------|
| Bitmap | 8,000 bytes | Pixel data (320×200 bits or 160×200 2-bit pairs) |
| Screen RAM | 1,000 bytes | Color data (40×25 characters) |
| Color RAM | 1,000 bytes | Additional color data (multicolor mode) |

### Screen RAM and Color RAM

The screen is divided into 40×25 character cells (8×8 pixels each). Each cell has:
- **Screen RAM byte**: Contains two 4-bit color indices (nibbles)
- **Color RAM byte**: Contains one 4-bit color index (lower nibble only)

---

## Bitmap Organization

C64 bitmap data is organized by character cells, not scanlines:

```
For each character row (0-24):
  For each character column (0-39):
    8 consecutive bytes (one per scanline within the cell)
```

### Extracting a Pixel

To extract a pixel at (x, y):

```cpp
// Character cell position
int char_col = x / 8;
int char_row = y / 8;
int row_in_char = y % 8;

// Bitmap byte offset
size_t offset = (char_row * 40 + char_col) * 8 + row_in_char;

// For hi-res: single bit (MSB = leftmost)
int bit_pos = 7 - (x % 8);
int pixel = (bitmap[offset] >> bit_pos) & 1;

// For multicolor: 2-bit pair
int pair = (x % 8) / 2;
int shift = 6 - (pair * 2);
int color_sel = (bitmap[offset] >> shift) & 0x03;
```

---

## Koala Format

**Extensions:** `.koa`, `.kla`, `.gg`
**Mode:** Multicolor (160×200, displayed as 320×200)

Koala Painter was one of the most popular C64 paint programs. The format is simple and widely supported.

### Uncompressed Format

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Load address (little-endian, typically `$6000`) |
| 2 | 8,000 | Bitmap data |
| 8,002 | 1,000 | Screen RAM |
| 9,002 | 1,000 | Color RAM |
| 10,002 | 1 | Background color |

**File Sizes:**
- 10,003 bytes (with load address)
- 10,001 bytes (without load address)
- 10,006 bytes (Run Paint variant)
- 10,018 bytes (OCP Art Studio variant)

### Multicolor Color Selection

For each 4×8 pixel cell, the 2-bit selector determines the color:

| Selector | Source |
|----------|--------|
| 00 | Background color (global) |
| 01 | Screen RAM upper nibble |
| 10 | Screen RAM lower nibble |
| 11 | Color RAM lower nibble |

### GG Compressed Format (.gg)

GodotGames uses RLE compression with escape byte `0xFE`:

```
For each byte:
  If byte == 0xFE:
    Read value byte
    Read count byte
    Repeat value (count) times
  Else:
    Output literal byte
```

Decompresses to 10,001 bytes (Koala without load address).

### Identification

1. File size matches known Koala sizes, OR
2. Small file with valid load address (0x6000, 0x4000, 0x2000, 0x5C00) and contains 0xFE escape bytes

---

## Doodle Format

**Extensions:** `.dd`, `.jj`
**Mode:** Hi-Res (320×200)

Doodle is a hi-res format with per-character colors.

### Uncompressed Format

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Load address |
| 2 | 1,000 | Video matrix (screen RAM) |
| 1,002 | 24 | Padding (Run Paint variant) or additional data |
| 1,026 | 8,000 | Bitmap data |

**File Sizes:**
- 9,218 bytes (standard)
- 9,217 bytes (Hires Editor variant)
- 9,026 bytes (Run Paint variant)
- 9,346 bytes (extended variant)

### Hi-Res Color Selection

For each 8×8 pixel cell, the video matrix byte determines colors:
- **Bit 0 (background):** Lower nibble (bits 3-0)
- **Bit 1 (foreground):** Upper nibble (bits 7-4)

### JJ Compressed Format

JJ files use the same RLE scheme as GG:
- Escape byte: `0xFE`
- Decompresses to 9,024 bytes

### Identification

1. File size matches known Doodle sizes, OR
2. Compressed file that decompresses to exactly 9,024 or 9,216 bytes

---

## DrazLace Format

**Extensions:** `.drl`
**Mode:** Interlaced Multicolor (320×200 with increased color depth)

DrazLace creates higher color depth by interlacing two multicolor frames with a 1-pixel horizontal shift.

### File Structure

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Load address |
| 2 | 1,000 | Color RAM |
| 1,026 | 1,000 | Video matrix (Screen RAM) |
| 2,050 | 8,000 | Bitmap 1 |
| 10,050 | 2 | Background color + reserved |
| 10,052 | 2 | Shift value (0 or 1) |
| 10,242 | 8,000 | Bitmap 2 |

**Uncompressed Size:** 18,242 bytes

### Interlacing

DrazLace stores two complete multicolor bitmaps. During display:
1. Frame 1 is displayed at x offset 0
2. Frame 2 is displayed at x offset -shift (typically -1)
3. The eye blends both frames, creating the illusion of more colors

### Compression

Compressed files have the signature `DRAZLACE! 1.0` at offset 2 and use DRP RLE:
- Escape byte at offset 15
- Compressed data starts at offset 16
- Format: escape, count, value

### Identification

1. File size is exactly 18,242 bytes, OR
2. File contains signature `DRAZLACE! 1.0` at offset 2

---

## InterPaint Format

**Extensions:** `.iph` (hi-res), `.ipt` (multicolor)

InterPaint supports both hi-res and multicolor modes.

### IPH Format (Hi-Res)

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Load address |
| 2 | 8,000 | Bitmap data |
| 8,002 | 1,000 | Video matrix |

**File Sizes:** 9,002, 9,003, or 9,009 bytes

### IPT Format (Multicolor)

Same layout as Koala format:

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Load address |
| 2 | 8,000 | Bitmap data |
| 8,002 | 1,000 | Screen RAM |
| 9,002 | 1,000 | Color RAM |
| 10,002 | 1 | Background color |

**File Size:** 10,003 bytes

### Identification

File size is 9,002, 9,003, 9,009 (IPH) or 10,003 (IPT) bytes.

---

## Amica Paint Format

**Extensions:** `.ami`, `.afli`
**Mode:** Multicolor (160×200)

Amica Paint (AFLI - Advanced FLI) uses RLE-compressed Koala-compatible format.

### Compression

Uses DRP RLE with fixed escape byte `0xC2`:
- Format: escape (0xC2), count, value
- Decompresses to 10,001 bytes (Koala layout)

### File Structure (Decompressed)

Same as Koala format without load address:

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 8,000 | Bitmap data |
| 8,000 | 1,000 | Screen RAM |
| 9,000 | 1,000 | Color RAM |
| 10,000 | 1 | Background color |

### Identification

1. File size less than 10,001 bytes (compressed)
2. Load address is 0x4000
3. Contains escape byte 0xC2 in data

---

## FunPaint II Format

**Extensions:** `.fun`, `.fp2`, `.fph`
**Mode:** IFLI (Interlaced FLI, 296×200)

FunPaint II is an advanced format using FLI (Flexible Line Interpretation) with interlacing for maximum color depth.

### FLI Mode

FLI mode changes the video matrix every scanline, allowing different colors on every 8-pixel horizontal strip instead of every 8×8 cell. This is achieved by CPU intervention during screen refresh.

**FLI Bug:** The leftmost 3 characters (24 pixels) show garbage, so usable width is 296 pixels.

### File Structure

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Load address |
| 2 | 14 | Signature: `FUNPAINT (MT) ` |
| 16 | 1 | Compression flag (0=none, non-zero=compressed) |
| 17 | 1 | RLE escape byte (if compressed) |
| 18 | Variable | Compressed data or uncompressed content |

**Uncompressed Size:** 33,694 bytes

### Memory Layout (Unpacked)

| Offset | Size | Description |
|--------|------|-------------|
| 0x0012 (18) | 8,000 | Video matrix 1 (8 banks × 1,000 bytes) |
| 0x2012 (8,210) | 8,000 | Bitmap 1 |
| 0x4012 (16,402) | 1,000 | Color RAM |
| 0x43FA (17,402) | 8,000 | Video matrix 2 (8 banks × 1,000 bytes) |
| 0x63FA (25,594) | 8,000 | Bitmap 2 |

### FLI Video Matrix Addressing

In FLI mode, the video matrix changes per scanline:
```cpp
video_offset = video_matrix_base + ((y & 7) << 10) + char_offset;
```

Each of the 8 scanline groups has its own 1,000-byte video matrix bank.

### Interlacing

Like DrazLace, FunPaint stores two complete frames that are blended:
- Frame 1: bitmap1 + video_matrix1, x offset 0
- Frame 2: bitmap2 + video_matrix2, x offset -1

The blending averages RGB values: `(c1 & c2) + ((c1 ^ c2) >> 1)`

### Identification

File contains signature `FUNPAINT (MT) ` at offset 2.

---

## C64 HiRes Format

**Extensions:** `.hbm`, `.fgs`, `.gih`, `.art`, `.hpc`
**Mode:** Hi-Res (320×200)

Generic C64 hi-res bitmap format used by various paint programs.

### File Structure

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Load address |
| 2 | 8,000 | Bitmap data |
| 8,002 | 1,000 | Video matrix (optional) |

**File Sizes:**
- 8,002 bytes (bitmap only)
- 8,194 bytes (GCD/MON variant)
- 9,002 bytes (with video matrix)
- 9,003 bytes (HPC variant)
- 9,009 bytes (AAS/ART variant)

### Default Colors

If video matrix is not present, default colors are used:
- Background (bit 0): Black (0)
- Foreground (bit 1): White (1)

Encoded as color byte `0x10` (upper nibble 1, lower nibble 0).

### Valid Load Addresses

Common C64 hi-res load addresses:
- 0x2000, 0x4000, 0x6000, 0xA000
- 0x5C00, 0x4100, 0x3F40
- 0x1C00, 0x6C00

### Identification

1. File size matches known hi-res sizes
2. First two bytes form a valid C64 load address

---

## Run Paint Format

**Extensions:** `.rpm`
**Mode:** Multicolor (160×200)

Run Paint is a multicolor format identical to Koala.

### File Structure

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Load address |
| 2 | 8,000 | Bitmap data |
| 8,002 | 1,000 | Screen RAM |
| 9,002 | 1,000 | Color RAM |
| 10,002 | 1 | Background color |

**File Sizes:**
- 10,003 bytes (standard)
- 10,006 bytes (extended variant)

### Valid Load Addresses

- 0x6000, 0x4000, 0x5C00, 0x2000

### Identification

1. File size is 10,003 or 10,006 bytes
2. First two bytes form a valid C64 multicolor load address

---

## RLE Compression Summary

Several C64 formats use similar RLE compression schemes:

| Format | Escape Byte | Structure |
|--------|-------------|-----------|
| GG (Koala) | 0xFE | escape, value, count |
| JJ (Doodle) | 0xFE | escape, value, count |
| AMI | 0xC2 | escape, count, value |
| DRP | Variable | escape, count, value |
| FunPaint | Variable | escape, count, value |

**Note:** The order of value/count differs between formats.

---

## References

- RECOIL (Retro Computer Image Library) - https://recoil.sourceforge.net/
- C64 Wiki - https://www.c64-wiki.com/
- Codebase64 - https://codebase64.org/
- VICE Emulator Documentation
