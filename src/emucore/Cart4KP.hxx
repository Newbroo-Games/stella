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

#ifndef CARTRIDGE_4KP_HXX
#define CARTRIDGE_4KP_HXX

#include "System.hxx"
#include "bspf.hxx"
#include "Cart.hxx"

/**
  4KP: 4K ROM banks and paged RAM (Newbroo Games' 512K or 1MB + 32K RAM
  board, docs/4kp.md).

  The whole 4K window is one switched ROM bank, up to 256 banks of 4K.  The
  32K of RAM is 256 pages of 128 bytes, seen through two windows that sit
  over the bottom 512 bytes of every bank:

    $1000-$107F  window A, write port
    $1080-$10FF  window B, write port
    $1100-$117F  window A, read port
    $1180-$11FF  window B, read port
    $1200-$1FFF  ROM: bytes $200-$FFF of the current bank

  The switches are writes to TIA addresses, decoded exactly, as in 3E:

    $3F  ROM bank (0-255, mod the bank count: a 512K image ignores bit 7)
    $3E  window A's RAM page (0-255)
    $3D  window B's RAM page (0-255)

  The board has no read/write line, so a read of $3D-$3F switches too, to
  whatever is on the bus; this class does the same with the TIA's value.

  @author  Newbroo Games
*/
class Cartridge4KP : public Cartridge
{
  public:
    /**
      Create a new cartridge using the specified image

      @param image     Pointer to the ROM image
      @param size      The size of the ROM image
      @param md5       The md5sum of the ROM image
      @param settings  A reference to the various settings (read-only)
    */
    Cartridge4KP(const ByteBuffer& image, size_t size, string_view md5,
                 const Settings& settings);
    ~Cartridge4KP() override = default;

  public:
    void reset() override;
    void install(System& system) override;

    bool bank(uInt16 bank, uInt16 segment = 0) override;
    uInt16 getBank(uInt16 address = 0) const override;
    uInt16 romBankCount() const override;

    bool patch(uInt16 address, uInt8 value) override;
    const ByteBuffer& getImage(size_t& size) const override;

    bool save(Serializer& out) const override;
    bool load(Serializer& in) override;

    string name() const override { return "Cartridge4KP"; }

    uInt32 internalRamSize() const override { return RAM_SIZE; }
    uInt8 internalRamGetValue(uInt16 addr) const override {
      return myRAM[addr % RAM_SIZE];
    }

  public:
    uInt8 peek(uInt16 address) override;
    bool poke(uInt16 address, uInt8 value) override;

  private:
    // A write (or a read) of $3D-$3F
    void checkSwitch(uInt16 address, uInt8 value);

    // Show RAM page 'page' in window 'w' (0 = A, 1 = B)
    void setPage(uInt8 w, uInt8 page);

    // Map the pages of the current ROM bank, or of one RAM window
    void mapRom();
    void mapWindow(uInt8 w);
    void mapPage(uInt16 addr, System::PageAccessType type,
                 uInt8* directData, uInt32 codeOffset);

    // The RAM byte that a window address ($1000-$11FF) reaches
    uInt32 ramIndex(uInt16 address) const {
      return myPage[(address >> 7) & 1] * PAGE_BYTES + (address & (PAGE_BYTES - 1));
    }

  private:
    static constexpr size_t BANK_SIZE = 4_KB;
    static constexpr uInt32 RAM_SIZE = 32 * 1024;
    static constexpr uInt32 PAGE_BYTES = 128;

    // The ROM image, a whole number of 4K banks
    ByteBuffer myImage;
    size_t mySize{0};

    // The 32K of RAM
    std::array<uInt8, RAM_SIZE> myRAM{};

    // The ROM bank, and the RAM page in each window
    uInt16 myCurrentBank{0};
    std::array<uInt8, 2> myPage{};

  private:
    // Following constructors and assignment operators not supported
    Cartridge4KP() = delete;
    Cartridge4KP(const Cartridge4KP&) = delete;
    Cartridge4KP(Cartridge4KP&&) = delete;
    Cartridge4KP& operator=(const Cartridge4KP&) = delete;
    Cartridge4KP& operator=(Cartridge4KP&&) = delete;
};

#endif
