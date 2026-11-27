import { fileUrl } from '@ein/ui/lib';
import type { RunningGame, StartOptions } from '@games/adapter';
import { fetchFile, startWorkerGame } from '@games/worker';
import type { InstalledGame } from '../../lib/api.ts';
import { mouseBit, scancode, WHEEL_DOWN, WHEEL_UP } from './keys.ts';
import { type DukeStart, GAME_DIR } from './start.ts';

/** Where the module's built engine sits in EiN's tree. */
const ASSETS = '/usr/modules/duke3d/assets';

/** The files read from EiN as the game asks for them, not fetched whole: the GRPs, the game's data, up to 44 MB of it. */
const LAZY = /\.grp$/i;

/**
 * Start `game` in the module's engine worker (`engine-worker.ts`). Its files are in the game's
 * directory, by the lower-case names the game opens them by, whatever they are called here: its GRPs
 * read from EiN as the game asks for them, the rest fetched whole, then the player's saves and
 * duke3d.cfg of `game` beside them, which the game's Load menu lists and its start-up reads;
 * `onKeep` hears each of them the game writes. The game mixes its sound effects and its music
 * together into `sound`; without it the game is silent. With `map`, one of the game's user maps, the
 * game starts on it, as `-map` on its command line does.
 */
export async function startDuke(
  { game, kept, map, sound, log, onKeep }: Pick<StartOptions<InstalledGame>, 'game' | 'kept' | 'map' | 'sound' | 'log' | 'onKeep'>,
): Promise<RunningGame> {
  const at = (name: string) => `${GAME_DIR}/${name.toLowerCase()}`;
  const whole = await Promise.all(game.files.filter((name) => !LAZY.test(name)).map(async (name) => ({ path: at(name), data: await fetchFile(`${game.path}/${name}`) })));
  return await startWorkerGame<DukeStart>('duke3d', 'engine', {
    payload: {
      build: { glue: fileUrl(`${ASSETS}/duke3d.js`), wasm: fileUrl(`${ASSETS}/duke3d.wasm`) },
      args: map ? ['-map', `${map.toLowerCase()}.map`] : [],
    },
    lazy: game.files.filter((name) => LAZY.test(name)).map((name) => ({ path: at(name), url: fileUrl(`${game.path}/${name}`) })),
    files: [...whole, ...kept.map((file) => ({ path: `${GAME_DIR}/${file.name}`, data: file.data }))],
    controls: (post) => ({
      key(pressed, code) {
        const key = scancode(code);
        if (key !== undefined) post({ type: 'key', pressed, key });
        return key !== undefined;
      },
      button(pressed, button) {
        const bit = mouseBit(button);
        if (bit !== undefined) post({ type: 'button', pressed, button: bit });
        return bit !== undefined;
      },
      // The wheel is a click that only goes down, as BUILD read it.
      wheel: (up) => post({ type: 'button', pressed: true, button: up ? WHEEL_UP : WHEEL_DOWN }),
    }),
  }, { sound, log, onKeep });
}
