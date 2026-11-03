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

#include "Cart3PO.hxx"
#include "EditTextWidget.hxx"
#include "PopUpWidget.hxx"
#include "CartEnhancedWidget.hxx"
#include "Cart3POWidget.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Cartridge3POWidget::Cartridge3POWidget(
      GuiObject* boss, const GUI::Font& lfont, const GUI::Font& nfont,
      int x, int y, int w, int h, Cartridge3PO& cart)
  : CartridgeEnhancedWidget(boss, lfont, nfont, x, y, w, h, cart),
    myCart3PO{cart}
{
  initialize();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
string Cartridge3POWidget::description()
{
  ostringstream info;
  size_t size{0};
  const ByteBuffer& image = myCart.getImage(size);
  const uInt16 numRomBanks = myCart.romBankCount();
  const uInt16 numRamBanks = myCart.ramBankCount();

  info << "3PO cartridge - (3E+ with an outer bank, 1K" << ELLIPSIS
       << "16MB ROM + RAM)\n"
    << "  " << numRomBanks << " 1K ROM banks + " << numRamBanks << " 512b RAM banks\n"
    << "  mapped into four segments\n"
       "Outer bank (bank / 64) selected by writing to $2F\n"
       "ROM bank % 64 & segment selected by writing to $3F\n"
       "RAM bank & segment selected by writing to $3E\n"
       "  Lower 512b of segment for read access\n"
       "  Upper 512b of segment for write access\n"
       "Startup bank = -1/-1/-1/0 (ROM), outer bank 0\n";

  // Eventually, we should query this from the debugger/disassembler
  uInt16 start = (image[0x400 - 3] << 8) | image[0x400 - 4];
  start -= start % 0x1000;
  info << "Bank RORG" << " = $" << Common::Base::HEX4 << start << "\n";

  return info.str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge3POWidget::bankSelect(int& ypos)
{
  size_t size{0};
  const ByteBuffer& image = myCart.getImage(size);
  const int VGAP = myFontHeight / 4;
  VariantList banktype;

  VarList::push_back(banktype, "ROM", "ROM");
  VarList::push_back(banktype, "RAM", "RAM");

  myBankWidgets = make_unique<PopUpWidget* []>(bankSegs());

  ypos -= VGAP * 2;

  for(uInt32 seg = 0; seg < bankSegs(); ++seg)
  {
    int xpos = 2, ypos_s = ypos + 1, width = 0;
    ostringstream label;
    VariantList items;

    label << "Set segment " << seg << " as ";

    new StaticTextWidget(_boss, _font, xpos, ypos, label.view());
    ypos += myLineHeight + VGAP * 2;

    xpos += _font.getMaxCharWidth() * 2;

    CartridgeEnhancedWidget::bankList(std::max(myCart.romBankCount(), myCart.ramBankCount()),
                                      seg, items, width);
    myBankWidgets[seg] =
      new PopUpWidget(_boss, _font, xpos, ypos - 2, width,
                      myLineHeight, items, "Bank ", 0, kBankChanged);
    myBankWidgets[seg]->setID(seg);
    myBankWidgets[seg]->setTarget(this);
    addFocusWidget(myBankWidgets[seg]);

    xpos += myBankWidgets[seg]->getWidth();
    myBankType[seg] =
      new PopUpWidget(_boss, _font, xpos, ypos - 2, 3 * _font.getMaxCharWidth(),
                      myLineHeight, banktype, " of ", 0, kRomRamChanged);
    myBankType[seg]->setID(seg);
    myBankType[seg]->setTarget(this);
    addFocusWidget(myBankType[seg]);

    xpos = myBankType[seg]->getRight() + _font.getMaxCharWidth();

    myBankCommit[seg] = new ButtonWidget(_boss, _font, xpos, ypos - 4,
                                         _font.getStringWidth(" Commit "), myButtonHeight,
                                         "Commit", kChangeBank);
    myBankCommit[seg]->setID(seg);
    myBankCommit[seg]->setTarget(this);
    addFocusWidget(myBankCommit[seg]);

    const int xpos_s = myBankCommit[seg]->getRight() + _font.getMaxCharWidth() * 2;

    uInt16 start = (image[0x400 - 3] << 8) | image[0x400 - 4];
    start -= start % 0x1000;
    const int addr1 = start + (seg * 0x400), addr2 = addr1 + 0x200;

    label.str("");
    label << "$" << Common::Base::HEX4 << addr1 << "-$"
          << Common::Base::HEX4 << (addr1 + 0x1FF);
    auto* t = new StaticTextWidget(_boss, _font, xpos_s, ypos_s + 2, label.view());

    const int xoffset = t->getRight() + _font.getMaxCharWidth();
    const size_t bank_off = static_cast<size_t>(seg) * 2;
    myBankState[bank_off] = new EditTextWidget(_boss, _font, xoffset, ypos_s,
                                               _w - xoffset - 10, myLineHeight, "");
    myBankState[bank_off]->setEditable(false, true);
    ypos_s += myLineHeight + VGAP;

    label.str("");
    label << "$" << Common::Base::HEX4 << addr2 << "-$" << Common::Base::HEX4 << (addr2 + 0x1FF);
    new StaticTextWidget(_boss, _font, xpos_s, ypos_s + 2, label.view());

    myBankState[bank_off + 1] = new EditTextWidget(_boss, _font,
        xoffset, ypos_s, _w - xoffset - 10, myLineHeight, "");
    myBankState[bank_off + 1]->setEditable(false, true);

    ypos += myLineHeight + VGAP * 4;
  }

  // The outer bank register: the bank numbers the next writes to $3F reach
  const auto* t = new StaticTextWidget(_boss, _font, 2, ypos + 2, "Outer bank ($2F) ");
  const int xoffset = t->getRight();
  myOuterState = new EditTextWidget(_boss, _font, xoffset, ypos,
                                    _w - xoffset - 10, myLineHeight, "");
  myOuterState->setEditable(false, true);
  ypos += myLineHeight + VGAP * 4;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge3POWidget::loadConfig()
{
  CartridgeEnhancedWidget::loadConfig();
  updateUIState();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge3POWidget::handleCommand(CommandSender* sender,
                                        int cmd, int data, int id)
{
  const uInt16 segment = id;

  switch(cmd)
  {
    case kBankChanged:
    case kRomRamChanged:
    {
      const bool isROM = myBankType[segment]->getSelectedTag() == "ROM";
      const int bank = myBankWidgets[segment]->getSelected();

      myBankCommit[segment]->setEnabled((isROM && bank < myCart.romBankCount())
        || (!isROM && bank < myCart.ramBankCount()));
      break;
    }
    case kChangeBank:
    {
      // Ignore bank if either number or type hasn't been selected
      if(myBankWidgets[segment]->getSelected() < 0 ||
         myBankType[segment]->getSelected() < 0)
        return;

      // Up to 16384 ROM banks; the outer register is left as it is
      const auto bank = static_cast<uInt16>(myBankWidgets[segment]->getSelected());

      myCart.unlockHotspots();

      if(myBankType[segment]->getSelectedTag() == "ROM")
        myCart.bank(bank, segment);
      else
        myCart.bank(bank + myCart.romBankCount(), segment);

      myCart.lockHotspots();
      invalidate();
      updateUIState();
      break;
    }
    default:
      break;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Cartridge3POWidget::updateUIState()
{
  // Set description for each 1K segment state (@ each index)
  // Set contents for actual banks number and type (@ each even index)
  for(int seg = 0; seg < myCart3PO.myBankSegs; ++seg)
  {
    const uInt16 bank = myCart.getSegmentBank(seg);
    const size_t bank_off = static_cast<size_t>(seg) * 2;
    ostringstream buf;

    if(bank >= myCart.romBankCount()) // was RAM mapped here?
    {
      const uInt16 ramBank = bank - myCart.romBankCount();

      buf << "RAM @ $" << Common::Base::HEX4
          << (ramBank << myCart3PO.myBankShift) << " (R)";
      myBankState[bank_off]->setText(buf.view());

      buf.str("");
      buf << "RAM @ $" << Common::Base::HEX4
        << ((ramBank << myCart3PO.myBankShift) + myCart3PO.myBankSize) << " (W)";
      myBankState[bank_off + 1]->setText(buf.view());

      myBankWidgets[seg]->setSelectedIndex(ramBank);
      myBankType[seg]->setSelected("RAM");
    }
    else
    {
      // ROM image offsets past $FFFF take more than four digits
      buf << "ROM #" << std::dec << bank << " @ $" << Common::Base::HEX4
        << (static_cast<uInt32>(bank) << myCart3PO.myBankShift);
      myBankState[bank_off]->setText(buf.view());

      buf.str("");
      buf << "ROM #" << std::dec << bank << " @ $" << Common::Base::HEX4
        << ((static_cast<uInt32>(bank) << myCart3PO.myBankShift) + myCart3PO.myBankSize);
      myBankState[bank_off + 1]->setText(buf.view());

      myBankWidgets[seg]->setSelectedIndex(bank);
      myBankType[seg]->setSelected("ROM");
    }
  }

  ostringstream buf;
  const uInt32 first = myCart3PO.outer() * 64U;

  buf << "$" << Common::Base::HEX2 << static_cast<int>(myCart3PO.outer())
      << ": $3F picks ROM #" << std::dec << (first % myCart.romBankCount())
      << "-" << ((first + 63) % myCart.romBankCount());
  myOuterState->setText(buf.view());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
string Cartridge3POWidget::internalRamDescription()
{
  ostringstream desc;

  desc << "Accessible 512b at a time via:\n"
       << "  $f000/$f400/$f800/$fc00 for read access\n"
       << "  $f200/$f600/$fa00/$fe00 for write access";

  return desc.str();
}
