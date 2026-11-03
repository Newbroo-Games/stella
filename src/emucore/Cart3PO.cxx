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
#include "Cart3PO.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Cartridge3PO::Cartridge3PO(const ByteBuffer& image, size_t size,
                             string_view md5, const Settings& settings)
  : Cartridge3EPlus(image, size, md5, settings)
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge3PO::reset()
{
  // The board must power up with the outer register at 0, so a 3E+ ROM
  // finds its banks where 3E+ puts them
  myOuter = 0;

  Cartridge3EPlus::reset();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge3PO::checkSwitchBank(uInt16 address, uInt8 value)
{
  // The board decodes all 13 address lines: only exactly $002F, $003E and
  // $003F switch
  switch(address & 0x1FFF)
  {
    case 0x002F:
      // The outer bank number, for the next writes to $3F; no slot changes
      if(!hotspotsLocked())
        myOuter = value;
      return true;

    case 0x003F:
      // ROM bank outer * 64 + D5-D0 into slot D7-D6; past the end wraps
      bank(((myOuter << 6) | (value & 0b111111)) % romBankCount(), value >> 6);
      return true;

    case 0x003E:
      // RAM bank D5-D0 into slot D7-D6, as in 3E+ (no outer bank)
      bank((value & 0b111111) + romBankCount(), value >> 6);
      return true;

    default:
      return false;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 Cartridge3PO::peek(uInt16 address)
{
  if(!(address & 0x1000))  // TIA access
  {
    // No read/write line: a read of $2F, $3E or $3F switches to what's on
    // the bus
    const uInt8 value = mySystem->tia().peek(address);

    checkSwitchBank(address, value);
    return value;
  }

  return CartridgeEnhanced::peek(address);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge3PO::poke(uInt16 address, uInt8 value)
{
  if(!(address & 0x1000))  // TIA access
  {
    checkSwitchBank(address, value);
    return mySystem->tia().poke(address, value);
  }

  // A write in the cart's own space never switches (Cart3E's poke would
  // take $103E/$103F for the switches)
  return CartridgeEnhanced::poke(address, value);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge3PO::save(Serializer& out) const
{
  if(!Cartridge3EPlus::save(out))
    return false;

  try
  {
    out.putByte(myOuter);
  }
  catch(...)
  {
    cerr << "ERROR: " << name() << "::save\n";
    return false;
  }

  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge3PO::load(Serializer& in)
{
  if(!Cartridge3EPlus::load(in))
    return false;

  try
  {
    myOuter = in.getByte();
  }
  catch(...)
  {
    cerr << "ERROR: " << name() << "::load\n";
    return false;
  }

  return true;
}
