/**
 * The module's engine worker, run by the games module's kit (`serveEngine`): EDuke32, Emscripten's
 * build in `assets/`, with the game's GRP read from EiN a piece at a time as it asks.
 *
 * The game keeps its own loops, so it runs under ASYNCIFY (`engine/einlayer.cpp`): whenever
 * it has shown a frame, or has polled for a frame's time, it waits for its next tick
 * (`Module.einFrame`), and a tick lets it run until it waits again. Between ticks the keys, the
 * mouse and the kit's other calls reach it. It draws with WebGL 2 on the worker's canvas, Polymost's
 * true colour or the classic renderer's palette alike, and the canvas is taken as the frame.
 */
import { type EmscriptenFS, type EmscriptenModule, type Engine, exitStatus, FrameCanvas, heapSound, serveEngine, startEmscripten } from '@games/engine';
import { type DukeStart, GAME_DIR } from './start.ts';

/** What the engine's platform layer exports (`engine/`), beside Emscripten's own. */
type DukeModule = EmscriptenModule & {
  FS: EmscriptenFS & { chdir(path: string): void };
  callMain(args: string[]): number;
  _ein_key(pressed: number, scancode: number): void;
  _ein_char(character: number): void;
  _ein_mouse(dx: number, dy: number): void;
  _ein_mouse_button(pressed: number, button: number): void;
  _ein_wheel(up: number): void;
  _ein_write_config(): void;
  _ein_menu(): number;
  _ein_frames(): number;
  _ein_audio_rate(rate: number): void;
  _ein_audio_render(at: number, frames: number): void;
};

let duke: DukeModule;
const frames = new FrameCanvas(1280, 720);
let drawn = 0;
let sound: ((count: number) => Int16Array<ArrayBuffer>) | undefined;
/** Lets the game, waiting for its next tick, run on; nothing while it runs or once it has ended. */
let waiting: (() => void) | undefined;
/** Ends the tick running, once the game waits again or has ended. */
let ticking: (() => void) | undefined;
/** Ends the game, once it has started. */
let end: ((error?: unknown) => void) | undefined;

const ticked = () => {
  const done = ticking;
  ticking = undefined;
  done?.();
};

// The game fails inside a tick in a promise of ASYNCIFY's, which nothing awaits: it ends the game.
addEventListener('unhandledrejection', (event) => {
  if (!end) return;
  event.preventDefault();
  end(event.reason);
});

const engine: Engine<DukeStart> = {
  async boot(boot) {
    // The game ends on its own in a tick, on Quit with exit(0) or as it fails: the page hears it,
    // and the tick is over.
    end = (error) => {
      waiting = undefined;
      boot.end(error);
      ticked();
    };
    const ending = end;
    // What the game does as it starts, for the page's spinner, read from the lines it prints: its
    // definitions, the packs' among them, one file after another, then the game itself.
    const definitions = 1 + boot.payload.args.filter((arg) => arg === '-mh').length;
    let read = 0;
    const said = (line: string) => {
      if (/ Loading (duke3d\.def|module )/.test(line)) {
        read++;
        boot.progress({ label: definitions > 1 ? "Reading the packs' definitions" : 'Reading the definitions', fraction: Math.min(1, read / definitions) });
      } else if (/ Initializing console/.test(line)) boot.progress({ label: 'Starting the game', fraction: 1 });
      boot.log(line);
    };
    boot.progress({ label: 'Starting Duke Nukem 3D' });
    duke = await startEmscripten<DukeModule>({ ...boot, log: said }, boot.payload.build, {
      canvas: frames.canvas,
      // The game reads its GRP in many small pieces as it starts: small chunks fetch less of it.
      chunk: 256 * 1024,
      // The game writes its saves and its settings in its directory, by their names.
      written: (path) => {
        const name = path.split('/').at(-1) ?? path;
        return { path: `${GAME_DIR}/${name}`, name };
      },
      module: {
        einFrame: () =>
          new Promise<void>((resume) => {
            waiting = resume;
            ticked();
          }),
        onExit: (status: number) => ending(exitStatus(status)),
        onAbort: (what: unknown) => ending(what instanceof Error ? what : new Error(`the engine aborted: ${String(what)}`)),
      },
    });
    duke.FS.mkdirTree(GAME_DIR);
    duke.FS.chdir(GAME_DIR);
    if (boot.sampleRate) {
      duke._ein_audio_rate(boot.sampleRate);
      sound = heapSound(duke, (at, count) => duke._ein_audio_render(at, count));
    }
    // main runs until the game first waits for a tick.
    try {
      duke.callMain(boot.payload.args);
    } catch (error) {
      ending(error);
    }
  },
  tick() {
    const resume = waiting;
    if (!resume) return;
    waiting = undefined;
    return new Promise<void>((done) => {
      ticking = done;
      resume();
    });
  },
  frame() {
    if (duke._ein_frames() === drawn) return undefined;
    drawn = duke._ein_frames();
    return frames.take();
  },
  sound: (count) => sound?.(count),
  key: (pressed, scancode) => duke._ein_key(pressed ? 1 : 0, scancode),
  char: (character) => duke._ein_char(character),
  mouse: (dx, dy) => duke._ein_mouse(dx, dy),
  button: (pressed, button) => duke._ein_mouse_button(pressed ? 1 : 0, button),
  input(data) {
    const { wheel } = data as { wheel?: boolean };
    if (wheel !== undefined) duke._ein_wheel(wheel ? 1 : 0);
  },
  menu: () => duke._ein_menu() !== 0,
  keep: () => duke._ein_write_config(),
};

serveEngine(engine);
