import { execCommand } from '@ein/ui/signals';
import type { GameAdapter } from '@games/adapter';
import type { Game } from '../lib/api.ts';
import { type End, startEngine } from './duke3d/engine.ts';
import { mouseBit, scancode, WHEEL_DOWN, WHEEL_UP } from './duke3d/keys.ts';

/** The files a player keeps of a game, in their saves folder of it: `game0.sav` to `game9.sav`, the game's ten slots, and `duke3d.cfg`. */
const KEPT = /^(game\d\.sav|duke3d\.cfg)$/;

/** The game's end as `step` throws it: Emscripten's `ExitStatus` for an exit, which `quitted` knows for Quit's 0. */
class ExitStatus extends Error {
  override name = 'ExitStatus';
  constructor(readonly status: number) {
    super(`The game exited with status ${status}`);
  }
}

/** Duke Nukem 3D on the games module's page for a game and its dashboard card. */
export const adapter: GameAdapter<Game> = {
  module: 'duke3d',
  title: 'Duke Nukem 3D',
  list: () => execCommand<Game[]>('duke3d-games'),
  async start({ game, ...options }) {
    if (game.state !== 'installed') throw new Error(`${game.title} is not here yet`);
    // The game ends on its own turn, which `step` gave it; the next `step` says so.
    let ended: End | undefined;
    const engine = await startEngine({ game, ...options, onEnd: (end) => ended = end });
    const press = (pressed: boolean, key: number | undefined, to: (pressed: boolean, key: number) => void): boolean => {
      if (key === undefined) return false;
      to(pressed, key);
      return true;
    };
    return {
      width: engine.width,
      height: engine.height,
      frames: engine.frames,
      draw: engine.draw,
      step() {
        if (ended) throw 'status' in ended ? new ExitStatus(ended.status) : ended.error;
        engine.turn();
      },
      key: (pressed, code) => press(pressed, scancode(code), engine.key),
      button: (pressed, button) => press(pressed, mouseBit(button), engine.mouseButton),
      mouse: engine.mouse,
      // The wheel is a click that only goes down, as BUILD read it.
      wheel: (up) => engine.mouseButton(true, up ? WHEEL_UP : WHEEL_DOWN),
      keep: engine.writeConfig,
    };
  },
  help: 'The mouse aims and fires, the arrows move, Ctrl fires, Space opens, A jumps, Z crouches, Shift runs, F1 is the help, Escape the menu.',
  pointer: true,
  kept: { pattern: KEPT, depth: 0 },
  startsOnMap: true,
  // 320 by 200 was shown at 4:3, as every other mode the game draws in is.
  frame: { width: 640, height: 480 },
};
