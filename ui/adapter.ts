import { execCommand } from '@ein/ui/signals';
import type { GameAdapter, Written } from '@games/adapter';
import type { Game } from '../lib/api.ts';
import { startDuke } from './duke3d/engine.ts';
import { DukePad, heardKept, heardWritten } from './duke3d/pad.tsx';

/** The files a player keeps of a game, in their saves folder of it: `game0.sav` to `game9.sav`, the game's ten slots, and `duke3d.cfg`. */
const KEPT = /^(game\d\.sav|duke3d\.cfg)$/;

/** Duke Nukem 3D on the games module's page for a game and its dashboard card. */
export const adapter: GameAdapter<Game> = {
  module: 'duke3d',
  title: 'Duke Nukem 3D',
  list: () => execCommand<Game[]>('duke3d-games'),
  async start({ game, onKeep, ...options }) {
    if (game.state !== 'installed') throw new Error(`${game.title} is not here yet`);
    // The keys' drawing (`DukePad`) shows the keys the game starts with and those it writes.
    heardKept(game, options.kept);
    const keep = onKeep && ((written: Written) => (heardWritten(game, written), onKeep(written)));
    return await startDuke({ game, onKeep: keep, ...options });
  },
  help: 'The mouse aims and fires, the wheel changes weapon, the arrows move, Ctrl fires, Space opens, A jumps, Z crouches, Shift runs, F1 is the help, Escape the menu.',
  // The player's keys on the keyboard and the mouse, which the games page shows where a keyboard
  // plays, beside the menu's and the help's keys.
  keys: {
    Pad: DukePad,
    shortcuts: [
      { key: 'Esc', icon: 'menu-2', label: 'Menu' },
      { key: 'F1', icon: 'help', label: 'Help' },
    ],
  },
  pointer: true,
  kept: { pattern: KEPT, depth: 0 },
  startsOnMap: true,
  // 320 by 200 was shown at 4:3, as every other mode the game draws in is.
  frame: { width: 640, height: 480 },
};
