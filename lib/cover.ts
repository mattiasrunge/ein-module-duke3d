/**
 * A game's title screen, read out of its GRP (`grp.ts`). An ART file is a header (version, tiles,
 * first and last tile), each tile's width, then height, then animation, then the tiles' pixels, a
 * palette index each, column after column. Only the bytes needed are read.
 */

import type { Command } from '@ein/client';
import { pngDataUrl } from '@games/png';
import { int16, int32, withGrp } from './grp.ts';

/** The title screen's tile in Duke Nukem 3D's art: Duke on the mushroom cloud (`BETASCREEN`, names.h). */
const TITLE_TILE = 2493;

/**
 * The title screen of the game whose GRP is at `path`, in the title's own colours, as a PNG `data:`
 * URL; undefined when the file is no GRP or has no such picture.
 */
export async function coverOf(cmd: Command, path: string): Promise<string | undefined> {
  return await withGrp(cmd, path, async ({ files, read }) => {
    // The title's palette, `titlepal`, is the third of those after LOOKUP.DAT's colour lookups: their
    // number in a byte, then each one's index and its 256 entries, then the water's, the slime's and
    // the title's palettes.
    const lookups = files.get('LOOKUP.DAT');
    if (!lookups) return undefined;
    const numbered = (await read(lookups.at, 1))[0];
    const colours = await read(lookups.at + 1 + numbered * 257 + 768 * 2, 768);

    for (const [name, art] of files) {
      if (!/^TILES\d{3}\.ART$/.test(name)) continue;
      const head = await read(art.at, 16);
      const first = int32(head, 8);
      const last = int32(head, 12);
      if (TITLE_TILE < first || TITLE_TILE > last) continue;
      const tiles = last - first + 1;
      const sizes = await read(art.at + 16, tiles * 4);
      const index = TITLE_TILE - first;
      let offset = art.at + 16 + tiles * 8;
      for (let i = 0; i < index; i++) offset += int16(sizes, i * 2) * int16(sizes, tiles * 2 + i * 2);
      const width = int16(sizes, index * 2);
      const height = int16(sizes, tiles * 2 + index * 2);
      if (width <= 0 || height <= 0) return undefined;
      const pixels = await read(offset, width * height);
      if (pixels.length < width * height) return undefined;
      const rgba = new Uint8Array(width * height * 4);
      for (let x = 0; x < width; x++) {
        for (let y = 0; y < height; y++) {
          const colour = pixels[x * height + y] * 3;
          const to = (y * width + x) * 4;
          // The palette's levels are 0 to 63, a VGA DAC's.
          rgba[to] = colours[colour] * 255 / 63;
          rgba[to + 1] = colours[colour + 1] * 255 / 63;
          rgba[to + 2] = colours[colour + 2] * 255 / 63;
          rgba[to + 3] = 255;
        }
      }
      return await pngDataUrl(width, height, rgba);
    }
    return undefined;
  });
}
