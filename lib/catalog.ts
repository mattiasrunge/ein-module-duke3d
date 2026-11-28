import type { Command, NodeAttrs } from '@ein/client';
import type { Game, InstalledGame } from './api.ts';
import { coverOf } from './cover.ts';
import { byRank, descriptionOf, GAMES, GRP_FILE, isShareware, MAP_FILE, OFFERS, titleOf } from './games.ts';

/** `bytes` as a person reads a size: `11 MB`. */
const sizeText = (bytes: number): string =>
  bytes >= 1e9 ? `${(bytes / 1e9).toFixed(1)} GB` : bytes >= 1e6 ? `${Math.round(bytes / 1e6)} MB` : `${Math.max(1, Math.round(bytes / 1e3))} kB`;

/** The game in the folder `name` of `GAMES`, as `duke3d-games` lists it; undefined when it is no game, a folder without a base GRP. */
async function readGame(cmd: Command, name: string): Promise<InstalledGame | undefined> {
  const path = `${GAMES}/${name}`;
  const children = await cmd.sys.vfs.children<NodeAttrs & { size?: number }>(cmd.access, path).catch(() => []);
  const files = children.filter((child) => child.node.type !== 'directory').map((child) => ({ name: child.name, size: child.node.attrs.size ?? 0 }));
  // The game plays the first base GRP by name (`-gamegrp`); the list names the folder by it.
  const grp = files.filter((file) => GRP_FILE.test(file.name)).sort((a, b) => a.name.localeCompare(b.name))[0];
  if (!grp) return undefined;
  const size = files.reduce((sum, file) => sum + file.size, 0);
  const maps = isShareware(grp.size) ? [] : files.map((file) => MAP_FILE.exec(file.name)?.[1]).filter((map) => map !== undefined).sort();
  const cover = await coverOf(cmd, `${path}/${grp.name}`).catch(() => undefined);
  const mapsText = maps.length === 0 ? '' : maps.length === 1 ? ' · 1 user map' : ` · ${maps.length} user maps`;
  const title = titleOf(name, grp.size);
  const description = descriptionOf(title);
  return {
    id: name,
    title,
    state: 'installed',
    path,
    detail: `${grp.name} · ${sizeText(size)}${mapsText}`,
    ...(description ? { description } : {}),
    ...(cover ? { cover } : {}),
    grp: grp.name,
    files: files.map((file) => file.name).sort(),
    size,
    ...(maps.length > 0 ? { maps } : {}),
    // The shareware episode plays no user maps, so a map dropped for it could never be played.
    ...(isShareware(grp.size) ? { sealed: true } : {}),
  };
}

/**
 * The games in `GAMES`, the shareware episode first, then the free games the module offers whose
 * folders are not there, as `duke3d-games` lists them.
 */
export async function findGames(cmd: Command): Promise<Game[]> {
  // No folder yet is no games yet: a fetch or a drop makes it.
  const children = await cmd.sys.vfs.children(cmd.access, GAMES).catch(() => []);
  const folders = children.filter((child) => child.node.type === 'directory' && !child.name.startsWith('.')).map((child) => child.name);
  const installed = (await Promise.all(folders.map((name) => readGame(cmd, name)))).filter((game) => game !== undefined).sort(byRank);
  return [...installed, ...OFFERS.filter((offer) => !folders.includes(offer.id))];
}
