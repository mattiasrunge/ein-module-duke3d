# duke3d

A module for [EiN](https://einhq.eu) that plays Duke Nukem 3D in the browser,
with its sound effects and its music. Its games are on EiN's **Games** page,
which the games module gives: the module brings the engine and its games, and
the games module the page each is played on, the saves, the sound and the
dropped files.

The engine is [EDuke32](https://www.eduke32.com/), the long-running source port
of Duke Nukem 3D, as
[DigitalCyberSoft's eduke32-wasm](https://github.com/DigitalCyberSoft/eduke32-wasm)
builds it to WebAssembly, with a platform layer of this module's own in place of
SDL's (`engine/files/source/build/src/einlayer.cpp`, a sound card the worker
pulls from in `driver_ein.cpp`), and no DOM. It draws with WebGL 2: EDuke32's
Polymost renderer in true colour with filtered textures, at 1280 by 720 for a
new player, or BUILD's classic renderer, its 8-bit picture shown through WebGL,
as **Options → Display Setup → Video mode** sets it. Polymost's OpenGL, the
fixed-function OpenGL of its time, is a layer of the module's own on WebGL 2
(`engine/files/source/build/src/glbuild_ein.cpp`), which merges the polygons
drawn alike into one draw and hands WebGL only the state that changed, so a
browser without WebGL 2 does not play the game.

The engine runs in the module's Web Worker (`ui/duke3d/engine-worker.ts`), on
the games module's engine-worker kit, not on the page, and draws on the worker's
canvas. The worker loads it at runtime from the module's `assets/` directory and
reads the game's GRP from EiN a quarter of a megabyte at a time as the game asks
for its files, so only what is played is fetched. The game keeps its own loops,
so it runs under Emscripten's ASYNCIFY: whenever it has shown a frame it waits
for the worker's next tick, which the games module's page asks for at each of
its animation frames, and answers with the frame and the sound mixed since the
last tick. The page draws the frame on its canvas and plays the sound. A slow
frame stalls the game, not the page, and nothing is shared memory, so it plays
the same over plain HTTP as over HTTPS.

It is not one of EiN's own modules and has nothing to do with a family's
archive.

## Install

It needs EiN's games module (`games`, 0.1.2-dev.8 or later, for its engine
worker kit and the keys in a game's header), an EiN whose module API is 0.15.77
or later, which gives modules their Web Workers, and a browser with WebGL 2. As an administrator,
open **Settings → Modules**, choose **Install**, and give
`https://github.com/mattiasrunge/ein-module-duke3d` under **From a repository**.

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
pointer, the mouse turns and aims, the left button fires, the right opens, and
the wheel changes weapon, up to the next and down to the one before. As in the
Quake module, WASD moves, Space jumps, C crouches and Shift runs; E opens, Q
kicks, Ctrl fires too, the arrows turn, and Duke's other keys are EDuke32's: F2
saves, F3 loads, Escape opens the menu and the key under it (`` ` ``) the
console. The game's help (F1) is Duke's own, which shows his keys of 1996, the
arrows moving, Space opening, A jumping and Z crouching. Escape also gives the
mouse back to the browser; click the game to hand it over again. While the game
has the mouse or the focus it takes its keys before EiN does, Alt+letter
included; the console and the menus' fields take what a US keyboard types. The
buttons in the header turn the sound off and on and show the game full screen.
Quit in the game's menu leaves the page as it was before **Play**.

Where a mouse and keyboard play and the page is wide enough, its header shows
your keys: the ones you move, fire, jump, open, crouch and run with, what the
mouse's buttons and wheel do, Escape and F1, and F11, which is also the
fullscreen button there. **⋮** > **Keys** shows them on any width. They are the
game's own keys, read from your settings and shown again as the game writes
them; you change them in the game, in **Options** > **Control Setup**. Beside
forward and back the drawing shows the keys you strafe with when they are
letters, as WASD's are, and the ones you turn with when they are not.

The page shows the game at the shape of its video mode, 16:9 at 1280 by 720, and
**Options → Display Setup → Video mode** offers the modes from 640 by 480 to
1920 by 1200, at 4:3, 16:9 and 16:10. **Renderer setup** there sets the texture
filtering, which a new player has trilinear and anisotropic.

A player who has played before starts with their own settings, the keys and the
mouse as they set them in **Options**; WASD and the keys above are only the
first settings of a new one, who also starts with mouse aiming on, so the mouse
looks up and down, and with the middle button walking forward. Until you choose a
name in **Options → Player Setup**, you play under your account's.

## Saved games and settings

The game's **Save** and **Load** keep your games in your home, in
`.duke3d/<game>/`, one folder for each game you play, so a save is loaded only
into the game it was made in and nobody else sees it. Each save is a file,
`save0000.esv` on, as EDuke32 writes it. Beside them are `eduke32.cfg`, the
game's video, sound and mouse settings, and `settings.cfg`, its keys and its
console's settings, written as a menu of the game's closes, when you quit the
game, and when you go to another page of EiN's.

Without an account, saves and settings last only as long as the page.

## Add your own games

A game is a folder with a base GRP in it, as Duke's directory was: `DUKE3D.GRP`,
or any `duke3d*.grp`. Drop the GRP by itself anywhere on the Games page and it
is put in a folder of its own, named after the file; drop a folder with a
`DUKE3D.GRP` in it and it is uploaded as it is, GRP and all, under the folder's
name. Either appears as a card. The game reads everything in its folder, under
lower-case names, as it read its directory on DOS.

The Games page's **?** (**How to add games**) says the same in short
(`etc/games.d/help.md`).

- The whole game: the `DUKE3D.GRP` of your own Duke Nukem 3D 1.3D, or of the
  Atomic Edition 1.4 or 1.5. The list calls them **Duke Nukem 3D** and **Duke
  Nukem 3D: Atomic Edition**.
- The 20th Anniversary World Tour's GRP is not known to play here, and there is
  no multiplayer.

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

The sound effects are EDuke32's mixer, Apogee's grown up, mixing as the worker
asks for sound; Web Audio plays them. The music is played as on a Sound Blaster
16: the songs' MIDI through Apogee's AdLib driver into an OPL3, which
[DOSBox](https://www.dosbox.com/)'s DBOPL emulates, a quarter to a half of what
EDuke32's own Nuked OPL3 takes of the processor, for phones, and to the ear the
same; `mus_al_core 0` in the console plays it on Nuked OPL3 instead. With the
Roland SC-55 music pack (below) the songs are its recordings instead. **Options →
Sound Setup** sets the effects' and the music's volume.

## Packs

**Packs**, in the **⋮** menu of a game's page, lists add-on packs EDuke32's
players have made for Duke Nukem 3D, with what each is, who made it and its size:

- **Roland SC-55 music** (148 MB): the songs as a Roland Sound Canvas played
  them, recorded by MusicallyInspired.
- **PlayStation sounds** (7 MB): the sound effects in the PlayStation release's
  better quality, by Marphy Black.
- **Voxels** (4 MB): items, weapons and props as 3D voxel models, by ReaperMan,
  Borion and others; a game takes about 20 seconds more to start while the
  models are read and built.
- **Widescreen fixes** (1 MB): the status bar, the weapons and the menus drawn
  for widescreen, by NightFright and Fox.

Nothing is fetched until someone chooses to: **Fetch** (an administrator's, as
EiN lets only the administrator fetch from the internet) downloads a pack from
its makers' own site, checks it against its SHA-256 and keeps it in the games
folder's `.packs`, and **Remove** deletes it. Every game loads the packs fetched,
from the next time it starts, in the order listed, read from EiN as the game asks
for their files. Each pack's definitions, what its files replace, are
loaded as modules of their own, so packs do not hide each other's. In EiN's terminal `duke3d-packs` lists them and `duke3d-fetch
<pack>` (`sc55`, `psx`, `voxels`, `widescreen`) fetches one.

The High Resolution Pack is not offered: it is made for EDuke32's Polymer
renderer, which this build leaves out.

## Layout

| Path                                                                                               | What it is for                                                                                                                                                                                                                                        |
| -------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `mod.json`                                                                                         | The manifest: the games module it needs, the route of a game's page, and the engine worker.                                                                                                                                                           |
| `etc/games.d/games.json`                                                                           | What the games module knows of the module: its title, icon and route, its list and fetch commands, its games folder, and the files it takes when dropped.                                                                                             |
| `etc/games.d/help.md`                                                                              | How to add games and user maps, which the Games page's **?** shows.                                                                                                                                                                                   |
| `ui/pages/game.tsx`                                                                                | A game's page, `/games/duke3d/<game>`: the games module's page for a game, with the module's adapter.                                                                                                                                                 |
| `ui/adapter.ts`                                                                                    | The adapter the games module plays the games with: lists them, starts one, and names the saves kept.                                                                                                                                                  |
| `ui/duke3d/engine.ts`                                                                              | Starts a game in the engine worker: its build, its GRP read as the game asks, its other files and the saves whole, and the keys and mouse as it takes them.                                                                                           |
| `ui/duke3d/engine-worker.ts`                                                                       | The engine worker (`ui.workers`): runs the game a tick at a time under ASYNCIFY, and hands the page the frames it draws on the worker's canvas, its sound and the files it writes.                                                                    |
| `ui/duke3d/start.ts`                                                                               | What the page starts the worker with, and where the game's files are in the engine's file system; free of the DOM, as all the worker imports is.                                                                                                      |
| `ui/duke3d/keys.ts`                                                                                | Browser key codes to the PC keyboard's scancodes, and the characters they type.                                                                                                                                                                       |
| `ui/duke3d/pad.tsx`                                                                                | The keys' drawing the games page shows: the player's keys on the keyboard and the mouse, read from their settings.cfg and eduke32.cfg.                                                                                                                |
| `bin/duke3d-games`                                                                                 | Lists the games with their titles, title screens, GRPs, files and user maps, and the free one not fetched yet, for the Games page and a game's page.                                                                                                  |
| `lib/games.ts`, `lib/api.ts`                                                                       | Where the games are, the editions known by their GRP, the free game offered, and the shape `duke3d-games` answers in.                                                                                                                                 |
| `lib/catalog.ts`                                                                                   | Reads the games folder: each game's GRP, files and user maps, and what `duke3d-games` says of it.                                                                                                                                                     |
| `lib/cover.ts`                                                                                     | Decodes a game's title screen out of its GRP, reading only the parts it needs, as a PNG.                                                                                                                                                              |
| `bin/duke3d-fetch`                                                                                 | Fetches the free game into the games folder, or an add-on pack into the packs folder, checked against its SHA-256.                                                                                                                                    |
| `bin/duke3d-packs`, `lib/packs.ts`                                                                 | The add-on packs the module offers, where each is fetched from, and which are fetched.                                                                                                                                                                |
| `ui/duke3d/packs.tsx`                                                                              | Packs, the dialog ⋮ opens on a game's page: the packs, Fetch and Remove.                                                                                                                                                                              |
| `engine/files/source/build/src/einlayer.cpp`                                                       | EDuke32's platform layer for the engine worker, in place of SDL's: WebGL 2 on the worker's canvas, the video modes, keys, characters, mouse and wheel, and the waits for its ticks.                                                                   |
| `engine/files/source/audiolib/src/driver_ein.cpp`                                                  | The sound card: the mixer, the music in it, run as the worker asks for sound.                                                                                                                                                                         |
| `engine/files/source/duke3d/src/game_ein.cpp`                                                      | What the worker asks of the game itself: whether a menu or the console is up, its settings written, and the files it writes handed to the page.                                                                                                       |
| `engine/files/source/build/src/glbuild_ein.cpp`, `engine/files/source/build/include/glbuild_ein.h` | The fixed-function OpenGL Polymost, the 2D drawing and the classic renderer use, on WebGL 2: matrix stacks, immediate mode and client arrays, fog and the alpha test, texture formats WebGL takes, and draws merged with only the changed state sent. |
| `engine/files/source/build/src/polymost1Es3Vert.glsl`, `polymost1Es3Frag.glsl`                     | Polymost's shader in GLSL ES 3.00, reading the layer's matrices, colour, fog and alpha test.                                                                                                                                                          |
| `engine/files/platform/emscripten/ein-library.js`                                                  | The worker's calls the platform layer makes into JavaScript.                                                                                                                                                                                          |
| `engine/webgl.patch`                                                                               | Builds Polymost on WebGL 2 through the layer: its shaders, the `r_glbatch` setting that turns merging off, and the frame drawn before it is shown.                                                                                                    |
| `engine/ein.patch`                                                                                 | The build for the engine worker (`EIN=1`), its exports, a new player's settings, frames paced by the worker's ticks, a GRP known by its size, music packs' OGG files, and the AdLib music on DOSBox's DBOPL (`mus_al_core`).                          |
| `engine/Containerfile`, `engine/build.sh`                                                          | The build: pinned emsdk image, eduke32-wasm at the commit it pins with the patches and files above, and the make command.                                                                                                                             |
| `assets/icon.svg`                                                                                  | A radiation trefoil, as Duke's menus have them, drawn for the module: its icon on the Games page, filled with the text colour.                                                                                                                        |
| `assets/eduke32.js`, `assets/eduke32.wasm`                                                         | The built engine, committed, since a module is installed as its repository stands. `assets/eduke32-build.txt` says what built them.                                                                                                                   |
| `scripts/build-engine.sh`                                                                          | Rebuilds `assets/eduke32.js` and `assets/eduke32.wasm`.                                                                                                                                                                                               |
| `test/smoke.mjs`                                                                                   | Signs in with headless Chrome, plays a little, saves screenshots.                                                                                                                                                                                     |
| `deno.jsonc`                                                                                       | Generated from `mod.json` by core's `deno task sync:modules`, for type-checking in a core checkout.                                                                                                                                                   |

## Building the engine

Needs only docker with buildx, or podman; nothing is installed on the host.

    scripts/build-engine.sh

It builds `engine/Containerfile` and exports the result into `assets/` with the
local exporter. The source of the committed `assets/eduke32.wasm` is
DigitalCyberSoft's eduke32-wasm at the commit `engine/Containerfile` pins, with
DOSBox's `dbopl.cpp` and `dbopl.h` at the commit it pins, `engine/webgl.patch`
and `engine/ein.patch` applied and the files in `engine/files/` added, built with
`make PLATFORM=EMSCRIPTEN EIN=1`.

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

EDuke32's code, Apogee's sound code, DOSBox's DBOPL and this module's own are
GPL-2.0-or-later: see `LICENSE`. The BUILD engine is Ken Silverman's, under his
BUILD licence, which allows its use only free of charge: see `BUILDLIC.TXT`.

    // "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
    // Ken Silverman's official web site: "http://www.advsys.net/ken"
    // See the included license file "BUILDLIC.TXT" for license info.

Duke Nukem 3D's source is © 3D Realms and Apogee Software, EDuke32's © its
authors (voidpoint), eduke32-wasm's © DigitalCyberSoft; their notices, and those
of the libraries EDuke32 brings, are in the source the build fetches.
`assets/eduke32.js` is Emscripten's runtime, under the MIT licence.
`assets/icon.svg` is the module's own drawing of a radiation
trefoil. The module fetches and does not ship the game: the shareware episode is
3D Realms', under its own licence, and Duke Nukem 3D is a trademark of Gearbox
Software.
