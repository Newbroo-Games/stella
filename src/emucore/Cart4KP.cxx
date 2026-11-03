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

#include "System.hxx"
#include "TIA.hxx"
#include "Cart4KP.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Cartridge4KP::Cartridge4KP(const ByteBuffer& image, size_t size,
                           string_view md5, const Settings& settings)
  : Cartridge(settings, md5),
    mySize{BSPF::nextMultipleOf(size, BANK_SIZE)}
{
  // A whole number of 4K banks; a short last bank is padded with $FF
  myImage = make_unique<uInt8[]>(mySize);
  std::fill_n(myImage.get(), mySize, 0xFF);
  std::copy_n(image.get(), std::min(size, mySize), myImage.get());

  // Code-access tracking for the ROM and the RAM (the RAM after the ROM)
  createRomAccessArrays(mySize + RAM_SIZE);
  myRAM.fill(0xFF);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge4KP::reset()
{
  // The board powers up with random latches and random RAM; Stella's
  // developer settings decide whether to show that
  initializeRAM(myRAM.data(), myRAM.size());
  initializeStartBank(0);

  if(randomStartBank())
  {
    myPage[0] = static_cast<uInt8>(mySystem->randGenerator().next());
    myPage[1] = static_cast<uInt8>(mySystem->randGenerator().next());
  }
  else
    myPage.fill(0);

  mapWindow(0);
  mapWindow(1);
  bank(startBank());

  myBankChanged = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge4KP::install(System& system)
{
  mySystem = &system;

  // The switches ($3D-$3F) are in TIA address space, so claim it (as Cart3E
  // does); peek() and poke() pass every access on to the TIA
  const System::PageAccess access(this, System::PageAccessType::WRITE);
  for(uInt16 addr = 0x00; addr < 0x40; addr += System::PAGE_SIZE)
    mySystem->setPageAccess(addr, access);

  mapWindow(0);
  mapWindow(1);
  bank(startBank());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge4KP::mapPage(uInt16 addr, System::PageAccessType type,
                           uInt8* directData, uInt32 codeOffset)
{
  System::PageAccess access(this, type);

  // Only reads of the read ports and the ROM may skip peek(); every write,
  // and every read of a write port, goes through peek() and poke()
  if(type == System::PageAccessType::READ)
    access.directPeekBase = directData;
  access.romAccessBase = &myRomAccessBase[codeOffset];
  access.romPeekCounter = &myRomAccessCounter[codeOffset];
  access.romPokeCounter = &myRomAccessCounter[codeOffset + myAccessSize];
  mySystem->setPageAccess(addr, access);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge4KP::mapRom()
{
  const auto offset = static_cast<uInt32>(myCurrentBank * BANK_SIZE);

  for(uInt16 addr = 0x1200; addr < 0x2000; addr += System::PAGE_SIZE)
    mapPage(addr, System::PageAccessType::READ,
            &myImage[offset + (addr & 0x0FFF)], offset + (addr & 0x0FFF));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge4KP::mapWindow(uInt8 w)
{
  const uInt32 ram = myPage[w] * PAGE_BYTES;

  // A window is two of Stella's 64-byte pages in each port
  for(uInt16 i = 0; i < PAGE_BYTES; i += System::PAGE_SIZE)
  {
    const auto off = static_cast<uInt16>(w * PAGE_BYTES + i);

    mapPage(0x1000 + off, System::PageAccessType::WRITE,
            nullptr, static_cast<uInt32>(mySize) + ram + i);
    mapPage(0x1100 + off, System::PageAccessType::READ,
            &myRAM[ram + i], static_cast<uInt32>(mySize) + ram + i);
  }
  myBankChanged = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge4KP::setPage(uInt8 w, uInt8 page)
{
  if(hotspotsLocked()) return;

  myPage[w] = page;
  mapWindow(w);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge4KP::checkSwitch(uInt16 address, uInt8 value)
{
  // The board decodes all 13 address lines: only exactly $003D-$003F
  switch(address & 0x1FFF)
  {
    case 0x003F: bank(value);        break;
    case 0x003E: setPage(0, value);  break;
    case 0x003D: setPage(1, value);  break;
    default: break;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 Cartridge4KP::peek(uInt16 address)
{
  const uInt16 peekAddress = address;
  address &= 0x1FFF;

  if(!(address & 0x1000))  // TIA access
  {
    // No read/write line: a read of $3D-$3F switches to what's on the bus
    const uInt8 value = mySystem->tia().peek(address);
    checkSwitch(address, value);
    return value;
  }

  address &= 0x0FFF;
  if(address < 0x0100)
  {
    // Reading a write port writes the bus into the RAM byte, as on a Superchip
    return peekRAM(myRAM[ramIndex(address)], peekAddress);
  }
  if(address < 0x0200)
    return myRAM[ramIndex(address)];

  return myImage[myCurrentBank * BANK_SIZE + address];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge4KP::poke(uInt16 address, uInt8 value)
{
  const uInt16 pokeAddress = address;
  address &= 0x1FFF;

  if(!(address & 0x1000))  // TIA access
  {
    checkSwitch(address, value);
    return mySystem->tia().poke(address, value);
  }

  address &= 0x0FFF;
  if(address < 0x0100)
  {
    pokeRAM(myRAM[ramIndex(address)], pokeAddress, value);
    return true;
  }
  if(address < 0x0200)
  {
    // Writing a read port: on the board the SRAM and the 6507 both drive
    // the bus; ignore it, but trigger a break if that option is on
    uInt8 dummy{0};

    pokeRAM(dummy, pokeAddress, value);
    myRamWriteAccess = pokeAddress;
  }
  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge4KP::bank(uInt16 bank, uInt16)
{
  if(hotspotsLocked()) return false;

  myCurrentBank = bank % romBankCount();
  mapRom();

  return myBankChanged = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt16 Cartridge4KP::getBank(uInt16) const
{
  return myCurrentBank;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt16 Cartridge4KP::romBankCount() const
{
  return static_cast<uInt16>(mySize / BANK_SIZE);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge4KP::patch(uInt16 address, uInt8 value)
{
  address &= 0x0FFF;

  // The patch command ignores the cart's restrictions: either port of a
  // window changes its RAM byte
  if(address < 0x0200)
    myRAM[ramIndex(address)] = value;
  else
    myImage[myCurrentBank * BANK_SIZE + address] = value;

  return myBankChanged = true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const ByteBuffer& Cartridge4KP::getImage(size_t& size) const
{
  size = mySize;
  return myImage;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge4KP::save(Serializer& out) const
{
  try
  {
    out.putShort(myCurrentBank);
    out.putByteArray(myPage.data(), myPage.size());
    out.putByteArray(myRAM.data(), myRAM.size());
  }
  catch(...)
  {
    cerr << "ERROR: " << name() << "::save\n";
    return false;
  }

  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge4KP::load(Serializer& in)
{
  try
  {
    myCurrentBank = in.getShort();
    in.getByteArray(myPage.data(), myPage.size());
    in.getByteArray(myRAM.data(), myRAM.size());
  }
  catch(...)
  {
    cerr << "ERROR: " << name() << "::load\n";
    return false;
  }

  mapWindow(0);
  mapWindow(1);
  bank(myCurrentBank);

  return true;
}
