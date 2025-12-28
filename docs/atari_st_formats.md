# Atari ST Image Formats

This document describes the Atari ST image formats supported by onyx_image.

## Table of Contents

1. [Atari ST Graphics Overview](#atari-st-graphics-overview)
2. [Color System](#color-system)
3. [Bitplane Storage](#bitplane-storage)
4. [NEOchrome Format](#neochrome-format)
5. [DEGAS Format](#degas-format)
6. [Doodle Format](#doodle-format)
7. [Crack Art Format](#crack-art-format)
8. [Tiny Stuff Format](#tiny-stuff-format)
9. [Spectrum 512 Format](#spectrum-512-format)
10. [Photochrome Format](#photochrome-format)

---

## Atari ST Graphics Overview

The Atari ST uses a Motorola 68000 CPU with **big-endian** byte order. All multi-byte values in ST image formats are stored in big-endian format.

### Display Modes

| Mode | Resolution | Colors | Bitplanes | Bitmap Size |
|------|------------|--------|-----------|-------------|
| Low | 320×200 | 16 | 4 | 32,000 bytes |
| Medium | 640×200 | 4 | 2 | 32,000 bytes |
| High | 640×400 | 2 | 1 | 32,000 bytes |

All modes use exactly 32,000 bytes for the bitmap data (80 bytes per scanline × 400 scanlines for high-res, or 160 bytes × 200 scanlines for low/medium-res).

---

## Color System

### ST 512-Color Palette (Original ST)

The original Atari ST uses a 9-bit RGB palette with 3 bits per channel:

```
Word format (big-endian): 0000 0RRR 0GGG 0BBB

Bits 15-12: Unused (always 0)
Bits 11-9:  Red component (0-7)
Bits 8:     Unused (always 0)
Bits 7-5:   Green component (0-7)
Bits 4:     Unused (always 0)
Bits 3-1:   Blue component (0-7)
Bit 0:      Unused (always 0)
```

**Conversion to 8-bit RGB:**
```
R8 = (R3 << 5) | (R3 << 2) | (R3 >> 1)   // Approximately: R3 * 255 / 7
G8 = (G3 << 5) | (G3 << 2) | (G3 >> 1)
B8 = (B3 << 5) | (B3 << 2) | (B3 >> 1)
```

This produces values: 0, 36, 73, 109, 146, 182, 219, 255

### STE 4096-Color Palette (Enhanced ST)

The Atari STE extends the palette to 12-bit RGB with 4 bits per channel:

```
Word format (big-endian): 0000 rRRR gGGG bBBB

The 4-bit value for each channel is: (high 3 bits << 1) | (low bit)
Where the low bit is stored separately from the high 3 bits.
```

The STE uses the previously unused bits (bits 11, 7, 3) as the LSB of each channel.

**Detection:** A palette uses STE colors if any of bits 3, 7, or 11 are set in any color entry.

---

## Bitplane Storage

The Atari ST uses **word-interleaved bitplanes**. For each group of 16 horizontal pixels, the bitplane words are stored consecutively:

### Low Resolution (4 bitplanes)
```
Offset 0-1:   Plane 0, pixels 0-15
Offset 2-3:   Plane 1, pixels 0-15
Offset 4-5:   Plane 2, pixels 0-15
Offset 6-7:   Plane 3, pixels 0-15
Offset 8-9:   Plane 0, pixels 16-31
... and so on
```

### Extracting a Pixel Value

To get the color index for pixel at position (x, y):

```cpp
int group = x / 16;                    // Which 16-pixel group
int bit_pos = 15 - (x % 16);           // Bit position within word (MSB first)
size_t base = y * stride + group * bitplanes * 2;

uint8_t pixel = 0;
for (int plane = 0; plane < bitplanes; ++plane) {
    uint16_t word = read_be16(data + base + plane * 2);
    if (word & (1 << bit_pos)) {
        pixel |= (1 << plane);
    }
}
```

### Row Strides

| Mode | Formula | Value |
|------|---------|-------|
| Low | (320 / 16) × 4 × 2 | 160 bytes |
| Medium | (640 / 16) × 2 × 2 | 160 bytes |
| High | (640 / 16) × 1 × 2 | 80 bytes |

---

## NEOchrome Format

**Extensions:** `.neo`
**File Size:** 32,128 bytes (exactly)

NEOchrome was a popular paint program for the Atari ST.

### File Structure

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Flag word (must be 0x0000) |
| 2 | 2 | Resolution (0=low, 1=medium, 2=high) |
| 4 | 32 | Palette (16 colors × 2 bytes) |
| 36 | 12 | Original filename (space-padded) |
| 48 | 2 | Color animation limit |
| 50 | 2 | Color animation speed |
| 52 | 2 | Color animation step count |
| 54 | 2 | Color animation direction |
| 56 | 32 | Reserved |
| 88 | 40 | Padding |
| 128 | 32,000 | Bitmap data |

### Identification

1. File size is exactly 32,128 bytes
2. First two bytes are 0x00 0x00
3. Resolution byte (offset 3) is 0, 1, or 2

---

## DEGAS Format

**Extensions:** `.pi1`, `.pi2`, `.pi3` (uncompressed), `.pc1`, `.pc2`, `.pc3` (compressed)

DEGAS (and DEGAS Elite) was a popular paint program that supported both uncompressed and compressed formats.

### Uncompressed Format

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Compression flag (0x00 = uncompressed) |
| 1 | 1 | Resolution (0=low, 1=medium, 2=high) |
| 2 | 32 | Palette (16 colors × 2 bytes) |
| 34 | 32,000 | Bitmap data |

**File Sizes:**
- Standard DEGAS: 32,034 bytes
- DEGAS Elite: 32,066 bytes (includes animation data)

### Compressed Format (PackBits)

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Compression flag (0x80 = compressed) |
| 1 | 1 | Resolution |
| 2 | 32 | Palette |
| 34 | Variable | PackBits compressed bitmap |

### PackBits Decompression

The compression uses a modified PackBits algorithm, decompressing per-scanline with bitplane interleaving:

```
For each byte B:
  If B < 128:   Read (B + 1) literal bytes
  If B > 128:   Repeat next byte (257 - B) times
  If B == 128:  No-op (skip)
```

The decompression writes data in a specific order: for each scanline, it decompresses each bitplane separately, then interleaves them into the standard word-interleaved format.

### Identification

1. Compression byte is 0x00 or 0x80
2. Resolution byte is 0, 1, or 2
3. For uncompressed: file size matches expected size

---

## Doodle Format

**Extensions:** `.doo`
**File Size:** 32,000 bytes (exactly)

Doodle is a simple monochrome (high-resolution) format containing only raw bitmap data with no header.

### File Structure

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 32,000 | Raw monochrome bitmap (640×400, 1 bit per pixel) |

### Pixel Format

- Each byte contains 8 horizontal pixels
- Bit 7 (MSB) is the leftmost pixel
- Bit value 1 = black, 0 = white
- 80 bytes per scanline, 400 scanlines

### Identification

1. File size is exactly 32,000 bytes
2. Does NOT start with "CA" (to distinguish from Crack Art)

---

## Crack Art Format

**Extensions:** `.ca1`, `.ca2`, `.ca3`

Crack Art supports all three ST resolutions with optional compression.

### File Structure

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Signature "CA" (0x43 0x41) |
| 2 | 1 | Compression (0=none, 1=compressed) |
| 3 | 1 | Resolution (0=low, 1=medium, 2=high) |
| 4 | N | Palette (32 bytes for low, 8 for medium, 0 for high) |
| 4+N | Variable | Bitmap data (32,000 bytes if uncompressed) |

### Compression Algorithm

Crack Art uses a column-based RLE scheme:

1. Read 4-byte header: escape byte, default value, unpack step (2 bytes)
2. Decompress in column order based on unpack step

```
For each byte B:
  If B != escape: literal byte
  If B == escape:
    Read next byte C:
    If C == escape: literal escape byte
    If C == 0: read count byte, then value byte → repeat value (count+1) times
    If C == 1: read 2-byte count, then value → repeat value (count+1) times
    If C == 2: read count/flag → repeat default value
    Else: repeat next byte (C+1) times
```

### Identification

1. Starts with "CA" (0x43 0x41)
2. Byte at offset 2 is 0 or 1
3. Byte at offset 3 is 0, 1, or 2

---

## Tiny Stuff Format

**Extensions:** `.tny`, `.tn1`, `.tn2`, `.tn3`

Tiny Stuff is a compressed format that achieves good compression ratios.

### File Structure

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 1 | Mode (0-2: standard, 3-5: with animation header) |
| 1 | 32 | Palette (16 colors × 2 bytes) |
| 33 | 2 | Control bytes count |
| 35 | 2 | Data words count (multiply by 2 for bytes) |
| 37 | Variable | Control bytes stream |
| 37+ctrl | Variable | Data words stream |

If mode > 2, there's a 4-byte animation header before the content, and mode is adjusted by -3.

### Compression Algorithm

Tiny Stuff uses a two-stream RLE compression:
- Control stream: contains RLE commands
- Value stream: contains 16-bit data words

```
For each control byte B:
  If B < 128:
    If B == 0 or 1: read 2-byte count from control stream
    Else: count = B
    If B == 1: read (count) values from value stream (literal run)
    Else: read one value, repeat (count) times
  Else:
    count = 256 - B
    Read (count) values from value stream (literal run)
```

Decompression writes in a specific bitplane order: bitplanes 0,2,4,6 (as word pairs), column by column.

### Identification

1. Mode byte is 0-5
2. Control and value lengths produce reasonable file size

---

## Spectrum 512 Format

**Extensions:** `.spu` (uncompressed), `.spc` (compressed)

Spectrum 512 is an advanced format that displays 512 colors simultaneously by changing the palette during screen refresh. It achieves this by storing 48 palette entries per scanline (3 palette switches per line × 16 colors).

### Resolution

Always 320×199 pixels (the last scanline is not displayed due to palette timing).

### SPU Format (Uncompressed)

**File Size:** 51,104 bytes

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 160 | Unused (first 160 bytes skipped) |
| 160 | 31,840 | Bitmap data (199 lines × 160 bytes) |
| 32,000 | 19,104 | Palettes (199 lines × 48 colors × 2 bytes) |

### SPC Format (Compressed)

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Signature "SP" (0x53 0x50) |
| 2 | 2 | Reserved |
| 4 | 4 | Palette data offset (from file start) |
| 8 | 4 | Reserved |
| 12 | Variable | Compressed bitmap |
| ? | Variable | Compressed palette |

### Bitmap Interleaving

Spectrum 512 uses a special byte interleaving for the bitmap:

```cpp
// For pixel at linear offset pixelsOffset:
int idx = pixelsOffset >> 3;
size_t byteOffset = (idx & ~1) * 4 + (idx & 1);
int bit = ~pixelsOffset & 7;
```

### Per-Scanline Palette Selection

Each scanline has 48 palette entries (3 banks of 16). The bank is selected based on x-position:

```cpp
int x1 = c * 10 + 1 - (c & 1) * 6;
if (x >= x1 + 160) c += 32;
else if (x >= x1) c += 16;
```

### Identification

- SPU: exactly 51,104 bytes
- SPC: starts with "SP"

---

## Photochrome Format

**Extensions:** `.pcs`

Photochrome is another advanced format achieving more than 16 colors per scanline through palette switching.

### Resolution

Always 320×199 pixels.

### File Structure

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 4 | Header: 0x01 0x40 0x00 0xC8 |
| 4 | 2 | Unknown |
| 6 | Variable | Compressed data |

### Decompressed Structure

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 32,000 | Bitmap (separated bitplanes, not interleaved) |
| 32,000 | 19,104 | Palettes (199 lines × 48 colors × 2 bytes) |

### Bitplane Format

Unlike other ST formats, Photochrome uses **separated bitplanes**:
- Plane 0: bytes 0-7999
- Plane 1: bytes 8000-15999
- Plane 2: bytes 16000-23999
- Plane 3: bytes 24000-31999

Each plane contains 40 bytes per scanline (320 pixels / 8).

### Compression

Uses a block-based RLE similar to Tiny Stuff:
- First block: bitmap bytes
- Second block: palette words

### Palette Selection

Complex palette selection based on x-position:

```cpp
int c = pixel_value << 1;
if (x >= c * 2) {
    if (c < 28) {
        if (x >= c * 2 + 76) {
            if (x >= 176 + c * 5 - (c & 2) * 3) c += 32;
            c += 32;
        }
    } else if (x >= c * 2 + 92) {
        c += 32;
    }
    c += 32;
}
```

### STE Support

Photochrome can use STE 12-bit colors. Detection is automatic by checking if any palette entry has bits 3, 7, or 11 set.

### Identification

File starts with bytes: 0x01 0x40 0x00 0xC8

---

## References

- RECOIL (Retro Computer Image Library) - https://recoil.sourceforge.net/
- Atari ST Graphics File Formats - http://www.atari-forum.com/wiki/index.php?title=ST_Picture_Formats
- FileFormats.Wiki - https://www.fileformats.wiki/
