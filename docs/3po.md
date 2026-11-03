# 3PO specification

Newbroo Games, LLC

3PO is 3E+ with one write-only register at `$2F` that holds the upper bits of the
ROM bank number. It takes 3E+'s 1K slots from 64K of ROM to 16MB, and every 3E+
game runs on it unchanged. The name is "3E Plus, Outer bank", said like C-3PO.

**Status (2026-11-02):** 3PO runs in the Newbroo Games Stella 7.0c fork (branch
`newbroo-7.0c`, class `Cartridge3PO`) and in a 6507 simulator, and its test ROM
passes in both at every size from 512K to 4MB. Firmware for a Pico-based cart is
written and checked on a PC, not yet on a console. No 3PO board is built yet.

Related schemes: `3pd.md` (3PO plus DPC-style data fetchers) and `4kp.md` (4K banks
and paged RAM).

## 1. Summary

3E+ packs the slot number and the bank number into one byte written to `$3F`: 2 bits
of slot, 6 bits of bank. That stops it at 64 banks of 1K (64K of ROM). 3PO adds an
"outer" bank register at `$2F` that supplies the bank number's upper 8 bits.

|  | 3E+ | 3PO |
| --- | --- | --- |
| ROM | 64 banks of 1K (64K) | up to 16,384 banks of 1K (16MB) |
| RAM | 64 banks of 512 bytes (32K) | the same |
| Slots | four 1K slots, each ROM or RAM | the same |
| Switch registers | `$3E` (RAM), `$3F` (ROM) | `$3E`, `$3F`, and `$2F` (outer bank) |
| ROM bank put in a slot by `$3F` | D5-D0 | outer x 64 + D5-D0 |

The ROM bank number is 14 bits:

| Bits 13-6 | Bits 5-0 |
| --- | --- |
| the outer register, written at `$2F` | D5-D0 of the byte written to `$3F` |

**3E+ is the case outer = 0.** 3PO powers up with the outer at 0, and a 3E+ game
never writes `$2F`, so every bank number means what it means on 3E+.

## 2. Memory map

Exactly 3E+'s. Cartridge space is A12 = 1, so `$1xxx` and `$Fxxx` are the same lines.
A slot holding RAM reads at its first 512 bytes and writes at its last 512.

| Slot | Address | RAM in it: read port | RAM in it: write port |
| --- | --- | --- | --- |
| 0 | `$1000-$13FF` | `$1000-$11FF` | `$1200-$13FF` |
| 1 | `$1400-$17FF` | `$1400-$15FF` | `$1600-$17FF` |
| 2 | `$1800-$1BFF` | `$1800-$19FF` | `$1A00-$1BFF` |
| 3 | `$1C00-$1FFF` | `$1C00-$1DFF` | `$1E00-$1FFF` |

## 3. Switch registers

Three write-only registers; only `$2F` is new.

| Write to | Byte | Does |
| --- | --- | --- |
| `$002F` | the outer bank, 0-255 | stores it; **no slot changes** |
| `$003F` | D7-D6 the slot, D5-D0 the low bank bits | ROM bank **outer x 64 + D5-D0** into the slot |
| `$003E` | D7-D6 the slot, D5-D0 the RAM bank | RAM bank D5-D0 into the slot, as 3E+ (the outer isn't used) |

- **Each slot keeps its whole bank number.** The cart latches outer x 64 + D5-D0 into
  the slot at the `$3F` write. A later `$2F` write moves nothing; it only changes what
  the next `$3F` writes reach. So all four slots can show banks from anywhere in the
  ROM, and code can change the outer without pulling its own bank out from under
  itself.
- **The outer register is sticky.** One `$2F` write serves any number of `$3F` writes
  after it.
- **Past the end wraps.** A bank number at or past the image's bank count is taken
  modulo the bank count. For a power-of-two image that is what a board does when the
  outer bits past its size aren't connected. Games shouldn't count on it. (Stella's
  3E+ instead sends a bank number past the ROM to a RAM bank, because it numbers RAM
  banks after the ROM's.)
- **Exact decoding.** Only an address whose low 13 bits are `$002F`, `$003E` or
  `$003F` switches. Mirrors such as `$006F`, `$012F` and `$103F` don't. Stella's 3E+
  also switches on a write to `$103E`/`$103F` (a write into slot 0's own space); 3PO
  doesn't, as a board decoding all 13 address lines wouldn't.
- **Reads switch too.** The cartridge port has no read/write line, so any access to
  those three addresses loads the register from whatever is on the data bus. Programs
  must never read them (section 6). The Stella fork switches on a read of them, with
  the TIA's value, so testing in the emulator catches it. Stella's own 3E and 3E+
  switch on writes only (section 8).
- **`$002E`:** 3PO ignores it. It is the only other TIA address that is free for both
  reads and writes. 3PD (`3pd.md`) uses it as its control register.

**Why `$2F`.** The TIA ignores writes to `$2D-$3F`, and it decodes reads with A0-A3
only, so a read of `$x0-$xD` is always a TIA register. Of the write-free addresses,
only `$2E`, `$2F`, `$3E` and `$3F` are also read-free. `$3C` and `$3D` would have
been the obvious neighbours, but they read as INPT4 and INPT5 for code that uses
`TIA_BASE_READ_ADDRESS = $30`.

## 4. Power-up

- **The outer register is 0.** A board must guarantee it, since the cartridge port
  has no reset line: a power-on reset on its latch, or firmware. This is what lets a
  3E+ game run unchanged on a 3PO board with a bigger flash.
- Slot 3 holds ROM bank 0, so the reset vector is at the end of bank 0 (`$FFFC` in
  slot 3). Slots 0-2 hold random ROM banks and the RAM is random, as in Stella's 3E+.

## 5. The image and detection

- A whole number of 1K banks, 1K to 16MB, bank 0 first. A power of two for a board.
- **Detection** in the Stella fork, in this order:
  1. The file extension `.3po`. It overrides everything else.
  2. `-bs 3PO` on the command line (or the type chosen in the game's properties), for
     a file whose extension Stella doesn't know, such as `.bin`.
  3. Autodetection: the four bytes `C3PO` anywhere in the image. They are checked
     before 3E+'s `TJ3E` (and after 3PD's `3PD1`), so an image may carry both keys.
- Stock Stella opens files up to 512K; the fork raises its limit to 16MB.

## 6. Programming

A switch inside the 64K the outer already points at is 3E+'s own 4 bytes and 5
cycles; only a switch to another 64K costs more.

```
; ROM bank B (0-16383) into slot S (0-3), from code in another slot
        lda #B / 64
        sta $2F             ; the outer bank (skip it when it's already right)
        lda #S * 64 + (B & 63)
        sta $3F
```

| Switch | Bytes | Cycles |
| --- | --- | --- |
| ROM, inside the 64K the outer already points at (`$3F` only) | 4 | 5 |
| ROM, to another 64K (`$2F`, then `$3F`) | 8 | 10 |
| RAM (`$3E`, as 3E+) | 4 | 5 |

Rules:

1. **Never read `$2F`, `$3E` or `$3F`, and never index into them** (an indexed access
   makes a dummy read). A clear loop through zero page (`sta $00,x`) writes 0 to all
   three: outer 0, ROM bank 0 and RAM bank 0 into slot 0. DASM's `CLEAN_START` clears
   through the stack at `$0100-$01FF`, which doesn't switch.
2. **The outer register can't be read back.** Code that needs to know it keeps a copy
   in RAM, or writes it before every `$3F` write whose 64K it isn't sure of (a far-call
   macro can always write both).
3. **Code switching the slot it runs in** gets its next instruction from the new bank,
   as in 3E+. A `$2F` write never moves the running slot. On a cart that serves big
   images from slow memory (a microcontroller cart reading its flash), the next fetch
   after such a switch can be late; switch from another slot, or from code in the
   console's RAM.
4. **RAM from every bank:** keep one slot on RAM for the whole game (slot 0 is the
   natural one) and assemble the 1K ROM banks for the other slots. A 1K ROM bank is
   assembled for one slot's addresses, so the RAM must sit in the same slot for every
   bank.
5. **3E+'s RAM rules stay:** never read a write port (it writes junk into the byte),
   no read-modify-write on cart RAM, no indexed store into a write port that crosses a
   page.

**Compatibility.** A 3E+ game breaks on 3PO only if it writes something other than 0
to `$2F`, reads `$2F`/`$3E`/`$3F`, or relies on Stella's switch at `$103E`/`$103F`.
None should. In an emulator a 3E+ image is at most 64 banks, so it wraps and the outer
can't matter at all; it matters only on a board whose flash is bigger than the game.

## 7. Hardware notes

A discrete board needs three things beyond a 3E+ board: an 8-bit latch at `$2F` with
a power-on clear, its decode, and wider slot registers. Each slot's bank register
grows from 6 bits to 6 + k:

| Image | ROM banks | Bank bits | Outer bits used (k) |
| --- | --- | --- | --- |
| 64K (3E+) | 64 | 6 | 0 |
| 256K | 256 | 8 | 2 |
| 512K | 512 | 9 | 3 |
| 1MB | 1,024 | 10 | 4 |
| 4MB | 4,096 | 12 | 6 |
| 16MB | 16,384 | 14 | 8 |

- An 8-bit register chip per slot (a '574) has 2 bits spare over 3E+'s 6, so 256K
  needs no new register chips; more needs a second chip per slot.
- The memory is the hard part: no 5V parallel flash over 512K is still made.
- A microcontroller cart needs only firmware: 3E+'s bus loop with the `$2F` write
  added. On a Pico-based cart, a game that fits in the microcontroller's RAM runs as
  fast as 3E+; a bigger one is read from its flash, with less timing margin, and must
  follow rule 3 of section 6.

## 8. Reference implementation and tests

- **Stella:** the Newbroo Games Stella 7.0c fork, branch `newbroo-7.0c`:
  `src/emucore/Cart3PO.cxx` and `.hxx`, a subclass of Stella's `Cartridge3EPlus`;
  `src/debugger/gui/Cart3POWidget.cxx` and `.hxx`, the debugger's cart page, which
  shows each slot's bank number (up to 16383) and the outer register. Type `3PO`.
- **A 6507 simulator** with a 3PO model that also flags the hazards of section 6 as a
  program runs: reads of `$2F`/`$3E`/`$3F`, a read of a write port, read-modify-write
  on cart RAM, an indexed store's dummy read in a write port.
- **A DASM include** with macros for every switch and for cart RAM stores. It stops
  the build on a slot or bank out of range, a store label that isn't a read port, or
  an indexed store whose table crosses a page, and warns on a switch of the slot the
  code runs in.
- **Test ROM:** made from a 3E+ test ROM; every bank's first four bytes are its ID. It
  walks all four slots through every bank of the image and reads each ID back, checks
  the other slots didn't move, runs the 3E+ RAM tests, then tests the outer register
  itself: outer 0 at power-up, kept for a second `$3F` write, a later `$2F` write
  moving no slot, the wrap one past the end, and RAM banks ignoring it. A pass shows
  "OK" over a sweep count on a dark blue screen; a failure shows a code naming the
  fault on an orange one.

**Notes for emulator authors, from the Stella work:**

- **Why Stella's 3E and 3E+ switch on writes only.** Stella sends every access in a
  page the cart claims to the cart, reads included; the page's `WRITE` type is only a
  label. So a read of `$00-$3F` does reach their `peek()`, but it passes the read on to
  the TIA without calling `checkSwitchBank()`. 3PO's `peek()` calls it, with the
  value the TIA returns.
- **A debugger bug, fixed in the fork.** Stella's `Cartridge::bankOrigin()` finds a
  RAM bank's access flags at (ROM banks + RAM bank) x the ROM bank size. 3E+'s RAM
  banks are 512 bytes, so for RAM bank 32 and up it reads up to 32K past the end of
  the flags. With 3E+'s small images that went unnoticed; with a 3PO image of 512K or
  more Stella crashed whenever the debugger drew a slot holding RAM bank 32-63. The
  fork's fix (`src/emucore/Cart.cxx`) keeps the loop inside the flags. Upstream
  Stella 8.0 has the same code.

Results (2026-10-21), all pass:

| Check | Result |
| --- | --- |
| Test ROM at 512K, 1MB, 2MB, 4MB in the Stella fork | OK, 3 or 4 sweeps each (a sweep is about 3.4 s at 512K, 11 s at 4MB) |
| The same four in the simulator, 300 frames | 262 lines on every frame, no hazard flagged |
| The 512K test ROM forced to 3E+ | fails with `F0 16`, as it must (3E+ ignores the outer) |
| The 3E+ test ROM run as 3PO | passes, in Stella and in the simulator |
| 22 deliberately broken carts in the simulator | each shows its own fault code; the correct one passes |

The broken carts cover: the outer ignored; the outer applied at read time to every
slot instead of latched; the outer not kept between `$3F` writes; the outer moving a
RAM bank; no wrap; the outer not 0 at power-up; and each outer bit and bank bit stuck.

## 9. Design choices

These are settled, with the reasons. Comments are welcome.

1. **The address, `$2F`:** read-free and write-free for the TIA (section 3), next to
   3E+'s own registers.
2. **Latching:** the outer is latched into each slot at the `$3F` write, rather than
   one outer applied to every slot at read time. Four slots can then come from four
   different 64K areas, and changing the outer never moves running code.
3. **Wrapping:** a bank number past the end wraps, as a latch board does by itself
   (its unused top bits reach no address line), and as Stella does for every scheme
   without cart RAM.
4. **Reads switch, in the emulator too,** so an emulator catches what a board would
   do. The bank a read picks is whatever byte is on the bus then (the TIA's, or
   floating bits), so a stray read lands a slot on a junk bank.
5. **RAM stays at 3E+'s 32K.** 3PD has since taken `$2E`, the natural place for a RAM
   outer register, so a RAM outer for 3PO would need another address.
6. **The ROM key** is the bytes `C3PO`, after the scheme's name.
