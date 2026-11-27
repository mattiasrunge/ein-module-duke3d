/**
 * A game's title screen, read out of its GRP. A GRP is Ken Silverman's: `KenSilverman`, the number
 * of files, a 16-byte entry for each (its name in 12 bytes and its size), then the files one after
 * the other. An ART file is a header (version, tiles, first and last tile), each tile's width, then
 * height, then animation, then the tiles' pixels, a palette index each, column after column. Only
 * the bytes needed are read.
 */

import type { Command } from '@ein/client';
import { pngDataUrl } from '@games/png';

/** The title screen's tile in Duke Nukem 3D's art: Duke on the mushroom cloud (`BETASCREEN`, names.h). */
const TITLE_TILE = 2493;

/** Seek from the start of the file: core's `ResourceSeekMode.Start`, which `@ein/client` does not export. */
const SEEK_START = 0;

/** Little-endian numbers in `bytes`, from byte `at`. */
const int32 = (bytes: Uint8Array, at: number) => new DataView(bytes.buffer, bytes.byteOffset + at, 4).getInt32(0, true);
const int16 = (bytes: Uint8Array, at: number) => new DataView(bytes.buffer, bytes.byteOffset + at, 2).getInt16(0, true);

/**
 * The title screen of the game whose GRP is at `path`, in the title's own colours, as a PNG `data:`
 * URL; undefined when the file is no GRP or has no such picture.
 */
export async function coverOf(cmd: Command, path: string): Promise<string | undefined> {
  const handle = await cmd.sys.vfs.open(cmd.access, path, { mode: 'r' });
  // `length` bytes from `at` on, fewer when the file ends sooner.
  const read = async (at: number, length: number): Promise<Uint8Array> => {
    await handle.seek(at, SEEK_START);
    const bytes = new Uint8Array(length);
    let filled = 0;
    while (filled < length) {
      const chunk = await handle.read(length - filled);
      if (chunk.length === 0) break;
      bytes.set(chunk, filled);
      filled += chunk.length;
    }
    return bytes.subarray(0, filled);
  };
  try {
    const header = await read(0, 16);
    if (header.length < 16 || new TextDecoder().decode(header.subarray(0, 12)) !== 'KenSilverman') return undefined;
    const count = int32(header, 12);
    const directory = await read(16, count * 16);
    const files = new Map<string, { at: number; size: number }>();
    let at = 16 + count * 16;
    for (let i = 0; i + 16 <= directory.length; i += 16) {
      const name = new TextDecoder().decode(directory.subarray(i, i + 12)).replace(/\0.*$/s, '').toUpperCase();
      const size = int32(directory, i + 12);
      files.set(name, { at, size });
      at += size;
    }

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
  } finally {
    await handle.close();
  }
}
