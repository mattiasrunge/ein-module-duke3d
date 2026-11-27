# duke3d

A module for [EiN](https://einhq.eu) that plays Duke Nukem 3D in the browser,
with its sound effects and its music. Its games are on EiN's **Games** page,
which the games module gives: the module brings the engine and its games, and
the games module the page each is played on, the saves, the sound and the
dropped files.

The engine is
[BelgianChocolateDuke3D](https://github.com/GPSnoopy/BelgianChocolateDuke3D),
the maintained fork of
[Chocolate Duke3D](https://github.com/fabiensanglard/chocolate_duke3D), which
keeps 3D Realms' Duke Nukem 3D 1.3D/1.5 as it was. It is built to WebAssembly
with Emscripten, single-threaded, with a platform layer of its own
(`engine/display_ein.c`, `engine/dsl_ein.c`, `engine/music_ein.c`,
`engine/game_ein.c`), no SDL and no DOM. It draws with BUILD's software renderer
at 640 by 480 unless the player picks another mode in **Options → Setup Video**.

The engine runs in the module's Web Worker (`ui/duke3d/engine-worker.ts`), on
the games module's engine-worker kit, not on the page. The worker loads it at
runtime from the module's `assets/` directory and reads the game's GRP from EiN
a megabyte at a time as the game asks for its files, so a game starts without
downloading all of its GRP, and only what is played is fetched. The game keeps
its own loops, so it runs under Emscripten's ASYNCIFY: whenever it has finished
a frame it waits for the worker's next tick, which the games module's page asks
for at each of its animation frames, and answers with the frame and the sound
mixed since the last tick. The page draws the frame on its canvas and plays the
sound. A slow frame stalls the game, not the page, and nothing is shared memory,
so it plays the same over plain HTTP as over HTTPS.

It is not one of EiN's own modules and has nothing to do with a family's
archive.

## Install

It needs EiN's games module (`games`, 0.1.1-dev.2 or later, for its engine
worker kit) and an EiN whose module API is 0.15.77 or later, which gives modules
their Web Workers. As an administrator, open **Settings → Modules**, choose
**Install**, and give `https://github.com/mattiasrunge/ein-module-duke3d` under
**From a repository**.

The games are on the **Games** page, under Browse in the sidebar, a card each.
The module comes with no games. Until there is one, the Games page offers the
free one under **Free to fetch**, with a **Fetch** button for an administrator
(EiN lets only the administrator fetch from the internet) that downloads it,
checks it against the release's SHA-256 and puts it in a folder of its own:

- **Duke Nukem 3D (shareware)**: L.A. Meltdown, the first episode of Duke Nukem
  3D 1.3D, which 3D Realms let anyone play and give away free of charge. Its
  licence, `license.txt`, comes with it.

## Play

Each game's card on the Games page has its title screen, read out of its GRP,
and under its title the GRP, its size and how many user maps it has. Choose one
to open its page, `/games/duke3d/<game>`, and **Play** to play it. **Play on a
map**, in the page's header, starts the game on one of its user maps instead
(below), as `-map` on Duke's command line did.

Click the game to give it the mouse and the keyboard: the browser hides the
pointer, the mouse turns and aims, and the left button fires. The keys are
Duke's own, which the game's help (F1) shows: the arrows move, Ctrl fires, Space
opens, A jumps, Z crouches, Shift runs, Alt strafes, Escape opens the menu, F2
saves and F3 loads. Escape also gives the mouse back to the browser; click the
game to hand it over again. While the game has the mouse or the focus it takes
its keys before EiN does, Alt+letter included. The buttons in the header turn
the sound off and on and show the game full screen. Quit in the game's menu (or
F10) leaves the page as it was before **Play**, once the game has shown what it
shows on quitting.

The page shows the game at 4:3 in every mode, as a monitor showed Duke's 320
by 200 and its VESA modes.

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
`duke3d.cfg`, the game's settings and key bindings, written as a menu of the
game's closes, when you quit the game, and when you go to another page of EiN's.

Without an account, saves and settings last only as long as the page.

## Add your own games

A game is a folder with a base GRP in it, as Duke's directory was: `DUKE3D.GRP`,
or any `duke3d*.grp`. Drop the GRP by itself anywhere on the Games page and it
is put in a folder of its own, named after the file; drop a folder with a
`DUKE3D.GRP` in it and it is uploaded as it is, GRP and all, under the folder's
name. Either appears as a card. The game reads everything in its folder, under
lower-case names, as it read its directory on DOS.

- The whole game: the `DUKE3D.GRP` of your own Duke Nukem 3D 1.3D, or of the
  Atomic Edition 1.4 or 1.5. The list calls them **Duke Nukem 3D** and **Duke
  Nukem 3D: Atomic Edition**.
- The 20th Anniversary World Tour's GRP needs EDuke32's additions and does not
  play here, nor do add-ons that need more than Duke's own CON files, nor
  multiplayer.

### Where your copy keeps its GRP

Only the GRP is needed, and `DUKE.RTS` if your copy has one beside it (the
voices of Remote Ridicule). Drop the GRP, then `DUKE.RTS` into the game's
folder, or put them in a folder named for the game, `atomic` say, and drop the
folder. Leave the rest behind, a copy's DOSBox, manuals, installers and
launchers, which the game never reads and would only be uploaded with it.

- **A DOS installation or CD**: `DUKE3D.GRP` is in the game's directory,
  `C:\DUKE3D` as Duke's setup made it.
- **An installed GOG or Steam copy**: `DUKE3D.GRP` is in the game's folder; the
  Steam Megaton Edition keeps it in `gameroot/`.
- **GOG's offline installer** (`setup_duke3d_<version>.exe`): it is an Inno
  Setup installer, which [innoextract](https://constexpr.org/innoextract/)
  unpacks without Windows or installing the game (Debian and Ubuntu have it as
  `innoextract`):

  ```sh
  innoextract -e -d duke3d-gog setup_duke3d_<version>.exe
  find duke3d-gog -iname 'duke3d*.grp' -o -iname 'duke.rts'
  ```

The list says which edition a GRP is when it knows its size; one it does not
know, a modified GRP say, still plays and is listed under its folder's name.

### User maps

A user map, a `.map` file made with BUILD for Duke Nukem 3D, dropped on the
Games page goes into a game's folder, beside its GRP; the page asks which game
when there is more than one. **Play on a map** then offers it, by its name
without `.map`. The shareware episode plays no user maps, as Duke's shareware
never did, so give a map to the whole game.

The games are kept in `/groups/users/duke3d`, the household's shared folder, so
everyone with an account plays the same ones and may add to them, and they stay
when the module is updated or installed again. The folder's `processing`
attribute is empty, so EiN's media pipelines leave it alone. **Delete game**, in
the ⋮ menu of the game's page, deletes it for everyone; the free one is offered
again once it is gone.

In EiN's terminal, `duke3d-games` lists the games with their titles, GRPs, files
and user maps, and the free one when it is not fetched yet; `games-list` lists
them with every other game module's. `duke3d-fetch shareware` fetches the free
one, as **Fetch** does.

## Sound

The sound effects are Apogee's own mixer, the one Duke shipped with, mixing a
buffer at a time as it did for a Sound Blaster; Web Audio plays them. The music
is played as on a Sound Blaster 16: the songs' MIDI through Apogee's own
sequencer and AdLib driver, with the timbres Duke brought for it
(`D3DTIMBR.TMB`), into an OPL3, which
[Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3) emulates. **Options → Setup
Sound** sets the effects' and the music's volume.

## Layout

| Path                                                | What it is for                                                                                                                                              |
| --------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `mod.json`                                          | The manifest: the games module it needs, the route of a game's page, and the engine worker.                                                                 |
| `etc/games.d/games.json`                            | What the games module knows of the module: its title, icon and route, its list and fetch commands, its games folder, and the files it takes when dropped.   |
| `ui/pages/game.tsx`                                 | A game's page, `/games/duke3d/<game>`: the games module's page for a game, with the module's adapter.                                                       |
| `ui/adapter.ts`                                     | The adapter the games module plays the games with: lists them, starts one, and names the saves kept.                                                        |
| `ui/duke3d/engine.ts`                               | Starts a game in the engine worker: its build, its GRP read as the game asks, its other files and the saves whole, and the keys and mouse as it takes them. |
| `ui/duke3d/engine-worker.ts`                        | The engine worker (`ui.workers`): runs the game a tick at a time under ASYNCIFY, and hands the page its frames, its sound and the files it writes.          |
| `ui/duke3d/start.ts`                                | What the page starts the worker with, and where the game's files are in the engine's file system; free of the DOM, as all the worker imports is.            |
| `ui/duke3d/keys.ts`                                 | Browser key codes to the PC keyboard's scancodes, and mouse buttons to BUILD's.                                                                             |
| `bin/duke3d-games`                                  | Lists the games with their titles, title screens, GRPs, files and user maps, and the free one not fetched yet, for the Games page and a game's page.        |
| `lib/games.ts`, `lib/api.ts`                        | Where the games are, the editions known by their GRP, the free game offered, and the shape `duke3d-games` answers in.                                       |
| `lib/catalog.ts`                                    | Reads the games folder: each game's GRP, files and user maps, and what `duke3d-games` says of it.                                                           |
| `lib/cover.ts`                                      | Decodes a game's title screen out of its GRP, reading only the parts it needs, as a PNG.                                                                    |
| `bin/duke3d-fetch`                                  | Fetches the free game into the games folder, checked against its release's SHA-256.                                                                         |
| `engine/display_ein.c`                              | BUILD's display, keyboard, mouse and timer driver for the engine worker, and the waits for its ticks.                                                       |
| `engine/dsl_ein.c`                                  | The sound card: Apogee's mixer's buffers, and the music added to them, handed to the page.                                                                  |
| `engine/music_ein.c`                                | The music: Apogee's sequencer on the samples' clock, its AdLib driver's writes to Nuked OPL3, and the chip's output.                                        |
| `engine/game_ein.c`                                 | What the worker asks of the game itself: whether a menu or the console is up, and its settings written.                                                     |
| `engine/al/`                                        | The little of DOS (ports, interrupts, DPMI, the Sound Blaster) that Apogee's music code still names.                                                        |
| `engine/include/SDL2/SDL.h`, `engine/unix_compat.h` | The little of SDL the game's code still names, and what it expects of a compiler.                                                                           |
| `engine/Containerfile`, `engine/build.sh`           | The build: pinned emsdk image, BelgianChocolateDuke3D, Chocolate Duke3D's music code and Nuked OPL3, and the compiler's commands.                           |
| `engine/portability.patch`                          | Builds with Emscripten where the code took Linux or macOS for granted.                                                                                      |
| `engine/waits.patch`                                | Lets the page run in the two waits that never did: for the skill's sound to end, and for an answer at a console nobody sees.                                |
| `engine/files.patch`                                | Tells the page when the game has saved a game or written its settings, so it can keep them.                                                                 |
| `engine/new-player.patch`                           | Starts a new player with mouse aiming on.                                                                                                                   |
| `engine/grp-crc.patch`                              | Knows a GRP of a release's size by it, not by reading all of it for its CRC32, so the GRP is read only as the game asks.                                    |
| `engine/apogee-music.patch`                         | Lets the music's task be scheduled from outside Apogee's sequencer.                                                                                         |
| `assets/icon.svg`                                   | A radiation trefoil, as Duke's menus have them, drawn for the module: its icon on the Games page, filled with the text colour.                              |
| `assets/duke3d.js`, `assets/duke3d.wasm`            | The built engine, committed, since a module is installed as its repository stands. `assets/duke3d-build.txt` says what built them.                          |
| `scripts/build-engine.sh`                           | Rebuilds `assets/duke3d.js` and `assets/duke3d.wasm`.                                                                                                       |
| `test/smoke.mjs`                                    | Signs in with headless Chrome, plays a little, saves screenshots.                                                                                           |
| `deno.jsonc`                                        | Generated from `mod.json` by core's `deno task sync:modules`, for type-checking in a core checkout.                                                         |

## Building the engine

Needs only docker with buildx, or podman; nothing is installed on the host.

    scripts/build-engine.sh

It builds `engine/Containerfile` and exports the result into `assets/` with the
local exporter. The source of the committed `assets/duke3d.wasm` is
BelgianChocolateDuke3D at the commit `engine/Containerfile` pins, with
`engine/portability.patch`, `engine/waits.patch`, `engine/files.patch`,
`engine/new-player.patch` and `engine/grp-crc.patch` applied; Apogee's
`al_midi.c`, `gmtimbre.c` and `midi.c` from Chocolate Duke3D at the commit it
pins, with `engine/apogee-music.patch`; Nuked OPL3 at the commit it pins; and
the `engine/` files added.

## Developing

Mount the checkout read-only in EiN's container as `/modules-dev/duke3d` and set
`DEV_MODULES_PATH=/modules-dev`; EiN sets the module up when it starts, and it
runs instead of an installed one. The folder must be called `duke3d`, the
module's name. The games module has to be there too, installed or mounted beside
it. After a change under `ui/`, run `ui-build duke3d` in EiN's terminal and
reload the browser; after a change to `mod.json`, `mod setup duke3d`. A file in
`assets/` is read as it is.

The smoke test wants a running EiN with the games module, this one and a game
(`--game`, the shareware episode's folder when not given), Google Chrome, and
Node 22 or later:

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
