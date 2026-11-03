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
#include "Cart3PD.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Cartridge3PD::Cartridge3PD(const ByteBuffer& image, size_t size,
                           string_view md5, const Settings& settings)
  : Cartridge3PO(image, size, md5, settings)
{
  // Every cart read goes through peek(): the block and fast fetch can
  // change what any cart address returns
  myDirectPeek = false;

  // The display area: the image's last 256K, or all of a smaller image
  myAreaSize = std::min(mySize, AREA_MAX);
  myAreaStart = mySize - myAreaSize;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge3PD::install(System& system)
{
  Cartridge3PO::install(system);
  noDirectPeeks();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge3PD::reset()
{
  // Power-up: the block and fast fetch off, every fetcher at 0
  myControl = 0;
  myCounters.fill(0);
  myTops.fill(0);
  myBottoms.fill(0);
  myFlags.fill(0);
  myMusicMode.fill(false);
  mySwapClock.fill(false);
  myRandom = 0;
  myDrawLine = false;
  myLineAcc = 0;
  myLineCarry = false;
  myMovAmt = 0;
  myArmed = false;
  myOperandAddress = 0;
  myMusicCycles = 0;
  myFractionalClocks = 0.0;

  Cartridge3PO::reset();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge3PD::bank(uInt16 bank, uInt16 segment)
{
  const bool changed = Cartridge3PO::bank(bank, segment);
  noDirectPeeks();
  return changed;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge3PD::noDirectPeeks()
{
  if(mySystem == nullptr)
    return;

  for(uInt16 addr = 0x1000; addr < 0x2000; addr += System::PAGE_SIZE)
  {
    System::PageAccess access = mySystem->getPageAccess(addr);
    if(access.device == this &&
       (access.directPeekBase != nullptr || access.directPokeBase != nullptr))
    {
      access.directPeekBase = nullptr;
      access.directPokeBase = nullptr;
      mySystem->setPageAccess(addr, access);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge3PD::checkSwitchBank(uInt16 address, uInt8 value)
{
  if((address & 0x1FFF) == 0x002E)
  {
    // The control register: D7 block on, D6 fast fetch on, D1-D0 the slot
    if(!hotspotsLocked())
    {
      myControl = value;
      if(!(myControl & FAST_ON))
        myArmed = false;
    }
    return true;
  }
  return Cartridge3PO::checkSwitchBank(address, value);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 Cartridge3PD::slotPeek(uInt16 address)
{
  // Read here, not by CartridgeEnhanced::peek, which counts on the direct
  // peeks (now off): it takes any ROM address from $x200 for a Superchip-
  // style RAM port, and any RAM read for a read of the write port
  const uInt32 segOffset =
    myCurrentSegOffset[((address & 0x0FFF) >> myBankShift) % myBankSegs];

  if(isRamBank(address))
  {
    // The write port (the slot's last 512 bytes): the read writes junk, as
    // on the board
    if(address & (myBankSize >> 1))
      return CartridgeEnhanced::peek(address);

    // The read port
    return myRAM[((segOffset - mySize) >> (myBankShift - myRamBankShift))
                 + (address & myRamMask)];
  }

  return myImage[segOffset + (address & myBankMask)];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 Cartridge3PD::fetch(uInt32 counter) const
{
  if(counter & RAM_BIT)
    return myRAM[counter & 0x7FFF];

  return myImage[myAreaStart + ((counter & 0x7FFFFF) % myAreaSize)];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge3PD::updateMusic()
{
  // DPC's music voices (Stella's CartDPC), with a fixed 20 kHz oscillator
  const auto cycles = static_cast<uInt32>(mySystem->cycles() - myMusicCycles);
  myMusicCycles = mySystem->cycles();

  const double clocks = ((OSC_HZ * cycles) / CLOCK_HZ) + myFractionalClocks;
  const auto wholeClocks = static_cast<uInt32>(clocks);
  myFractionalClocks = clocks - static_cast<double>(wholeClocks);

  if(wholeClocks == 0)
    return;

  for(uInt32 x = 5; x <= 7; ++x)
  {
    if(!oscStepped(x))
      continue;

    if(!myMusicMode[x - 5])
    {
      // A sample stream (mode $20, section 14.3): all 23 bits step down, as a
      // read would step them; its flag is compared at reads only
      const uInt32 c = myCounters[x];
      myCounters[x] = (c & RAM_BIT) | ((c - wholeClocks) & (RAM_BIT - 1));
      continue;
    }

    const Int32 top = myTops[x] + 1;
    auto newLow = static_cast<Int32>(myCounters[x] & 0xFF);

    if(myTops[x] != 0)
    {
      newLow -= static_cast<Int32>(wholeClocks % top);
      if(newLow < 0)
        newLow += top;
    }
    else
      newLow = 0;

    if(newLow <= myBottoms[x])
      myFlags[x] = 0x00;
    else if(newLow <= myTops[x])
      myFlags[x] = 0xFF;

    myCounters[x] = (myCounters[x] & ~0xFFU) | static_cast<uInt32>(newLow);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge3PD::stepRandom()
{
  // DPC's 8-bit shift register (the patent; Horton's notes): bits 3, 4, 5 and
  // 7 XORed, inverted, shifted in at bit 0.  255 states from 0; $FF is the
  // stuck one
  const uInt8 r = myRandom;
  const uInt8 bit = ~((r >> 7) ^ (r >> 5) ^ (r >> 4) ^ (r >> 3)) & 1;
  myRandom = static_cast<uInt8>((r << 1) | bit);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 Cartridge3PD::readRegister(uInt8 r)
{
  const uInt32 i = r & 0x07;
  const uInt32 function = (r >> 3) & 0x07;
  uInt8 result = 0;

  // The oscillator runs all the time on the chip (and on the Pico's second
  // core): bring it up to date before every register access, so a write of
  // a voice's counter (it loads the top) between two updates doesn't take the
  // ticks before it as if they came after (Stella's DPC has that shortcut)
  updateMusic();

  // The window: the flag turns on when the counter's low byte reaches the
  // top, and off when it reaches the bottom (DPC), a music voice's too
  // (docs/3pd.md section 5: a silent voice, top 0 and counter 0, reads as on)
  if((myCounters[i] & 0xFF) == myTops[i])
    myFlags[i] = 0xFF;
  else if((myCounters[i] & 0xFF) == myBottoms[i])
    myFlags[i] = 0x00;

  switch(function)
  {
    case 0:
      // Fetchers 0-3: the random number, stepped by its reads (section 14.1,
      // Horton's; the patent steps it at every cart access)
      if(i < 4)
      {
        result = myRandom;
        stepRandom();
      }
      // Fetchers 4-7: the music mix (DPC)
      else
      {
        static constexpr std::array<uInt8, 8> amplitudes = {
          0x00, 0x04, 0x05, 0x09, 0x06, 0x0A, 0x0B, 0x0F
        };
        uInt8 v = 0;
        if(myMusicMode[0] && myFlags[5]) v |= 0x01;
        if(myMusicMode[1] && myFlags[6]) v |= 0x02;
        if(myMusicMode[2] && myFlags[7]) v |= 0x04;
        result = amplitudes[v];

        // Draw-line (section 14.4): a read at +$04/$05 adds fetcher 4's top
        // to the accumulator; every mix read gives MOVAMT in D7-D4 if the last
        // add carried
        if(myDrawLine)
        {
          if(i < 6)
          {
            const uInt32 sum = uInt32{myLineAcc} + myTops[4];
            myLineAcc = static_cast<uInt8>(sum);
            myLineCarry = sum > 0xFF;
          }
          if(myLineCarry)
            result |= myMovAmt;
        }
      }
      break;

    case 1:  // the data
      result = fetch(myCounters[i]);
      break;

    case 2:  // the data AND the flag (the window)
      result = fetch(myCounters[i]) & myFlags[i];
      break;

    case 3:  // masked, the nibbles swapped (section 14.2)
    {
      const uInt8 v = fetch(myCounters[i]) & myFlags[i];
      result = static_cast<uInt8>((v << 4) | (v >> 4));
      break;
    }

    case 4:  // masked, the bits reversed
    {
      const uInt8 v = fetch(myCounters[i]) & myFlags[i];
      for(int b = 0; b < 8; ++b)
        if(v & (1 << b))
          result |= static_cast<uInt8>(0x80 >> b);
      break;
    }

    case 5:  // masked, shifted right (a 0 in, not rotated)
      result = (fetch(myCounters[i]) & myFlags[i]) >> 1;
      break;

    case 6:  // masked, shifted left
      result = static_cast<uInt8>((fetch(myCounters[i]) & myFlags[i]) << 1);
      break;

    case 7:  // the flag
      result = myFlags[i];
      break;

    default:
      break;
  }

  // A read steps the fetcher down, unless the oscillator steps it (a voice,
  // or a fetcher with D5); a voice stepped by its reads (mode $30) steps its
  // low byte from the top to 0 and around, as the oscillator would
  if(!oscStepped(i))
  {
    const uInt32 c = myCounters[i];
    if(i >= 5 && myMusicMode[i - 5])
    {
      const uInt32 low = c & 0xFF;
      myCounters[i] = (c & ~0xFFU) | (low == 0 ? myTops[i] : low - 1);
    }
    else
      myCounters[i] = (c & RAM_BIT) | ((c - 1) & (RAM_BIT - 1));
  }

  return result;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge3PD::writeRegister(uInt8 r, uInt8 value)
{
  const uInt32 i = r & 0x07;
  const uInt32 function = (r >> 3) & 0x07;

  updateMusic();  // (as readRegister)

  switch(function)
  {
    case 0:  // the top; clears the flag
      myTops[i] = value;
      myFlags[i] = 0x00;
      break;

    case 1:  // the bottom
      myBottoms[i] = value;
      break;

    case 2:  // counter bits 0-7 (a music voice loads its top instead)
      if(i >= 5 && myMusicMode[i - 5])
        myCounters[i] = (myCounters[i] & ~0xFFU) | myTops[i];
      else
        myCounters[i] = (myCounters[i] & ~0xFFU) | value;
      if(i == 4)
        myLineAcc = value;  // the patent's IL4: the accumulator loads too
      break;

    case 3:  // counter bits 8-15
      myCounters[i] = (myCounters[i] & ~0xFF00U) | (uInt32{value} << 8);
      break;

    case 4:  // counter bits 16-23 (D7: the cart RAM)
      myCounters[i] = (myCounters[i] & 0xFFFFU) | (uInt32{value} << 16);
      break;

    case 5:  // the mode: fetchers 5-7, D4 = music voice, D5 = the other
             // clock; fetcher 4, D6 = draw-line
      if(i >= 5)
      {
        myMusicMode[i - 5] = value & 0x10;
        mySwapClock[i - 5] = value & 0x20;
      }
      else if(i == 4)
        myDrawLine = value & 0x40;
      break;

    case 6:  // reset the random number
      myRandom = 0;
      break;

    case 7:  // MOVAMT, D7-D4
      myMovAmt = value & 0xF0;
      break;

    default:
      break;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 Cartridge3PD::peek(uInt16 address)
{
  if(!(address & 0x1000))  // TIA space: 3PO's switches, and $2E
    return Cartridge3PO::peek(address);

  // The debugger looking: no side effects, the slot's own bytes
  if(hotspotsLocked())
    return slotPeek(address);

  const uInt16 addr = address & 0x1FFF;

  // Fast fetch: the operand of an LDA # just fetched
  if(myArmed)
  {
    myArmed = false;
    if(addr == myOperandAddress && !inBlock(addr))
    {
      const uInt8 operand = slotPeek(address);
      if(operand >= 0x04 && operand <= 0x17)
        return readRegister(operand);
      return operand;
    }
  }

  if(inBlock(addr))
  {
    const uInt8 offset = addr & 0x7F;
    if(offset < 0x40)
      return readRegister(offset);

    // The write half: the board can't tell a read from a write, so it takes
    // whatever is on the bus
    const uInt8 value = mySystem->getDataBusState();
    writeRegister(offset - 0x40, value);
    return value;
  }

  const uInt8 value = slotPeek(address);

  if((myControl & FAST_ON) && value == 0xA9)  // LDA #
  {
    myArmed = true;
    myOperandAddress = (addr + 1) & 0x1FFF;
  }
  return value;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge3PD::poke(uInt16 address, uInt8 value)
{
  if(!(address & 0x1000))  // TIA space: 3PO's switches, and $2E
    return Cartridge3PO::poke(address, value);

  myArmed = false;

  const uInt16 addr = address & 0x1FFF;
  if(inBlock(addr))
  {
    const uInt8 offset = addr & 0x7F;
    if(!hotspotsLocked())
    {
      if(offset >= 0x40)
        writeRegister(offset - 0x40, value);
      else
        // The read half: the board drives the bus with the register, so a
        // write there only steps the fetcher as a read would
        readRegister(offset);
    }
    return true;
  }

  return Cartridge3PO::poke(address, value);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge3PD::save(Serializer& out) const
{
  if(!Cartridge3PO::save(out))
    return false;

  try
  {
    out.putByte(myControl);
    out.putIntArray(myCounters.data(), myCounters.size());
    out.putByteArray(myTops.data(), myTops.size());
    out.putByteArray(myBottoms.data(), myBottoms.size());
    out.putByteArray(myFlags.data(), myFlags.size());
    for(const bool m: myMusicMode)
      out.putBool(m);
    for(const bool m: mySwapClock)
      out.putBool(m);
    out.putByte(myRandom);
    out.putBool(myDrawLine);
    out.putByte(myLineAcc);
    out.putBool(myLineCarry);
    out.putByte(myMovAmt);
    out.putBool(myArmed);
    out.putShort(myOperandAddress);
    out.putLong(myMusicCycles);
    out.putDouble(myFractionalClocks);
  }
  catch(...)
  {
    cerr << "ERROR: " << name() << "::save\n";
    return false;
  }

  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Cartridge3PD::load(Serializer& in)
{
  if(!Cartridge3PO::load(in))
    return false;

  try
  {
    myControl = in.getByte();
    in.getIntArray(myCounters.data(), myCounters.size());
    in.getByteArray(myTops.data(), myTops.size());
    in.getByteArray(myBottoms.data(), myBottoms.size());
    in.getByteArray(myFlags.data(), myFlags.size());
    for(auto& m: myMusicMode)
      m = in.getBool();
    for(auto& m: mySwapClock)
      m = in.getBool();
    myRandom = in.getByte();
    myDrawLine = in.getBool();
    myLineAcc = in.getByte();
    myLineCarry = in.getBool();
    myMovAmt = in.getByte();
    myArmed = in.getBool();
    myOperandAddress = in.getShort();
    myMusicCycles = in.getLong();
    myFractionalClocks = in.getDouble();
  }
  catch(...)
  {
    cerr << "ERROR: " << name() << "::load\n";
    return false;
  }

  noDirectPeeks();
  return true;
}
