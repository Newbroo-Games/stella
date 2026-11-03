# Stella 7.0c, Newbroo Games build

This branch (`newbroo-7.0c`) is Stella 7.0c with additions by Newbroo Games;
it reports its version as `7.0c-nb1`. It is not an official Stella release, and the Stella Team does not support
it. Please report problems with this build at
[Newbroo-Games/stella](https://github.com/Newbroo-Games/stella/issues), not on
the Stella issue tracker.

Added to 7.0c:

| Addition | What it is | Spec |
|---|---|---|
| 3PO | 3E+ with an outer bank register: up to 16MB of ROM, every 3E+ game runs unchanged | [docs/3po.md](docs/3po.md) |
| 3PD | 3PO plus DPC-style data fetchers and three music voices (new, emulated only so far) | [docs/3pd.md](docs/3pd.md) |
| 4KP | 4K ROM banks and 32K of paged RAM, up to 1MB of ROM | [docs/4kp.md](docs/4kp.md) |
| `-rawstream <file\|->` | every frame, audio sample and the console RAM, raw, for outside tools | [docs/rawstream.md](docs/rawstream.md) |
| `-inputscript <file>` | joystick and console switch input at given frames, the same in every run | `src/emucore/InputScript.hxx` |
| 3EX fix | 3EX RAM banks at 64K and above no longer land on lower banks (stella-emu/stella#1177) | |

Everything else is Stella 7.0c as released. The Stella Team's own text
follows.

---

Stella is a multi-platform Atari 2600 VCS emulator which allows you to
play all of your favourite Atari 2600 games on your PC.  You'll find the
Stella Users Manual in the docs subdirectory.  If you'd like to verify
that you have the latest release of Stella, visit the Stella Website at:

  [stella-emu.github.io](https://stella-emu.github.io)

Enjoy,

The Stella Team

# Reporting issues

Please check the list of known issues first (see below) before reporting new ones.
If you encounter any issues, please open a new issue on the project
[issue tracker](https://github.com/stella-emu/stella/issues).

Note: Please do **not** report issues regarding platforms other than PC and RetroN 77 
(e.g. Libretro) here. The Stella Team is not responsible for these forks and cannot 
help with them.

# Known issues

Please check out the [issue tracker](https://github.com/stella-emu/stella/issues) for
a list of all currently known issues.
