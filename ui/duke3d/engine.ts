import { fileUrl } from '@ein/ui/lib';
import { execCommand } from '@ein/ui/signals';
import type { RunningGame, StartOptions } from '@games/adapter';
import { fetchFile, startWorkerGame } from '@games/worker';
import type { InstalledGame, Pack } from '../../lib/api.ts';
import { character, scancode } from './keys.ts';
import { type DukeStart, GAME_DIR } from './start.ts';

/** Where the module's built engine sits in EiN's tree. */
const ASSETS = '/usr/modules/duke3d/assets';

/** The files read from EiN as the game asks for them, not fetched whole: the GRPs, the game's data, up to 44 MB of it. */
const LAZY = /\.grp$/i;

/** Where the packs fetched are (`lib/packs.ts`), and where the game finds them. */
const PACKS = '/groups/users/duke3d/.packs';
const AUTOLOAD = `${GAME_DIR}/autoload`;

/**
 * Start `game` in the module's engine worker (`engine-worker.ts`). Its files are in the game's
 * directory, by the lower-case names the game opens them by, whatever they are called here: its GRPs
 * read from EiN as the game asks for them, the rest fetched whole, then the player's saves and
 * settings of `game` beside them, which the game's Load menu lists and its start-up reads;
 * `onKeep` hears each of them the game writes. The game mixes its sound effects and its music
 * together into `sound`; without it the game is silent. With `map`, one of the game's user maps, the
 * game starts on it, as `-map` on its command line does. The add-on packs fetched (`duke3d-packs`)
 * are read from EiN as the game asks too, each named on its command line with `-g`, in the order
 * they are listed, so a later one's files win, and its definitions files with `-mh`. `name`, the
 * player's account, names a player whose settings name no one yet (`-name`).
 */
export async function startDuke(
  { game, kept, map, name, sound, log, onKeep }: Pick<StartOptions<InstalledGame>, 'game' | 'kept' | 'map' | 'name' | 'sound' | 'log' | 'onKeep'>,
): Promise<RunningGame> {
  const at = (name: string) => `${GAME_DIR}/${name.toLowerCase()}`;
  const whole = await Promise.all(game.files.filter((name) => !LAZY.test(name)).map(async (name) => ({ path: at(name), data: await fetchFile(`${game.path}/${name}`) })));
  // A visitor by a share link reads no more than the game: no packs then.
  const packs = (await execCommand<Pack[]>('duke3d-packs').catch(() => [])).filter((pack) => pack.fetched);
  let shift = false;
  return await startWorkerGame<DukeStart>('duke3d', 'engine', {
    payload: {
      build: { glue: fileUrl(`${ASSETS}/eduke32.js`), wasm: fileUrl(`${ASSETS}/eduke32.wasm`) },
      // Its files and the player's in the working directory, and the game's GRP by name, where a
      // folder could hold more than one.
      args: [
        '-usecwd',
        '-gamegrp',
        game.grp.toLowerCase(),
        ...packs.flatMap((pack) => ['-g', `autoload/${pack.file}`]),
        ...packs.flatMap((pack) => (pack.defs ?? []).flatMap((def) => ['-mh', def])),
        ...(name ? ['-name', name] : []),
        ...(map ? ['-map', `${map.toLowerCase()}.map`] : []),
      ],
    },
    lazy: [
      ...game.files.filter((name) => LAZY.test(name)).map((name) => ({ path: at(name), url: fileUrl(`${game.path}/${name}`) })),
      ...packs.map((pack) => ({ path: `${AUTOLOAD}/${pack.file}`, url: fileUrl(`${PACKS}/${pack.file}`), size: pack.size })),
    ],
    files: [...whole, ...kept.map((file) => ({ path: `${GAME_DIR}/${file.name}`, data: file.data }))],
    controls: (post) => ({
      // Each key goes as the PC's scancode, and the character it types with it, for the console's
      // and the menus' fields.
      key(pressed, code) {
        if (code === 'ShiftLeft' || code === 'ShiftRight') shift = pressed;
        const key = scancode(code);
        if (key === undefined) return false;
        post({ type: 'key', pressed, key });
        const typed = pressed ? character(code, shift) : undefined;
        if (typed) post({ type: 'char', character: typed.charCodeAt(0) });
        return true;
      },
      // The browser's numbering: 0 left, 1 middle, 2 right, 3 back, 4 forward.
      button(pressed, button) {
        if (button > 4) return false;
        post({ type: 'button', pressed, button });
        return true;
      },
      wheel: (up) => post({ type: 'input', data: { wheel: up } }),
    }),
  }, { sound, log, onKeep });
}
