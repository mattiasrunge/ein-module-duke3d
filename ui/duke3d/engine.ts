import { fileUrl } from '@ein/ui/lib';
import type { StartOptions } from '@games/adapter';
import type { InstalledGame } from '../../lib/api.ts';

/** Where the module's built engine sits in EiN's tree. */
const ASSETS = '/usr/modules/duke3d/assets';

/** Where the game's files are in Emscripten's file system: its working directory, as Duke's own was. */
const GAME_DIR = '/duke3d';

/** What `assets/duke3d.js` (Emscripten's glue, `engine/build.sh`) gives back once the wasm runs. */
type DukeModule = {
  FS: {
    writeFile(path: string, data: Uint8Array): void;
    readFile(path: string): Uint8Array<ArrayBuffer>;
    mkdir(path: string): void;
    chdir(path: string): void;
  };
  HEAPU8: Uint8Array;
  HEAP16: Int16Array;
  callMain(args: string[]): number;
  _malloc(bytes: number): number;
  _ein_key(pressed: number, scancode: number): void;
  _ein_mouse(dx: number, dy: number): void;
  _ein_mouse_button(pressed: number, bit: number): void;
  _ein_write_config(): void;
  _ein_screen(): number;
  _ein_width(): number;
  _ein_height(): number;
  _ein_frames(): number;
  _ein_audio_rate(rate: number): void;
  _ein_audio_render(buffer: number, frames: number): void;
};

/** How a game ended: the status it exited with, 0 when the player quit, or the error it stopped on. */
export type End = { status: number } | { error: unknown };

/** A running game: it runs itself, a turn at a time; the page hands it frames, keys and the mouse. */
type Engine = {
  /** The frame's size, which the game's video options may change. */
  width(): number;
  height(): number;
  /** How many frames the game has drawn: a new number means a new frame to copy. */
  frames(): number;
  /** Copy the current frame into `image`, which is `width()` by `height()`. */
  draw(image: ImageData): void;
  /**
   * Give the game its next turn, which it takes once the page has drawn: it runs until it has
   * finished its next frame, or has waited for a frame's time.
   */
  turn(): void;
  key(pressed: boolean, scancode: number): void;
  mouse(dx: number, dy: number): void;
  mouseButton(pressed: boolean, bit: number): void;
  /** Write duke3d.cfg, so the player's settings are kept when the page stops the game. */
  writeConfig(): void;
};

/** Fetch a file as bytes; `/file/` needs the session's cookie, which a same-origin fetch sends. */
async function fetchFile(path: string): Promise<Uint8Array> {
  const response = await fetch(fileUrl(path));
  if (!response.ok) throw new Error(`${path}: ${response.status} ${response.statusText}`);
  return new Uint8Array(await response.arrayBuffer());
}

/**
 * Load the engine and `game`'s files and start the game. `log` gets what the game prints, line by
 * line, and `sound` its sound effects and music, which the game mixes together; without it the game
 * is silent. `kept` are the player's saves and duke3d.cfg of `game`, which the game's Load menu
 * lists and its start-up reads, and `onKeep` hears each of them the game writes. With `map`, one of
 * the game's user maps, the game starts on it, as `-map` on its command line does. `onEnd` hears
 * the game end.
 *
 * The game runs under ASYNCIFY (`engine/display_ein.c`): whenever it has finished a frame, or has
 * polled for a frame's time, it waits for its next turn, which `turn` gives it. Until then the page
 * has the thread; a game given no more turns stands still and is dropped with the page.
 *
 * The glue is imported from a URL held in a variable, so core's bundler leaves it alone and the
 * browser loads it at runtime. The wasm is fetched here and handed over as bytes, so how `/file/`
 * labels a `.wasm` does not matter.
 */
export async function startEngine(
  { game, sound, log, kept, onKeep, map, onEnd }: Pick<StartOptions<InstalledGame>, 'game' | 'sound' | 'log' | 'kept' | 'onKeep' | 'map'> & { onEnd(end: End): void },
): Promise<Engine> {
  const glueUrl = fileUrl(`${ASSETS}/duke3d.js`);
  const [glue, wasmBinary, files] = await Promise.all([
    import(glueUrl) as Promise<{ default: (options: object) => Promise<DukeModule> }>,
    fetchFile(`${ASSETS}/duke3d.wasm`),
    Promise.all(game.files.map(async (name) => ({ name, data: await fetchFile(`${game.path}/${name}`) }))),
  ]);

  // The game's turn, waiting for the page's next one (`einFrame`, display_ein.c).
  let waiting: (() => void) | undefined;
  let ended = false;
  const end = (outcome: End) => {
    if (ended) return;
    ended = true;
    waiting = undefined;
    onEnd(outcome);
  };
  // Called by `ein_wrote` once the game has written the file; `duke` is set by then.
  const einWrote = (path: string) => {
    const name = path.split('/').at(-1) ?? path;
    onKeep?.({ files: [{ name, data: duke.FS.readFile(`${GAME_DIR}/${name}`) }] });
  };
  const duke = await glue.default({
    wasmBinary,
    print: log,
    printErr: log,
    einFrame: () => new Promise<void>((resolve) => waiting = resolve),
    einWrote,
    onExit: (status: number) => end({ status }),
    onAbort: (what: unknown) => end({ error: what instanceof Error ? what : new Error(String(what)) }),
  });

  // The game's directory, as Duke's was on DOS: its files, by the lower-case names the game opens
  // them by, whatever they are called here, then the player's saves and duke3d.cfg beside them.
  duke.FS.mkdir(GAME_DIR);
  for (const { name, data } of files) duke.FS.writeFile(`${GAME_DIR}/${name.toLowerCase()}`, data);
  for (const file of kept) duke.FS.writeFile(`${GAME_DIR}/${file.name}`, file.data);
  duke.FS.chdir(GAME_DIR);

  if (sound) duke._ein_audio_rate(sound.context.sampleRate);

  // The effects and the music: interleaved 16-bit stereo from the game's mixer, into a buffer kept
  // for them.
  let mixFrames = 0;
  let mixBuffer = 0;
  sound?.effects((left, right) => {
    if (ended) return;
    if (left.length > mixFrames) {
      mixFrames = left.length;
      mixBuffer = duke._malloc(mixFrames * 4);
    }
    duke._ein_audio_render(mixBuffer, left.length);
    // HEAP16 is looked up each time: memory growth replaces it.
    const samples = duke.HEAP16.subarray(mixBuffer >> 1, (mixBuffer >> 1) + left.length * 2);
    for (let i = 0; i < left.length; i++) {
      left[i] = samples[i * 2] / 32768;
      right[i] = samples[i * 2 + 1] / 32768;
    }
  });

  // The game runs until its first wait, then on its turns.
  try {
    duke.callMain(map ? ['-map', `${map.toLowerCase()}.map`] : []);
  } catch (error) {
    end({ error });
  }

  // Calls into a game that has ended are dropped: its memory is gone with it.
  const live = <T>(call: () => T): T | undefined => ended ? undefined : call();
  return {
    width: () => live(() => duke._ein_width()) ?? 0,
    height: () => live(() => duke._ein_height()) ?? 0,
    frames: () => live(() => duke._ein_frames()) ?? 0,
    draw(image) {
      live(() => {
        // The engine keeps the frame as R, G, B, A bytes already. The heap is looked up each time:
        // memory growth replaces it.
        const at = duke._ein_screen();
        image.data.set(duke.HEAPU8.subarray(at, at + image.width * image.height * 4));
      });
    },
    turn() {
      const resume = waiting;
      waiting = undefined;
      resume?.();
    },
    key: (pressed, scancode) => live(() => duke._ein_key(pressed ? 1 : 0, scancode)),
    mouse: (dx, dy) => live(() => duke._ein_mouse(dx, dy)),
    mouseButton: (pressed, bit) => live(() => duke._ein_mouse_button(pressed ? 1 : 0, bit)),
    writeConfig: () => live(() => duke._ein_write_config()),
  };
}
