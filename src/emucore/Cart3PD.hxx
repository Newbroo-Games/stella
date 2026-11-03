//============================================================================
//
//   SSSS    tt          lll  lll
//  SS  SS   tt           ll   ll
//  SS     tttttt  eeee   ll   ll   aaaa
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2024 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//============================================================================

#ifndef CARTRIDGE_3PD_HXX
#define CARTRIDGE_3PD_HXX

class System;

#include "bspf.hxx"
#include "Cart3PO.hxx"

/**
  3PD: 3PO plus DPC-style data fetchers (Newbroo Games,
  docs/3pd.md).

  Everything 3PO does ($2F, $3E, $3F), plus:

    - a control register at $2E: D7 the fetcher block on, D6 fast fetch on,
      D1-D0 the slot the block covers.  0 at power-up, so every 3E+ and 3PO
      game runs unchanged.

    - the fetcher block, the first 128 bytes of the chosen slot while it is
      on: reads at +$00-$3F (function * 8 + fetcher), writes at +$40-$7F.
      Eight fetchers, each a 24-bit counter that counts down, with DPC's
      top/bottom window and flag, and DPC's three music voices on fetchers
      5-7.  A counter points into the display area (the image's last 256K,
      or all of a smaller image) or, with bit 23 set, into the 32K cart RAM.

    - fast fetch: while on, an LDA # whose operand is $04-$17 returns the
      block's read register of that number instead.

    - more of DPC (docs/3pd.md section 14): the random number at +$00-$03 (stepped
      by its reads, reset by a write at +$70-$77); reads 3-6, the masked data
      with its nibbles swapped, reversed, shifted right, shifted left; the
      mode's D5 swaps a fetcher's clock (a voice stepped by its reads, a
      fetcher stepped by the oscillator); draw-line on fetcher 4 (its mode's
      D6; MOVAMT at +$78-$7F).

  No read/write line, as in 3PO: a read of $2E, or of the block's write
  half, acts as a write of whatever is on the bus.

  @author  Newbroo Games
*/
class Cartridge3PD : public Cartridge3PO
{
  public:
    Cartridge3PD(const ByteBuffer& image, size_t size, string_view md5,
                 const Settings& settings);
    ~Cartridge3PD() override = default;

  public:
    void install(System& system) override;
    void reset() override;

    bool save(Serializer& out) const override;
    bool load(Serializer& in) override;

    string name() const override { return "Cartridge3PD"; }

    uInt8 peek(uInt16 address) override;
    bool poke(uInt16 address, uInt8 value) override;

    bool bank(uInt16 bank, uInt16 segment) override;

    /** The control register ($2E) */
    uInt8 control() const { return myControl; }

  protected:
    bool checkSwitchBank(uInt16 address, uInt8 value) override;

  private:
    // Is this cart-space address in the fetcher block (while it's on)?
    bool inBlock(uInt16 address) const {
      return (myControl & BLOCK_ON) &&
             ((address >> 10) & 3) == (myControl & 3) &&
             (address & 0x3FF) < 0x80;
    }

    // A read of the block's read register r (0-$3F)
    uInt8 readRegister(uInt8 r);
    // A write of the block's write register r (0-$3F, i.e. offset - $40)
    void writeRegister(uInt8 r, uInt8 value);

    // What the slot itself shows at a cart address (its ROM or RAM bank)
    uInt8 slotPeek(uInt16 address);

    // The byte a counter points at
    uInt8 fetch(uInt32 counter) const;

    // DPC's music voices: bring fetchers 5-7 up to date
    void updateMusic();

    // Is fetcher x stepped by the oscillator (a voice, unless D5 swaps it to
    // its reads; a fetcher only when D5 swaps it to the oscillator)?
    bool oscStepped(uInt32 x) const {
      return x >= 5 && (myMusicMode[x - 5] != mySwapClock[x - 5]);
    }

    // DPC's random number: one step of its 8-bit shift register
    void stepRandom();

    // Every cart page through peek()/poke(): the block and fast fetch can
    // change what any address returns
    void noDirectPeeks();

  private:
    static constexpr uInt8 BLOCK_ON = 0x80;
    static constexpr uInt8 FAST_ON  = 0x40;
    static constexpr uInt32 RAM_BIT = 0x800000;
    static constexpr size_t AREA_MAX = 256_KB;
    static constexpr double OSC_HZ = 20000.0;  // the music oscillator
    static constexpr double CLOCK_HZ = 1193191.66666667;  // the 6507 (NTSC)

    uInt8 myControl{0};

    std::array<uInt32, 8> myCounters{};
    std::array<uInt8, 8> myTops{};
    std::array<uInt8, 8> myBottoms{};
    std::array<uInt8, 8> myFlags{};
    std::array<bool, 3> myMusicMode{};
    std::array<bool, 3> mySwapClock{};  // the mode's D5 (section 14.3)

    // DPC's random number (section 14.1)
    uInt8 myRandom{0};

    // Draw-line (section 14.4): fetcher 4's mode D6, the accumulator, the
    // last add's carry, MOVAMT (D7-D4)
    bool myDrawLine{false};
    uInt8 myLineAcc{0};
    bool myLineCarry{false};
    uInt8 myMovAmt{0};

    // The display area: the image's last AREA_MAX bytes (all of a smaller one)
    size_t myAreaStart{0};
    size_t myAreaSize{0};

    // Fast fetch: the address of an LDA #'s operand, armed by the $A9
    uInt16 myOperandAddress{0};
    bool myArmed{false};

    // Music timing
    uInt64 myMusicCycles{0};
    double myFractionalClocks{0.0};

  private:
    Cartridge3PD() = delete;
    Cartridge3PD(const Cartridge3PD&) = delete;
    Cartridge3PD(Cartridge3PD&&) = delete;
    Cartridge3PD& operator=(const Cartridge3PD&) = delete;
    Cartridge3PD& operator=(Cartridge3PD&&) = delete;
};

#endif
