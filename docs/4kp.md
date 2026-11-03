# 4KP: cartridge scheme specification for emulators

Newbroo Games, LLC

4KP ("4K banks, Paged RAM") is a bank switching scheme for Atari 2600 cartridges
with up to 1MB of ROM and 32K of RAM. The cartridge is built from parts of the
6507's era and kind: flash, SRAM, three latches and a GAL. There is no
coprocessor. This document describes everything an emulator needs to run 4KP
images.

**Status (2026-11-02):** 4KP runs in the Newbroo Games Stella 7.0c fork (branch
`newbroo-7.0c`, class `Cartridge4KP`) and in a 6507 simulator, and a test ROM
passes in both. Prototype boards are being built.

Related schemes: `3po.md` (3E+ with an outer bank register, 1K slots up to 16MB)
and `3pd.md` (3PO plus DPC-style data fetchers).

## 1. Summary

- **ROM:** up to 256 banks of 4K. A write to `$3F` maps a whole bank into
  `$1000-$1FFF`. There is no fixed bank.
- **RAM:** 32K, as 256 pages of 128 bytes. Two windows, A and B, sit over the
  bottom 512 bytes of every bank, so the RAM is visible whichever bank is
  mapped. Writes to `$3E` and `$3D` choose the page each window shows.
- **Ports:** each window has a write port and a separate read port, as on a
  Superchip cartridge.

## 2. Memory map

Cartridge space (A12 = 1). `$1xxx` and `$Fxxx` are the same lines.

| Address | Contents |
|---|---|
| `$1000-$107F` | window A, write port |
| `$1080-$10FF` | window B, write port |
| `$1100-$117F` | window A, read port |
| `$1180-$11FF` | window B, read port |
| `$1200-$1FFF` | ROM: bytes `$200-$FFF` of the current 4K bank |

- In `$1000-$11FF`: A0-A6 select the byte in the page, A7 the window (0 = A,
  1 = B) and A8 the port (0 = write, 1 = read).
- The RAM byte is `page[window] * 128 + (address & $7F)`.
- The first 512 bytes of every 4K bank in the image are never visible.
- Both windows may show the same page. A byte written through one window can
  then be read through the other.

## 3. Switch registers

| Address | Sets | Value |
|---|---|---|
| `$003F` | the ROM bank | 8 bits, taken modulo the number of 4K banks in the image |
| `$003E` | window A's page | 0-255 |
| `$003D` | window B's page | 0-255 |

- **Exact decoding.** The cartridge decodes all 13 address lines: only an
  address whose low 13 bits are `$003D`, `$003E` or `$003F` switches. TIA
  mirrors such as `$007F`, `$00BF`, `$00FF` and `$013D-$013F` do not. Unlike
  3F (Tigervision), ordinary TIA writes never switch.
- **Reads switch too.** The cartridge port has no read/write line, so any access
  to these addresses loads the register from the data bus. On a write, that's
  the value written. On a read, it's whatever is on the bus at the end of the
  cycle: the TIA drives only D6 and D7 for a read, and the other pins float.
  Programs must never read these addresses (section 7), so any plausible value
  will do for a read, but the read must switch. The Stella fork uses the value
  the TIA read returns.
- **The TIA is unaffected.** It ignores writes to `$2D-$3F`.
- **Immediate effect.** The fetch after `sta $3F` comes from the new bank, at
  the next address.
- **Bank number:** a 512K image (128 banks) ignores bit 7, as the 512K board
  does. A 1MB image uses all 8 bits. A smaller image wraps (bank modulo the
  bank count), as an image burned repeatedly to fill the flash would.

## 4. RAM port behaviour

| Access | Result |
|---|---|
| read of a read port (`$1100-$11FF`) | the RAM byte |
| write to a write port (`$1000-$10FF`) | stores the byte |
| read of a write port | the cartridge treats it as a write: whatever is on the data bus (nothing drives it, so in practice the last value on the bus) is stored in the RAM byte, and the CPU reads that same floating value. The same as a Superchip cartridge. |
| write to a read port | the SRAM drives the bus against the CPU. The RAM is unchanged: ignore the write (a debugger may want to flag it) |
| write to the ROM (`$1200-$1FFF`) | no effect |

The Stella fork handles a read of a write port as it does for Superchip
cartridges: it stores and returns Stella's stand-in for the floating bus (a
pseudo-random byte), and its developer settings can break on it.

## 5. Every bus access counts

The cartridge sees only the address and data lines, so the 6507's phantom
(dummy) accesses act exactly like real ones. Two cases that programs can meet:

- **`sta $3D,x`** (zero page, indexed) reads `$3D` before adding X. Window B's
  page switches to whatever is on the bus before the store lands.
- **An indexed store into the RAM that crosses a page** makes a dummy read in
  the page below. If that address is in `$1000-$10FF`, the floating bus is
  written into a RAM byte.

An emulator that passes every access to the cartridge, phantom ones included,
gets both right without special handling. The test ROM also makes zero-page
reads whose operand bytes sit at `$xx3D-$xx3F` (cartridge addresses). Those must
not switch.

## 6. Power-up, file format and state

- **Power-up:** on the board, the bank register, both page registers and all 32K
  of RAM start random. Programs carry a reset stub in every bank, so any start
  bank works. An emulator may start everything at zero or at random. The test
  ROM passes either way, and random is closer to the hardware.
- **In the Stella fork:** bank 0 and both pages 0 by default; all three random
  when Stella's "Random startup bank" setting is on. The RAM follows Stella's
  "Randomize zero-page and extended RAM" setting (random, or zero).
- **File:** extension `.4kp`. A raw image, bank 0 first, 4K per bank. 8K to
  1MB, a multiple of 4K. The Stella fork pads a short last bank with `$FF`.
- **Detection:** by the extension, or in Stella by `-bs 4KP` for a file whose
  extension it doesn't know. There is no autodetection and no reliable
  fingerprint. Programs built with the recommended convention end every bank
  with the same 36 bytes at `$FFDC-$FFFF`, but that is a convention, not a
  requirement.
- **State to save:** the ROM bank (0-255), window A's page, window B's page, and
  the 32K of RAM.

## 7. Rules for programs

A debugger may want to warn about these. The Stella fork's developer settings
already break on rules 2 and 3, through the same checks it uses for Superchip
cartridges.

1. Never read `$3D`, `$3E` or `$3F` (section 3).
2. Never read the write ports, `$1000-$10FF`.
3. Never write the read ports, `$1100-$11FF`.
4. No read-modify-write instructions (`inc`, `dec`, `asl`, `lsr`, `rol`, `ror`)
   on the RAM. Read from `$11xx`, write to `$10xx`.
5. Keep indexed stores inside the write page: no carry into the next page.
6. Switch the ROM bank only from code that is the same bytes at the same
   address in both banks (the shared block at `$FFDC`).
7. Set both pages before using the RAM (power-up is random).
8. Never index into `$3D-$3F` (section 5). A clear loop through zero page
   (`sta $00,x`) writes 0 to all three, which switches to ROM bank 0. DASM's
   `CLEAN_START` clears through the stack at `$0100-$01FF`, which doesn't
   switch.

## 8. Test ROMs

**Passing:** a dark blue screen with a steady white "OK" over a two-digit count
of completed sweeps (00-99). A sweep takes about 143 frames (147 on the 1MB
ROM). In the Stella fork the 512K ROM shows "OK 02" at frame 300. Both ROMs pass
from any start bank, with any starting pages and RAM.

**Failing:** an orange screen, frozen, with a blinking two-letter code over a
number. It blinks slowly for a ROM fault and fast for a RAM fault. GAME RESET
clears it.

Each sweep runs these steps, in this order:

| Step | Checks | Code on failure |
|---|---|---|
| ROM walk | a far call into every bank reads that bank's number | FL 12-19: the bank bit (12-18 for bits 0-6, 19 for bit 7 on 1MB) |
| RA | the data bits, then each RAM address line A0-A6 | RA 00-06 |
| WS | the two windows on pages `$55` and `$AA` stay apart | WS 07 |
| PA, PB | each page bit of window A, then of window B | PA 00-07, PB 00-07 |
| RK | switches made by code running from the RAM: a routine in window A switches window B's page and the ROM bank; one in window B switches window A's page | RK 03 (bank), 04 (A), 05 (B) |
| RD | fill all 256 pages through one window, walk the ROM banks again, check every page through the other window | RD nn: the lowest wrong data bit |
| (after every switch) | glitch bait: reads near `$xx3D-$xx3F` must not switch anything | SW 03 (bank), 04 (A), 05 (B) |

Some emulator mistakes and where they show:
- Hotspots decoded on fewer than 13 lines: **SW**.
- One page register shared by both windows: **WS**.
- Bit 7 of the bank ignored on a 1MB image: **FL 19**.
- RAM index built from the wrong address bits: **RA**, **PA** or **PB**.

## 9. Reference implementations

- **Stella 7.0:** the Newbroo Games Stella 7.0c fork, branch `newbroo-7.0c`:
  `src/emucore/Cart4KP.cxx` and `Cart4KP.hxx` (about 415 lines), registered in
  `Bankswitch.cxx` and `CartCreator.cxx` as type `4KP`. The fork raises
  Stella's file size limit, `Cartridge::maxSize()`, from 512K to 16MB (for 4KP's
  1MB and 3PO's 16MB).
