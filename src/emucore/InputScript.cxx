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

#include <algorithm>
#include <fstream>
#include <sstream>

#include "InputScript.hxx"

namespace {
  // The inputs pressed and released
  struct Button { string_view name; Event::Type type; };
  constexpr std::array<Button, 12> BUTTONS = {{
    { "p0up",    Event::LeftJoystickUp },    { "p0down",  Event::LeftJoystickDown },
    { "p0left",  Event::LeftJoystickLeft },  { "p0right", Event::LeftJoystickRight },
    { "p0fire",  Event::LeftJoystickFire },
    { "p1up",    Event::RightJoystickUp },   { "p1down",  Event::RightJoystickDown },
    { "p1left",  Event::RightJoystickLeft }, { "p1right", Event::RightJoystickRight },
    { "p1fire",  Event::RightJoystickFire },
    { "reset",   Event::ConsoleReset },      { "select",  Event::ConsoleSelect }
  }};

  // The switches set: the event that sets each position, and the other one
  struct Switch { string_view name; Event::Type on, off; };
  constexpr std::array<Switch, 6> SWITCHES = {{
    { "color", Event::ConsoleColor,      Event::ConsoleBlackWhite },
    { "bw",    Event::ConsoleBlackWhite, Event::ConsoleColor },
    { "p0a",   Event::ConsoleLeftDiffA,  Event::ConsoleLeftDiffB },
    { "p0b",   Event::ConsoleLeftDiffB,  Event::ConsoleLeftDiffA },
    { "p1a",   Event::ConsoleRightDiffA, Event::ConsoleRightDiffB },
    { "p1b",   Event::ConsoleRightDiffB, Event::ConsoleRightDiffA }
  }};
}  // namespace

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
InputScript::InputScript(Event& event, string_view path)
  : myEvent{event}
{
  if(parse(path))
    std::stable_sort(myChanges.begin(), myChanges.end(),
        [](const Change& a, const Change& b) { return a.frame < b.frame; });
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool InputScript::parse(string_view path)
{
  std::ifstream in{string{path}};
  if(!in)
  {
    myError = "inputscript couldn't open '" + string{path} + "'";
    return false;
  }

  string line;
  for(int number = 1; std::getline(in, line); ++number)
  {
    line = line.substr(0, line.find('#'));
    std::istringstream words{line};
    string frameText, verb, name;
    if(!(words >> frameText))
      continue;  // blank or comment

    const auto fail = [&](string_view why) {
      myError = "inputscript line " + std::to_string(number) + ": " + string{why};
      return false;
    };
    if(frameText.size() > 9 || !std::all_of(frameText.begin(), frameText.end(),
        [](char c) { return c >= '0' && c <= '9'; }))
      return fail("'" + frameText + "' is not a frame number");
    const auto frame = static_cast<uInt32>(std::stoul(frameText));

    if(!(words >> verb) || (verb != "press" && verb != "release" && verb != "set"))
      return fail("expected press, release or set after the frame");

    bool any = false;
    while(words >> name)
    {
      any = true;
      if(verb == "set")
      {
        const auto s = std::find_if(SWITCHES.begin(), SWITCHES.end(),
            [&](const Switch& w) { return w.name == name; });
        if(s == SWITCHES.end())
          return fail("'" + name + "' is not a switch (color bw p0a p0b p1a p1b)");
        myChanges.push_back({frame, s->on, 1});
        myChanges.push_back({frame, s->off, 0});
      }
      else
      {
        const auto b = std::find_if(BUTTONS.begin(), BUTTONS.end(),
            [&](const Button& w) { return w.name == name; });
        if(b == BUTTONS.end())
          return fail("'" + name + "' is not a joystick input, reset or select");
        myChanges.push_back({frame, b->type, verb == "press" ? 1 : 0});
      }
    }
    if(!any)
      return fail("no input named");
  }
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool InputScript::apply(uInt32 frame)
{
  bool changed = false;
  for(; myNext < myChanges.size() && myChanges[myNext].frame <= frame; ++myNext)
  {
    const Change& c = myChanges[myNext];
    myHeld[c.type] = c.value;
    myEvent.set(c.type, c.value);
    changed = true;
  }
  return changed;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void InputScript::start()
{
  apply(0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool InputScript::frameDone()
{
  return apply(++myFrame);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void InputScript::hold()
{
  for(const auto& [type, value]: myHeld)
    myEvent.set(type, value);
}
