# CARRIER_AC128 IR Protocol Specification

Reverse-engineered from ~360 raw IR captures of a Houghton (Carrier-manufactured) window AC unit's remote control. This is the only public byte-level documentation of this protocol's semantics.

## Overview

- **Protocol:** CARRIER_AC128 (as identified by Tasmota/IRremoteESP8266)
- **Length:** 16 bytes (128 bits)
- **Structure:** Two 8-byte halves, each with an independent checksum
- **Encoding:** BCD (Binary-Coded Decimal) for all numeric values
- **Transmission:** Every button press sends the complete AC state (not deltas)

## Byte Map

| Byte | High Nibble (Bits 1-4) | Low Nibble (Bits 5-8) |
|------|------------------------|------------------------|
| 1 | `0x1` (fixed header) | `0x6` (fixed header) |
| 2 | Fan speed | Operating mode |
| 3 | Clock minutes tens (0-5) | Clock minutes ones (0-9) |
| 4 | Clock hours tens (0-2) | Clock hours ones (0-9) |
| 5 | Timer-on enable + hour tens | Timer-on hour ones (0-9) |
| 6 | Timer-off enable + hour tens | Timer-off hour ones (0-9) |
| 7 | Set point °C tens (1-3) | Set point °C ones (0-9) |
| 8 | **Checksum 1** | Flags: Celsius, Sleep |
| 9 | Timer-on minutes tens (0-5) | Timer-on minutes ones (0-9) |
| 10 | Timer-off minutes tens (0-5) | Timer-off minutes ones (0-9) |
| 11 | Tx counter (0-8, ignored) | Flags: Power |
| 12 | Set point °F tens (6-8) | Set point °F ones (0-9) |
| 13 | Not used | Flags: Eco |
| 14 | Flags: Lock, LED | Not used |
| 15 | Clock seconds tens (0-5) | Clock seconds ones (0-9) |
| 16 | **Checksum 2** | Not used |

## Byte Details

### Byte 1 — Header
Always `0x16`. Identifies the protocol.

### Byte 2 — Fan Speed + Operating Mode

**High nibble — Fan speed** (exactly one bit set):

| Value | Fan Speed |
|-------|-----------|
| `0x8` | Low |
| `0x4` | Medium |
| `0x2` | High |
| `0x1` | Auto |

**Low nibble — Operating mode** (one or two bits set):

| Value | Mode |
|-------|------|
| `0x1` | Dehumidify |
| `0x2` | Cool (AC) |
| `0x4` | Fan Only |
| `0x8` | Heat |
| `0xA` | Maintain (Heat + Cool) |

**Combined examples:** `0x12` = Auto fan + Cool, `0x81` = Low fan + Dehumidify, `0x4A` = Medium fan + Maintain

### Bytes 3-4 — Clock Time (Hours:Minutes)

BCD-encoded 24-hour clock. Sent with every command to sync the AC's internal clock.

- **Byte 3:** Minutes (high nibble = tens 0-5, low nibble = ones 0-9)
- **Byte 4:** Hours (high nibble = tens 0-2, low nibble = ones 0-9)

Example: 14:35 → Byte 3 = `0x35`, Byte 4 = `0x14`

### Bytes 5-6 — Timer Hours

Each byte encodes a timer enable flag + BCD hour:

- **Bit 1 (MSB):** Timer enable (1 = enabled)
- **Bits 2-4:** Hour tens digit (0-2)
- **Bits 5-8:** Hour ones digit (0-9)

| Byte | Timer |
|------|-------|
| 5 | Auto-ON timer hour |
| 6 | Auto-OFF timer hour |

Example: ON timer at 13:00 → Byte 5 = `0x93` (enable=1, hour=13 BCD)
Example: No timer → Byte 5 = `0x00` (enable=0, hour=0)

### Byte 7 — Temperature Set Point (Celsius)

BCD-encoded Celsius temperature. Valid range: 16-30°C.

- **High nibble:** Tens digit (1-3)
- **Low nibble:** Ones digit (0-9)

Example: 24°C → `0x24`

### Byte 8 — Checksum 1 + Flags

- **High nibble (bits 1-4):** Checksum 1 result (see [Checksum Algorithm](#checksum-algorithm))
- **Bit 5:** Celsius display flag (1 = display in °C, 0 = display in °F)
- **Bit 6:** Not used (always 0)
- **Bit 7:** Sleep mode (1 = enabled)
- **Bit 8:** Not used (always 0)

Low nibble values: `0x0` = Fahrenheit, `0x2` = Sleep + Fahrenheit, `0x8` = Celsius, `0xA` = Celsius + Sleep

### Bytes 9-10 — Timer Minutes

BCD-encoded minutes for the auto-on and auto-off timers.

- **Byte 9:** Timer-on minutes (high nibble = tens, low nibble = ones)
- **Byte 10:** Timer-off minutes (high nibble = tens, low nibble = ones)

Example: ON timer at 13:58 → Byte 9 = `0x58`

> **Note:** The original protocol spreadsheet labels byte 10 as "Timer On Minute" — this is a copy-paste error. It is the Timer **Off** minutes, verified against captured data.

### Byte 11 — Tx Counter + Power Flags

- **High nibble (bits 1-4):** Transmission counter (values 0-8 observed, no effect on AC operation). For programmatic control, always use `0x0`.
- **Bit 5:** Power ON flag (1 = unit running)
- **Bit 6:** Power OFF flag (1 = turn unit off)
- **Bits 7-8:** Not used (always 0)

| Low Nibble | Meaning |
|------------|---------|
| `0x8` | Unit ON |
| `0xC` | Unit OFF |
| `0x0` | Unit ON (also valid, seen in timer/celsius captures) |

Power ON is the typical state (`0x08` or `0x08` with counter in high nibble, e.g., `0x68`).

### Byte 12 — Temperature Set Point (Fahrenheit)

BCD-encoded Fahrenheit temperature. Valid range: 60-86°F.

- **High nibble:** Tens digit (6-8)
- **Low nibble:** Ones digit (0-9)

Example: 76°F → `0x76`

When in Celsius-only mode, this byte is `0x00`.

### Byte 13 — Eco Mode

- **Bits 1-5:** Not used (always 0)
- **Bit 6:** Eco mode (1 = enabled)
- **Bits 7-8:** Not used (always 0)

| Value | Meaning |
|-------|---------|
| `0x00` | Eco off |
| `0x04` | Eco on |

### Byte 14 — Lock + LED Display

- **Bit 1 (MSB):** Control lock (1 = locked)
- **Bit 2:** LED display disable (1 = display OFF, 0 = display ON)
- **Bits 3-8:** Not used (always 0)

| Value | Lock | LED Display |
|-------|------|-------------|
| `0x00` | Off | On |
| `0x40` | Off | Off |
| `0x80` | On | On |
| `0xC0` | On | Off |

> **Note:** Bit 2 is **inverted** — a 0 means the LED display is ON (active). The original spec labels the default as 1, but the most common captured value is `0x00` (LED on, no lock).

### Byte 15 — Clock Seconds

BCD-encoded seconds, same format as bytes 3-4.

- **High nibble:** Tens digit (0-5)
- **Low nibble:** Ones digit (0-9)

### Byte 16 — Checksum 2

- **High nibble (bits 1-4):** Checksum 2 result (see [Checksum Algorithm](#checksum-algorithm))
- **Low nibble (bits 5-8):** Not used (always 0)

## Checksum Algorithm

Both checksums use the same algorithm: **sum all individual hex digits, then take modulo 16**.

### Checksum 1 (Byte 8, high nibble)

1. Take bytes 1 through 7
2. Split each byte into its two hex digits (high nibble and low nibble)
3. Sum all 14 hex digits
4. Add the low nibble of byte 8 (the flags: celsius, sleep)
5. Take the result modulo 16
6. Store in the high nibble of byte 8

### Checksum 2 (Byte 16, high nibble)

1. Take bytes 9 through 15
2. Split each byte into its two hex digits
3. Sum all 14 hex digits
4. Add the low nibble of byte 16 (always 0)
5. Take the result modulo 16
6. Store in the high nibble of byte 16

### Worked Example

**Input:** Checksum 1 for bytes `16 81 42 06 00 00 24` with flags low nibble = `0x0`

```
Hex digits: 1+6 + 8+1 + 4+2 + 0+6 + 0+0 + 0+0 + 2+4 + 0 = 34
34 mod 16 = 2
Byte 8 = 0x20 (checksum 2 in high nibble, 0 flags in low nibble)
```

## BCD Encoding Reference

All numeric fields use Binary-Coded Decimal:

```
Decimal 24 → BCD 0x24 (high nibble = 2, low nibble = 4)
Decimal 76 → BCD 0x76
Decimal 0  → BCD 0x00
Decimal 58 → BCD 0x58
```

To encode: `bcd = ((value / 10) << 4) | (value % 10)`
To decode: `value = (bcd >> 4) * 10 + (bcd & 0x0F)`

## Temperature Conversion

The AC expects both Celsius and Fahrenheit set points. Valid ranges:

| Scale | Range | BCD Range |
|-------|-------|-----------|
| Celsius | 16-30°C | `0x16` - `0x30` |
| Fahrenheit | 60-86°F | `0x60` - `0x86` |

Standard conversion: `°C = round((°F - 32) × 5/9)`

## Worked Examples

### Example 1: Dehumidify, Low Fan, 76°F

Power ON, Dehumidify, Low fan, 76°F (24°C), no timers, LED on, Fahrenheit display. Clock: 06:42:02.

```
Byte  1: 0x16  — Header (always)
Byte  2: 0x81  — Low fan (0x8_) + Dehumidify (0x_1)
Byte  3: 0x42  — Minutes: 42
Byte  4: 0x06  — Hours: 06
Byte  5: 0x00  — ON timer: disabled
Byte  6: 0x00  — OFF timer: disabled
Byte  7: 0x24  — Set point: 24°C
Byte  8: 0x20  — CS1=2, Fahrenheit, no sleep
Byte  9: 0x00  — ON timer minutes: 00
Byte 10: 0x00  — OFF timer minutes: 00
Byte 11: 0x68  — Counter=6, power ON
Byte 12: 0x76  — Set point: 76°F
Byte 13: 0x00  — Eco: off
Byte 14: 0x00  — Lock: off, LED: on
Byte 15: 0x02  — Seconds: 02
Byte 16: 0xD0  — CS2=D, unused

CS1: 1+6+8+1+4+2+0+6+0+0+0+0+2+4 = 34, +0 = 34, mod 16 = 2 ✓
CS2: 0+0+0+0+6+8+7+6+0+0+0+0+0+2 = 29, +0 = 29, mod 16 = 13 = D ✓
```

### Example 2: Maintain Mode, Eco ON, Timer ON at 08:00

Power ON, Maintain (Heat+Cool), Medium fan, 80°F (27°C), eco mode ON, ON timer enabled at 00:00. Clock: 07:39:07.

```
Byte  1: 0x16  — Header
Byte  2: 0x4A  — Medium fan (0x4_) + Maintain (0x_A)
Byte  3: 0x39  — Minutes: 39
Byte  4: 0x07  — Hours: 07
Byte  5: 0x80  — ON timer: enabled, hour=00 (midnight)
Byte  6: 0x00  — OFF timer: disabled
Byte  7: 0x27  — Set point: 27°C
Byte  8: 0x90  — CS1=9, Fahrenheit, no sleep
Byte  9: 0x00  — ON timer minutes: 00
Byte 10: 0x00  — OFF timer minutes: 00
Byte 11: 0x68  — Counter=6, power ON
Byte 12: 0x80  — Set point: 80°F
Byte 13: 0x04  — Eco: on
Byte 14: 0x00  — Lock: off, LED: on
Byte 15: 0x07  — Seconds: 07
Byte 16: 0x10  — CS2=1, unused

CS1: 1+6+4+10+3+9+0+7+8+0+0+0+2+7 = 57, +0 = 57, mod 16 = 9 ✓
CS2: 0+0+0+0+6+8+8+0+0+4+0+0+0+7 = 33, +0 = 33, mod 16 = 1 ✓
```

### Example 3: Cool, Timers ON+OFF, with Timer Minutes

Power ON, Cool, Auto fan, 77°F (25°C), ON timer 13:57, OFF timer 7:07. Clock: 14:23:10.

```
Byte  1: 0x16  — Header
Byte  2: 0x12  — Auto fan (0x1_) + Cool (0x_2)
Byte  3: 0x23  — Minutes: 23
Byte  4: 0x14  — Hours: 14
Byte  5: 0x93  — ON timer: enabled, hour=13
Byte  6: 0x87  — OFF timer: enabled, hour=07
Byte  7: 0x25  — Set point: 25°C
Byte  8: 0x60  — CS1=6, Fahrenheit, no sleep
Byte  9: 0x57  — ON timer minutes: 57
Byte 10: 0x07  — OFF timer minutes: 07
Byte 11: 0x08  — Counter=0, power ON
Byte 12: 0x77  — Set point: 77°F
Byte 13: 0x00  — Eco: off
Byte 14: 0x00  — Lock: off, LED: on
Byte 15: 0x10  — Seconds: 10
Byte 16: 0xA0  — CS2=A, unused

CS1: 1+6+1+2+2+3+1+4+9+3+8+7+2+5 = 54, +0 = 54, mod 16 = 6 ✓
CS2: 5+7+0+7+0+8+7+7+0+0+0+0+1+0 = 42, +0 = 42, mod 16 = 10 = A ✓
```

## Errata from Original Protocol Spreadsheet

The original reverse-engineering spreadsheet (`Houghton AC IR Protocol2.xlsx`) contains several errors:

1. **Byte 10** is labeled "Timer On Minute Digit 1/2" — should be "Timer **Off** Minute". Verified against captures where ON and OFF timer minutes differ.
2. **Byte 8 bit 5** (Celsius flag) default listed as 1 — actual default is 0 (Fahrenheit). The note "0=USE FAHRENHEIT" is correct; the default value is wrong.
3. **Byte 15** header labeled "BYTE 3" — should be "BYTE 15" (copy-paste error in spreadsheet).
4. **Byte 14 bit 2** (LED display) default listed as 1 — actual most common value is 0 (LED on). The bit is inverted: 0=LED on, 1=LED off.
