/**
 * A game's levels, as its GRP's CON files name them (`definelevelname <volume> <level> <map>
 * <par> <designer's time> <title>`), each whose map the GRP holds: the shareware episode's CON names
 * all three episodes' levels, its GRP has the first one's.
 */
import type { Command } from '@ein/client';
import type { Level } from './api.ts';
import { withGrp } from './grp.ts';

const DEFINE_LEVEL = /^\s*definelevelname\s+(\d+)\s+(\d+)\s+(\S+)\.map\s+\S+\s+\S+\s+(.+?)\s*$/gim;

/** `HOLLYWOOD HOLOCAUST` as `Hollywood Holocaust`; `L.A. RUMBLE`, `XXX-STACY` keep their initials and their letters without a vowel. */
export const titleCase = (title: string): string =>
  title.replace(/[^\s-]+/g, (word) => /\.|^[^AEIOUY]+$/i.test(word) ? word : word[0].toUpperCase() + word.slice(1).toLowerCase());

/** The levels of the game whose GRP is at `path`, in the order its episodes play them; none when it is no GRP. */
export async function levelsOf(cmd: Command, path: string): Promise<Level[]> {
  return await withGrp(cmd, path, async ({ files, read }) => {
    const levels = new Map<string, Level>();
    for (const [name, file] of files) {
      if (!name.endsWith('.CON')) continue;
      const text = new TextDecoder('latin1').decode(await read(file.at, file.size));
      for (const [, volume, level, map, title] of text.matchAll(DEFINE_LEVEL)) {
        if (!files.has(`${map.toUpperCase()}.MAP`)) continue;
        levels.set(map.toLowerCase(), { map: map.toLowerCase(), volume: Number(volume), level: Number(level), title: titleCase(title) });
      }
    }
    return [...levels.values()].sort((a, b) => a.volume - b.volume || a.level - b.level);
  }) ?? [];
}
