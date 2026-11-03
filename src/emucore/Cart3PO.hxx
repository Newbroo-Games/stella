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

#ifndef CARTRIDGE_3PO_HXX
#define CARTRIDGE_3PO_HXX

class System;

#include "bspf.hxx"
#include "Cart3EPlus.hxx"

#ifdef DEBUGGER_SUPPORT
  #include "Cart3POWidget.hxx"
#endif

/**
  3PO: 3E+ with an outer bank register (Newbroo Games,
  docs/3po.md).  "3E Plus, Outer bank".

  Everything 3E+ does, plus one register: a write to $2F stores the outer
  bank number (0-255), and every later write to $3F puts ROM bank
  outer * 64 + D5-D0 into slot D7-D6.  Each slot keeps its whole bank
  number, so a later write to $2F moves nothing.  1K ROM banks, up to 16384
  of them (16MB); a bank number at or past the end wraps.  RAM ($3E) is 3E+'s
  64 banks of 512 bytes, and doesn't use the outer register.

  The outer register starts at 0, so a 3E+ ROM runs unchanged.

  Unlike Stella's 3E+, the switches are decoded as the board would:
    - exactly $002F, $003E and $003F (a write to $103F doesn't switch);
    - the board has no read/write line, so a read there switches too, to
      whatever is on the bus (this class uses the TIA's value).

  @author  Newbroo Games
*/
class Cartridge3PO : public Cartridge3EPlus
{
  friend class Cartridge3POWidget;

  public:
    /**
      Create a new cartridge using the specified image and size

      @param image     Pointer to the ROM image
      @param size      The size of the ROM image
      @param md5       The md5sum of the ROM image
      @param settings  A reference to the various settings (read-only)
    */
    Cartridge3PO(const ByteBuffer& image, size_t size, string_view md5,
                  const Settings& settings);
    ~Cartridge3PO() override = default;

  public:
    /** Reset device to its power-on state */
    void reset() override;

    bool save(Serializer& out) const override;
    bool load(Serializer& in) override;

    /**
      Get a descriptor for the device name (used in error checking).

      @return The name of the object
    */
    string name() const override { return "Cartridge3PO"; }

    uInt8 peek(uInt16 address) override;
    bool poke(uInt16 address, uInt8 value) override;

    /** The outer bank register (the last byte written to $2F) */
    uInt8 outer() const { return myOuter; }

  #ifdef DEBUGGER_SUPPORT
    /**
      Get debugger widget responsible for accessing the inner workings
      of the cart.
    */
    CartDebugWidget* debugWidget(GuiObject* boss, const GUI::Font& lfont,
        const GUI::Font& nfont, int x, int y, int w, int h) override
    {
      return new Cartridge3POWidget(boss, lfont, nfont, x, y, w, h, *this);
    }
  #endif

  protected:
    bool checkSwitchBank(uInt16 address, uInt8 value) override;

  private:
    // The upper bits of the ROM bank number: bank = outer * 64 + D5-D0
    uInt8 myOuter{0};

  private:
    // Following constructors and assignment operators not supported
    Cartridge3PO() = delete;
    Cartridge3PO(const Cartridge3PO&) = delete;
    Cartridge3PO(Cartridge3PO&&) = delete;
    Cartridge3PO& operator=(const Cartridge3PO&) = delete;
    Cartridge3PO& operator=(Cartridge3PO&&) = delete;
};

#endif
