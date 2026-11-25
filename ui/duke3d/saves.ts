import { homePath } from '@ein/paths';
import { fileUrl, type NodeChildEntry, shq } from '@ein/ui/lib';
import { execCommand } from '@ein/ui/signals';

/**
 * Where `username` keeps what they have of `game`: its saved games and their `duke3d.cfg`, the
 * game's settings and key bindings. A folder of their home's, one for each game, so a save is only
 * ever loaded into the game it was made in. Nobody else sees it.
 */
export const savesFolder = (username: string, game: string): string => `${homePath(username)}/.duke3d/${game}`;

/** One of the files the game keeps: its name, `game0.sav` or `duke3d.cfg`, and its bytes. */
export type Kept = { name: string; data: Uint8Array<ArrayBuffer> };

/** The game's saves, `game0.sav` to `game9.sav` for its ten slots, and its `duke3d.cfg`. */
const KEPT_FILE = /^(game\d\.sav|duke3d\.cfg)$/;

/** The files in `folder`, which is made when it is not there yet, so a first save has somewhere to go. */
export async function loadKept(folder: string): Promise<Kept[]> {
  await execCommand(`mkdir -p ${shq(folder)}`);
  const entries = await execCommand<NodeChildEntry[]>(`ls -l ${shq(folder)}`);
  const names = entries.map((entry) => entry.name).filter((name) => KEPT_FILE.test(name));
  return await Promise.all(names.map(async (name) => {
    const response = await fetch(fileUrl(`${folder}/${name}`));
    if (!response.ok) throw new Error(`${name}: ${response.status} ${response.statusText}`);
    return { name, data: new Uint8Array(await response.arrayBuffer()) };
  }));
}

/**
 * The most a write may send and still be finished by the browser after the page has gone (`keepalive`,
 * which allows 64 KiB in flight): duke3d.cfg, which the page writes as it goes, is a few.
 */
const KEEPALIVE_MAX = 60 * 1024;

/**
 * Keep what the game writes in `folder`, one file after the other, in the order the game wrote them,
 * so a slot saved twice in quick succession ends with the later save. `onError` hears a file that
 * could not be kept. `loadKept` made the folder, so a file goes out at once.
 */
export function keeper(folder: string, onError: (error: unknown) => void): (file: Kept) => void {
  let queue = Promise.resolve();
  const keep = async ({ name, data }: Kept) => {
    // PUT stores the body at the path as it is, making the file or replacing it.
    const response = await fetch(fileUrl(`${folder}/${name}`), { method: 'PUT', body: data, keepalive: data.length <= KEEPALIVE_MAX });
    if (!response.ok) throw new Error(`${name}: ${response.status} ${response.statusText}`);
  };
  return (file) => {
    queue = queue.then(() => keep(file)).then(() => undefined, onError);
  };
}
