# 3PD specification: 3PO plus DPC-style data fetchers

Newbroo Games, LLC

3PD is 3PO (`3po.md`) with eight data fetchers in the style of Activision's DPC chip
(Pitfall II), read through DPC-style registers or CDFJ+-style fast fetch. It is
designed for a Pico-based cart (a microcontroller cart); it is not meant for a
flash/SRAM/GAL board. The fetchers are fixed cart features, the same for every game:
they change what a read returns and run no game code.

**Status: new (2026-11-01), emulated only, no carts yet.** 3PD runs in the Newbroo
Games Stella 7.0c fork (branch `newbroo-7.0c`, class `Cartridge3PD`) and in a 6507
simulator, and its test ROM passes in both. The Pico firmware is written and checked
on a PC only (section 12). Details may still change, in particular the size of the
display area (section 4).

The section numbers are cited by the Stella source; they stay fixed.

## 0. Design choices

| Choice | Decision |
|---|---|
| Where the registers live | a 128-byte block over the first 128 bytes of a slot the game picks, switched on by a write to `$2E` |
| Features | the 8 fetchers, windows (masked reads), DPC's 3-voice music, fast fetch (`LDA #`), and more of DPC (section 14): the random number, bit-arranged reads, the clock select bit, draw-line |
| What the fetchers point at | a display area (the image's last 256K) or the cart RAM |
| Name and key | 3PD; the ROM key is the bytes `3PD1` (the digit is for a later revision, `3PD2`) |

## 1. Summary

- Everything 3PO does, unchanged: `$2F` the outer bank, `$3F` ROM into a slot, `$3E`
  RAM into a slot; 1K slots, up to 16MB of ROM, 32K of RAM.
- **`$2E`, the control register:** the fetcher block on or off, the slot it covers,
  fast fetch on or off. 0 at power-up, so **every 3E+ and 3PO game runs unchanged**.
- **The fetcher block:** while on, the first 128 bytes of the chosen slot are the
  fetchers' registers: 64 to read, 64 to write.
- **8 fetchers**, each a 24-bit counter that counts down one step a read, with DPC's
  window (a top, a bottom, a flag) and DPC's music voices on fetchers 5-7.
- **Fast fetch:** while on, `LDA #n` with n from `$04` to `$17` returns read register
  n.

## 2. The control register, `$2E`

| Bit | |
|---|---|
| D7 | 1 = the fetcher block on |
| D6 | 1 = fast fetch on |
| D5-D2 | reserved: write 0 |
| D1-D0 | the slot the block covers (0-3) |

- Write only. As with 3PO's switches, **any access switches**: the cart has no
  read/write line, so a read of `$2E` loads it from whatever is on the bus. Never read
  it, never index into it.
- 3PO leaves `$2E` free (it was the natural place for a RAM outer register); 3PD takes
  it. A RAM outer register for 3PD would need another address.
- Exact decoding, as 3PO: only an address whose low 13 bits are `$002E`; mirrors
  don't switch.
- The block and fast fetch are independent: fast fetch works with the block off (the
  fetchers keep their settings), and the block works without fast fetch.
- Turning fast fetch off cancels a pending fast fetch (section 6).

## 3. The fetcher block

While D7 is on, in slot S (D1-D0), base B = `$1000 + S x $400`:

| Addresses | |
|---|---|
| B + `$00`-`$3F` | the **read** registers, function x 8 + fetcher |
| B + `$40`-`$7F` | the **write** registers, `$40` + function x 8 + fetcher |
| B + `$80`-`$3FF` | the slot's bank, as always |

Whatever bank the slot holds (ROM or RAM) is hidden under the block while it's on and
comes back when it's off. **A game leaves the first 128 bytes of each bank it puts in
that slot unused** (data only, never code, is the safe rule).

The block is DPC's register layout (as in Stella's `CartDPC.cxx`), with the counter
widened.

### Read registers (B + `$00`-`$3F`)

| Function | Address | Returns |
|---|---|---|
| 0 | B+`$00`-`$03` | the **random number** (section 14.1) |
| 0 | B+`$04`-`$07` | the **music mix** of voices 5-7, 0-15, for `AUDV0`, in D3-D0 (section 5); D7-D4 draw-line's MOVAMT (section 14.4) |
| 1 | B+`$08`-`$0F` | fetcher n's **data**: the byte its counter points at |
| 2 | B+`$10`-`$17` | the data **AND the flag**: the byte inside the window, 0 outside |
| 3-6 | B+`$18`-`$37` | the masked data **arranged**: the nibbles swapped, the bits reversed, shifted right, shifted left (section 14.2) |
| 7 | B+`$38`-`$3F` | the **flag**, `$00` or `$FF` |

Every read of the read half, whatever its function, works on fetcher n (the low 3
bits of the address) in this order, as DPC does:

1. If the counter's low byte equals the top, the flag turns on (`$FF`); else if it
   equals the bottom, the flag turns off (`$00`).
2. The register is read (with the flag just set).
3. The counter steps down by one, unless the oscillator steps this fetcher (a music
   voice, section 5, or a sample stream, section 14.3).

So a masked read gives the data while the counter's low byte is at or below the top
and above the bottom, and 0 elsewhere: skipdraw done by the cart. Counting down, data
is read from higher addresses to lower, as a 2600 sprite table read with `dey`.

Note that the music mix registers belong to fetchers 4-7 too: a read of B+`$04`
compares and steps fetcher 4, a read of B+`$05` fetcher 5, and so on.

### Write registers (B + `$40`-`$7F`)

| Function | Address | Sets |
|---|---|---|
| 0 | B+`$40`-`$47` | the **top**; clears the flag |
| 1 | B+`$48`-`$4F` | the **bottom** |
| 2 | B+`$50`-`$57` | counter **bits 0-7** (a music voice loads its top instead, as DPC; fetcher 4 also loads draw-line's accumulator, section 14.4) |
| 3 | B+`$58`-`$5F` | counter **bits 8-15** |
| 4 | B+`$60`-`$67` | counter **bits 16-23**: D7 = 1 points into the cart RAM |
| 5 | B+`$68`-`$6F` | the **mode**: fetchers 5-7, D4 = 1 a music voice (DPC's bit), D5 = 1 the other clock (section 14.3); fetcher 4, D6 = 1 draw-line (14.4); fetchers 0-3 ignore it |
| 6 | B+`$70`-`$77` | reset the random number to 0, whatever is written (14.1) |
| 7 | B+`$78`-`$7F` | **MOVAMT**, D7-D4 (14.4) |

DPC set bits 8-10 and the music bit with one write; 3PD splits them because the
counter is wider.

**A read of the write half writes**, whatever is on the bus (no read/write line, as a
3E+ RAM write port). **A write to the read half** steps the fetcher as a read would
(the cart drives the bus with the register, so the value written is lost).

## 4. What a counter points at

| Counter | Points at |
|---|---|
| D23 = 0 | the **display area**: byte (counter AND `$7FFFFF`) modulo the area's size |
| D23 = 1 | the **cart RAM**: byte (counter AND `$7FFF`); RAM bank = that / 512 |

- **The display area is the image's last 256K**, or all of an image of 256K or less.
  Byte 0 of the area is the image's byte (image size - area size). This size is a
  draft value: measuring the Pico cart may change it. A Pico-based cart holds a small
  image in its RAM anyway; for a bigger one it copies the area there at power-up.
- **The cart RAM** is the same 32K that `$3E` puts in slots, so a fetcher can stream a
  buffer the game built with cart RAM writes.
- Stepping down wraps inside the space: D23 is kept, bits 0-22 go down by one.

## 5. Music (DPC's)

- Fetchers 5-7 become **voices** with mode D4 = 1 (and D5 = 0; section 14.3 has D5).
  A voice's counter low byte counts down from its top to 0 and around again,
  **20,000 times a second**, and its flag is on while the low byte is above the
  bottom. So a voice's pitch is 20000 / (top + 1) Hz, and its bottom sets the duty
  cycle. A voice with top 0 stays at 0.
- After each tick the voice's flag is set: off if the low byte is at or below the
  bottom, else on if it is at or below the top (else unchanged).
- The rate in the Stella fork is Stella's DPC rate: 20,000 ticks per 1,193,191.67 CPU
  cycles (the NTSC clock), for every TV format.
- A voice's counter isn't stepped by reads, and writing bits 0-7 loads the top.
- **A read compares a voice's counter too, as DPC:** a read of a voice's register
  (the mix at B+`$05`-`$07` for voices 5-7, its data, masked data or flag) first sets
  its flag on if the counter's low byte is the top, off if it is the bottom, and
  answers with it; the flag stays so until a tick or the next compare. So a silent
  voice (top 0, counter 0) read at its own mix register is on: a steady level in that
  read's mix, no tone.
- **The oscillator runs all the time**, as on the chip: a tick before a register write
  acts on the state before the write. An emulator that brings the voices up to date
  only when asked must do it before every register access, read or write. (Stella's
  DPC does it only at a mix read or a mode write, so it takes the ticks before a
  counter write as if they came after; 3PD's emulation doesn't take that shortcut.)
- The mix: the three flags of the fetchers in music mode (D4 = 1), bit 0 for voice 5,
  bit 1 for 6, bit 2 for 7, through DPC's table:

  | Voices on (7, 6, 5) | 000 | 001 | 010 | 011 | 100 | 101 | 110 | 111 |
  |---|---|---|---|---|---|---|---|---|
  | D3-D0 | `$0` | `$4` | `$5` | `$9` | `$6` | `$A` | `$B` | `$F` |

- A read of B+`$04`-`$07` (or `LDA #$04`-`#$07` with fast fetch) returns the mix,
  ready for `STA AUDV0`. The game writes it once a scanline, as Pitfall II does.

## 6. Fast fetch

- While D6 is on, the cart watches for `$A9` (`LDA #`): when a read of the slot's own
  bytes returns `$A9`, and the **next** cartridge read is at that address + 1 and the
  byte there is `$04`-`$17`, the cart returns **read register n** instead of n (the
  music mix, a fetcher's data, its masked data), with all a read's effects. Any other
  byte loads as itself.
- In the Stella fork: a read of the block's registers never arms fast fetch; an
  operand that falls inside the block isn't taken; a write to cartridge space, or
  turning D6 off, cancels it. Reads outside cartridge space (TIA, RIOT) in between
  don't.
- 2 cycles a fetcher read, against 4 for `LDA` of the block's address.
- `LDA #$00`-`#$03` and `LDA #$18`-`#$FF` always load themselves: only the useful
  registers are taken, so the common constants 0-3 stay free. `LDX #` and `LDY #` are
  never touched (fewer checks for the cart).
- **Cost to the game:** while fast fetch is on, `LDA #4` to `LDA #23` don't load
  constants. Use `LDX #`/`LDY #`, or turn fast fetch off around that code.
- The cart can't tell an opcode from data: a data read of `$A9` followed by a read of
  the next address triggers it too. In 6507 code that only happens with an `LDA #`
  (CDFJ+ has the same rule).

## 7. Power-up

- `$2E` = 0: the block and fast fetch off. Every counter, top, bottom and flag 0, no
  music voices, every clock as D5 = 0. The random number 0; draw-line off, its
  accumulator, carry and MOVAMT 0 (section 14).
- The rest is 3PO's: the outer 0, slot 3 ROM bank 0.
- **State to save** (for save states and rewind): 3PO's, then `$2E`, the 8 counters,
  tops, bottoms and flags, each voice's D4 and D5, the random number, draw-line's
  enable, accumulator, carry and MOVAMT, a pending fast fetch and its address, and the
  oscillator's timing.

## 8. The image

- 3PO's: a whole number of 1K banks, 1K to 16MB, bank 0 first.
- **Detection** in the Stella fork: the extension `.3pd`; `-bs 3PD` for a file whose
  extension Stella doesn't know; or autodetection by the four bytes `3PD1` anywhere
  in the image (checked before 3PO's `C3PO` and 3E+'s `TJ3E`, so an image may carry
  all three).

## 9. Game-code rules

3PO's rules (`3po.md` section 6), and:

1. **Never read `$2E`**, never index into it. A zero-page clear loop (`sta $00,x`)
   writes 0 there, which is the power-up state: harmless.
2. **Never read the block's write half** (it writes junk into a register), and keep
   indexed reads of the read half from crossing a page (the dummy read steps a
   counter).
3. **Leave the first 128 bytes** of every bank that may sit in the block's slot
   unused.
4. While fast fetch is on, load constants 4-23 with `LDX #`/`LDY #`.
5. **Read the mix at B+`$05`-`$07` while fetcher 4 is in use**: a read of B+`$04` is
   fetcher 4's read and steps its counter; a voice's read doesn't.
6. **Set a sample stream's counter before its mode** (section 14.3): write bits 0-23
   while the mode isn't `$20`, then `$20`. On a microcontroller cart a second core may
   step a running stream's whole counter, so a counter write while it runs can lose a
   step or the write.

## 10. Status (2026-11-01)

Emulated only, no carts yet: 3PD runs in the Newbroo Games Stella 7.0c fork
(`src/emucore/Cart3PD.cxx`, type `3PD`, extension `.3pd`, key `3PD1`) and in a 6507
simulator.

## 11. Results (2026-11-01)

A 512K test ROM covering sections 2 to 14 passes in Stella and in the simulator,
and the 3PO and 3E+ test ROMs, forced to 3PD, pass unchanged.

## 12. The Pico, emulated (2026-11-01)

A bus loop for a Pico-based cart at 150 MHz meets the 6507's timing with time to
spare when emulated on a PC; real hardware has yet to confirm it.

## 13. The demo (2026-11-01)

An 8K demo (a scrolling playfield picture, a ball and a three-voice song) runs the
same in Stella and in the simulator.

## 14. More of DPC (2026-11-01)

The rest of DPC: the random number, the bit-arranged reads, the clock select bit and
draw-line. Sources: Activision's patent (US 4,644,495, "Video memory system") and the
Kevin Horton's widely used notes on 2600 bank switch schemes; where they differ it says
so. Every part fills a register that sections 3 and 5 would otherwise leave reserved,
so **3E+ and 3PO ROMs run unchanged**.

### 14.1 Random numbers

| Address | |
|---|---|
| B+`$00`-`$03` (read) | the random number, then one step |
| B+`$70`-`$77` (write) | reset it to 0, whatever is written |

- DPC's 8-bit shift register: bits 3, 4, 5 and 7 XORed, inverted, shifted in at bit
  0: `R = ((R << 1) | (~((R >> 7) ^ (R >> 5) ^ (R >> 4) ^ (R >> 3)) & 1)) & $FF`.
  255 states; `$FF` is the stuck one, never reached from 0. 0 at power-up.
- **What steps it: each read of B+`$00`-`$03`** (Kevin Horton's notes' way). A read
  returns the current value, then steps it, so the first read after power-up or a
  reset returns 0. The patent steps it on every cart access (each 6507 read or write
  of `$1000`-`$1FFF`, code fetches included); Stella's DPC steps it at each access of
  the DPC block, a third way. Stepping on its own reads gives a game the same sequence
  on every emulator and costs a microcontroller cart nothing outside the read.
- The read also updates fetcher n's flag and steps its counter, as every read of the
  read half (section 3; DPC does the same).
- Not in fast fetch: `LDA #0`-`#3` stay constants (section 6).

### 14.2 Bit-arranged reads

Functions 3-6. For the byte `ABCDEFGH` (bit 7 to bit 0), each masked as function 2
(0 outside the window) and with every read's effects:

| Function | Address | Returns | Use |
|---|---|---|---|
| 3 | B+`$18`-`$1F` | `EFGHABCD`, the nibbles swapped | PF0 from a byte's low half |
| 4 | B+`$20`-`$27` | `HGFEDCBA`, the bits reversed | PF1 and PF2 from the same data (they run in opposite directions) |
| 5 | B+`$28`-`$2F` | `0ABCDEFG`, shifted right | a sprite one pixel over |
| 6 | B+`$30`-`$37` | `BCDEFGH0`, shifted left | |

- **Shifted, not rotated:** the patent and Kevin Horton's notes both say shifted, a 0 in.
  Stella's DPC comments and table say "rotated", but its DPC returns 0 for all four.
- **Not in fast fetch:** `LDA #$18`-`#$37` stay constants, so `LDA #24` to `#55` keep
  working with fast fetch on; these reads cost 4 cycles (`LDA` of the address).

### 14.3 The clock select bit

The mode register (B+`$68`-`$6F`, fetchers 5-7) gets DPC's second bit:

| D5 | D4 | Fetcher 5-7 is | Its counter steps |
|---|---|---|---|
| 0 | 0 | a fetcher | at each read of it |
| 0 | 1 | a music voice (`$10`) | at the 20 kHz oscillator; low byte top to 0 and around |
| 1 | 1 | a music voice (`$30`) | at each read of it (any function, the mix included), low byte top to 0 and around |
| 1 | 0 | a fetcher, a **sample stream** (`$20`) | at the 20 kHz oscillator, all 23 bits, as a read would step it |

- **D5 = 1 swaps the clock.** DPC's bit means "the oscillator" (Pitfall II writes
  `$30`); 3PD's `$10` already means a voice on the oscillator, so D5 here means "the
  other clock" and every ROM writing `$10` keeps working. Porting DPC code: write
  `$10` where Pitfall II writes `$30`.
- **The sample stream (`$20`):** the counter walks a table at 20,000 bytes a second
  whatever the 6507 does, and a read returns the byte for this moment without stepping
  it. Digitized sound at a fixed rate with no cycle counting in the kernel, read once a
  line into `AUDV0` (at 15.7 kHz lines the game hears about 4 of every 5 samples). Its
  flag is compared at reads only, as section 3's; it is not in the music mix.
- **A read-stepped voice (`$30`)** gives a tone the kernel's timing decides: one read a
  line, top 49, is 15,700 / 50 = 314 Hz (a cycle is top + 1 steps, as on the
  oscillator). Its flag is set by the compare at each read (section 3), and it is in
  the music mix. Pitfall II doesn't use it; it's DPC's, so 3PD has it.

### 14.4 Draw-line

DPC's line drawer, unused by Pitfall II and known from the patent: an 8-bit
accumulator on fetcher 4, adding fetcher 4's top at each read of the mix, and a 4-bit
move value let out on the read whenever the add carries. A missile or ball moved by
`HMxx` from it draws a straight line of any slope, one `LDA`/`STA` a line, with no
math in the kernel.

| Address | |
|---|---|
| B+`$6C` (write, fetcher 4's mode) | D6 = 1 draw-line on (the patent puts the enable in fetcher 4's counter-high register without naming the bit; 3PD's is full, so it goes in the mode register) |
| B+`$54` (write, fetcher 4's counter bits 0-7) | also loads the accumulator |
| B+`$44` (write, fetcher 4's top) | the slope: the add carries top / 256 of the reads |
| B+`$78`-`$7F` (write) | **MOVAMT**: D7-D4, the move value (D3-D0 ignored). DPC's address for it, `$60`-`$67`, is 3PD's counter bits 16-23 |
| B+`$04`, `$05` (read) | the accumulator += top; returns MOVAMT in D7-D4 if the add carried, else 0, and the music mix in D3-D0 |
| B+`$06`, `$07` (read) | no add; D7-D4 as the last add left them (the patent's carry comes from the adder, which still holds the last add) |

- **One read for both:** `LDA` it, `STA AUDV0` (D3-D0) and `STA HMM0` (D7-D4: the
  HMOVE nibble), then `HMOVE` at the line's start. The line costs the kernel 3 more
  cycles (the extra `STA`) on top of the music read it already does.
- **Draw-line off: D7-D4 read 0**, so the mix reads give exactly the plain mix.
- Fetcher 4 is still a fetcher: its counter, flag and reads work as before (the
  patent's fetcher 4 also draws lines in addition to its normal work). Its top is
  shared with the window, so a game drawing lines usually gives fetcher 4 no other job.
- `LDA #$04`/`#$05` with fast fetch do the add; `#$06`/`#$07` don't.

### 14.5 What changes where

Against 3PD's first version (sections 1-9 without section 14):

- **Section 3's tables:** read functions 0 (the random number, at B+`$00`-`$03`) and
  3-6 filled; write functions 6 (B+`$70`-`$77`, reset) and 7 (B+`$78`-`$7F`,
  MOVAMT); the mode register's D5 for fetchers 5-7, and D6 for fetcher 4.
- **Section 7:** the random number, accumulator and MOVAMT 0 at power-up, draw-line
  off.
- The 3PD ROMs written for the first version (the test ROM's earlier steps and the
  demos) run unchanged. In the first version B+`$00`-`$03` always read 0; a ROM that
  relied on that would now see the random sequence.
