import { execCommand } from '@ein/ui/signals';
import type { GameAdapter, Written } from '@games/adapter';
import type { Game } from '../lib/api.ts';
import { startDuke } from './duke3d/engine.ts';
import { PacksDialog } from './duke3d/packs.tsx';
import { DukePad, heardKept, heardWritten } from './duke3d/pad.tsx';

/** The files a player keeps of a game, in their saves folder of it: its saves (`save0000.esv` on), and its settings, `eduke32.cfg` and `settings.cfg`, the keys' binds among them. */
const KEPT = /^(save\d+\.esv|eduke32\.cfg|settings\.cfg)$/;

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
  help: 'The mouse aims and fires, the right button opens, the wheel changes weapon, WASD moves, Space jumps, C crouches, E opens, Shift runs, Escape is the menu.',
  // The player's keys on the keyboard and the mouse, which the games page shows where a keyboard
  // plays, beside the menu's and the help's keys.
  keys: {
    Pad: DukePad,
    shortcuts: [
      { key: 'Esc', icon: 'menu-2', label: 'Menu' },
      { key: 'F1', icon: 'help', label: 'Help' },
    ],
  },
  // The add-on packs to fetch, which every game loads.
  dialogs: [{ label: 'Packs', icon: 'package', has: (game) => game.state === 'installed', Dialog: PacksDialog }],
  pointer: true,
  kept: { pattern: KEPT, depth: 0 },
  startsOnMap: true,
};
