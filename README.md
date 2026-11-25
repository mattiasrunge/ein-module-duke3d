# duke3d

A module for [EiN](https://einhq.eu) that plays Duke Nukem 3D in the browser,
with its sound effects and its music, on a page of its own at `/duke3d`.

The engine is
[BelgianChocolateDuke3D](https://github.com/GPSnoopy/BelgianChocolateDuke3D),
the maintained fork of
[Chocolate Duke3D](https://github.com/fabiensanglard/chocolate_duke3D), which
keeps 3D Realms' Duke Nukem 3D 1.3D/1.5 as it was. It is built to WebAssembly
with Emscripten, single-threaded, with a platform layer of its own
(`engine/display_ein.c`, `engine/dsl_ein.c`, `engine/music_ein.c`) and no SDL.
It draws with BUILD's software renderer at 640 by 480 unless the player picks
another mode in **Options → Setup Video**. The game keeps its own loops, so it
runs under Emscripten's ASYNCIFY and waits for the page's animation frames; the
page loads it at runtime from the module's `assets/` directory and draws its
frames on a canvas.

It is not one of EiN's own modules and has nothing to do with a family's
archive.

## Install

As an administrator, open **Settings → Modules**, choose **Install**, and give
`https://github.com/mattiasrunge/ein-module-duke3d` under **From a repository**.

Duke Nukem 3D appears in the sidebar under **Games**, a section between Browse
and System that the Doom and Quake modules give their pages too. The module
comes with no games. Until there is one, the page's list offers the free one
with a **Fetch** button that downloads it, checks it against the release's
SHA-256 and puts it in a folder of its own:

- **Duke Nukem 3D (shareware)**: L.A. Meltdown, the first episode of Duke Nukem
  3D 1.3D, which 3D Realms let anyone play and give away free of charge. Its
  licence, `license.txt`, comes with it.

## Play

The page lists the games there are at the left, each with its title screen, read
out of its GRP, and its name under it; on a phone they are behind the list
button under the page's title. Choose one to play it. The page remembers the one
you played last, and its title screen is the picture in the page's header.

Click the game to give it the mouse and the keyboard: the browser hides the
pointer, the mouse turns and aims, and the left button fires. The keys are
Duke's own, which the game's help (F1) shows: the arrows move, Ctrl fires, Space
opens, A jumps, Z crouches, Shift runs, Alt strafes, Escape opens the menu, F2
saves and F3 loads. Escape also gives the mouse back to the browser; click the
game to hand it over again. While the game has the mouse or the focus it takes
its keys before EiN does, Alt+letter included. The buttons in the header turn
the sound off and on and show the game full screen. Quit in the game's menu (or
F10) goes back to the list once the game has shown what it shows on quitting.

A player who has played before starts with their own settings, the keys and the
mouse as they set them in **Options**; a new one starts with mouse aiming on, so
the mouse looks up and down, as players have aimed since, and with the buttons
Duke's own setup gave: the left fires, the right strafes and, clicked, opens,
the middle walks forward.

## Saved games and settings

The game's **Save** and **Load** keep your games in your home, in
`.duke3d/<game>/`, one folder for each game you play, so a save is loaded only
into the game it was made in and nobody else sees it. Each of the ten slots is a
file, `game0.sav` to `game9.sav`, as the game writes it. Beside them is
`duke3d.cfg`, the game's settings and key bindings, written when you quit the
game, choose another, go to another page of EiN's, or close the tab.

Without an account, saves and settings last only as long as the page.

## Add your own games

A game is a folder with a base GRP in it, as Duke's directory was: `DUKE3D.GRP`,
or any `duke3d*.grp`. Drag the folder from your computer and drop it anywhere on
the page, or choose **Upload** in the page's header; it is uploaded, GRP and
all, and appears in the list under the folder's name. The game reads everything
in its folder, under lower-case names, as it read its directory on DOS.

- The whole game: the `DUKE3D.GRP` of your own Duke Nukem 3D 1.3D, or of the
  Atomic Edition 1.4 or 1.5. The list calls them **Duke Nukem 3D** and **Duke
  Nukem 3D: Atomic Edition**.
- The 20th Anniversary World Tour's GRP needs EDuke32's additions and does not
  play here, nor do add-ons that need more than Duke's own CON files, nor
  multiplayer.

The games are kept in `/groups/users/duke3d`, the household's shared folder, so
everyone with an account plays the same ones and may add to them, and they stay
when the module is updated or installed again. The folder's `processing`
attribute is empty, so EiN's media pipelines leave it alone. The bin beside a
game deletes it for everyone; the free one is offered again once it is gone.

In EiN's terminal, `duke3d-games` lists the games with their titles, GRPs and
files, and `duke3d-fetch shareware` fetches the free one, as the page's
**Fetch** does.

## Sound

The sound effects are Apogee's own mixer, the one Duke shipped with, mixing a
buffer at a time as it did for a Sound Blaster; Web Audio plays them. The music
is played as on a Sound Blaster 16: the songs' MIDI through Apogee's own
sequencer and AdLib driver, with the timbres Duke brought for it
(`D3DTIMBR.TMB`), into an OPL3, which
[Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3) emulates. **Options → Setup
Sound** sets the effects' and the music's volume.

## Layout

| Path                                                | What it is for                                                                                                                     |
| --------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| `mod.json`, `ui/nav.json`                           | The manifest, and the sidebar entry with its Games section.                                                                        |
| `ui/pages/duke3d.tsx`                               | The page: the list of games and the ones to fetch, uploads, keyboard, mouse, sound and fullscreen.                                 |
| `ui/duke3d/games.ts`                                | Lists, fetches and deletes games.                                                                                                  |
| `ui/duke3d/titlepic.ts`                             | Draws a game's title screen out of its GRP, fetching only the parts it needs.                                                      |
| `ui/duke3d/engine.ts`                               | Loads the glue, the wasm and the game's files, starts the game and wraps the engine's exports.                                     |
| `ui/duke3d/loop.ts`                                 | Draws the game's frames and gives it its turns, on animation frames.                                                               |
| `ui/duke3d/saves.ts`                                | Reads and keeps the player's saves and settings in their home.                                                                     |
| `ui/duke3d/sound.ts`                                | The game's sound on Web Audio.                                                                                                     |
| `ui/duke3d/keys.ts`                                 | Browser key codes to the PC keyboard's scancodes, and mouse buttons to BUILD's.                                                    |
| `bin/duke3d-games`                                  | Lists the games with their titles, GRPs and files, for the page.                                                                   |
| `bin/duke3d-fetch`                                  | Fetches the free game the page offers into the games folder, checked against its release's SHA-256.                                |
| `lib/games.ts`, `lib/api.ts`                        | Where the games are, the editions known by their GRP, the game the page offers, and the shape `duke3d-games` answers in.           |
| `engine/display_ein.c`                              | BUILD's display, keyboard, mouse and timer driver for the page, and the waits for its animation frames.                            |
| `engine/dsl_ein.c`                                  | The sound card: Apogee's mixer's buffers, and the music added to them, handed to the page.                                         |
| `engine/music_ein.c`                                | The music: Apogee's sequencer on the samples' clock, its AdLib driver's writes to Nuked OPL3, and the chip's output.               |
| `engine/al/`                                        | The little of DOS (ports, interrupts, DPMI, the Sound Blaster) that Apogee's music code still names.                               |
| `engine/include/SDL2/SDL.h`, `engine/unix_compat.h` | The little of SDL the game's code still names, and what it expects of a compiler.                                                  |
| `engine/Containerfile`, `engine/build.sh`           | The build: pinned emsdk image, BelgianChocolateDuke3D, Chocolate Duke3D's music code and Nuked OPL3, and the compiler's commands.  |
| `engine/portability.patch`                          | Builds with Emscripten where the code took Linux or macOS for granted.                                                             |
| `engine/waits.patch`                                | Lets the page run in the two waits that never did: for the skill's sound to end, and for an answer at a console nobody sees.       |
| `engine/files.patch`                                | Tells the page when the game has saved a game or written its settings, so it can keep them.                                        |
| `engine/new-player.patch`                           | Starts a new player with mouse aiming on.                                                                                          |
| `engine/apogee-music.patch`                         | Lets the music's task be scheduled from outside Apogee's sequencer.                                                                |
| `assets/icon.svg`                                   | A radiation trefoil, as Duke's menus have them, drawn for the module: the sidebar's icon, filled with the text colour.             |
| `assets/duke3d.js`, `assets/duke3d.wasm`            | The built engine, committed, since a module is installed as its repository stands. `assets/duke3d-build.txt` says what built them. |
| `scripts/build-engine.sh`                           | Rebuilds `assets/duke3d.js` and `assets/duke3d.wasm`.                                                                              |
| `test/smoke.mjs`                                    | Signs in with headless Chrome, plays a little, saves screenshots.                                                                  |
| `deno.jsonc`                                        | Generated from `mod.json` by core's `deno task sync:modules`, for type-checking in a core checkout.                                |

## Building the engine

Needs only docker with buildx, or podman; nothing is installed on the host.

    scripts/build-engine.sh

It builds `engine/Containerfile` and exports the result into `assets/` with the
local exporter. The source of the committed `assets/duke3d.wasm` is
BelgianChocolateDuke3D at the commit `engine/Containerfile` pins, with
`engine/portability.patch`, `engine/waits.patch`, `engine/files.patch` and
`engine/new-player.patch` applied; Apogee's `al_midi.c`, `gmtimbre.c` and
`midi.c` from Chocolate Duke3D at the commit it pins, with
`engine/apogee-music.patch`; Nuked OPL3 at the commit it pins; and the `engine/`
files added.

## Developing

Mount the checkout read-only in EiN's container as `/modules-dev/duke3d` and set
`DEV_MODULES_PATH=/modules-dev`; EiN sets the module up when it starts, and it
runs instead of an installed one. The folder must be called `duke3d`, the
module's name. After a change to a page, run `ui-build duke3d` in EiN's terminal
and reload the browser; after a change to `mod.json`, `mod setup duke3d`. A file
in `assets/` is read as it is.

The smoke test wants a running EiN with the module and a game, Google Chrome,
and Node 22 or later:

    EIN_PASSWORD=... CHROME=/usr/bin/google-chrome node test/smoke.mjs --out /some/directory

## Licence

The game's code, Apogee's sound code and this module's own are GPL-2.0-or-later:
see `LICENSE`. The BUILD engine is Ken Silverman's, under his BUILD licence,
which allows its use only free of charge: see `BUILDLIC.TXT`.

    // "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
    // Ken Silverman's official web site: "http://www.advsys.net/ken"
    // See the included license file "BUILDLIC.TXT" for license info.

Nuked OPL3 is © Nuke.YKT, under the LGPL-2.1-or-later; Duke Nukem 3D's source is
© 3D Realms and Apogee Software, Chocolate Duke3D's © Fabien Sanglard and its
contributors, BelgianChocolateDuke3D's © GPSnoopy; their notices are in the
source the build fetches. `assets/duke3d.js` is Emscripten's runtime, under the
MIT licence. `assets/icon.svg` is the module's own drawing of a radiation
trefoil. The module fetches and does not ship the game: the shareware episode is
3D Realms', under its own licence, and Duke Nukem 3D is a trademark of Gearbox
Software.
