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

#ifndef INPUT_SCRIPT_HXX
#define INPUT_SCRIPT_HXX

#include <map>
#include <vector>

#include "bspf.hxx"
#include "Event.hxx"

/**
  Presses and releases the joysticks and console switches at given emulated
  frames, read from a text file.  Enabled with '-inputscript <path>'.

  Each line is '<frame> <press|release|set> <input>...'; '#' starts a
  comment.  The input changes where frame <frame> starts, counting every
  emulated frame from 0 (the first frame of the raw stream, when there is
  one); frame 0 is from power-on.  The inputs:

    press, release:  p0up p0down p0left p0right p0fire
                     p1up p1down p1left p1right p1fire  reset select
    set:             color bw p0a p0b p1a p1b  (difficulty A or B)

  frameDone() runs on the emulation thread at the end of every frame, so
  the input changes on the same emulated cycle in every run, however fast
  Stella runs.  hold() runs on the main thread while emulation is paused,
  and puts back what a state change cleared.

  @author  Newbroo Games
*/
class InputScript
{
  public:
    InputScript(Event& event, string_view path);

    /**
      Empty if the file was read, else what was wrong with it.
    */
    const string& error() const { return myError; }

    /**
      Set the inputs of frame 0.
    */
    void start();

    /**
      A frame has ended: set the inputs of the next one.

      @return  true if an input changed (the ports must be read again)
    */
    bool frameDone();

    /**
      Set every input the script holds again.
    */
    void hold();

  private:
    bool parse(string_view path);
    bool apply(uInt32 frame);

  private:
    struct Change {
      uInt32 frame{0};
      Event::Type type{Event::NoType};
      Int32 value{0};
    };

    Event& myEvent;
    string myError;
    std::vector<Change> myChanges;   // in frame order
    size_t myNext{0};                // the first change not applied yet
    uInt32 myFrame{0};               // the frame running now
    std::map<Event::Type, Int32> myHeld;
};

#endif
