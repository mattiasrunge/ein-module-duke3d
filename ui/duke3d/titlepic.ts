import { fileUrl } from '@ein/ui/lib';
import { type Game, GAMES } from './games.ts';

/** The title screen's tile in Duke Nukem 3D's art: Duke on the mushroom cloud (`BETASCREEN`, names.h). */
const TITLE_TILE = 2493;

/** Pictures already drawn, by game, GRP and size: a game whose GRP changes draws again. */
const drawn = new Map<string, Promise<string | undefined>>();

/**
 * A game's title screen as an image URL, read out of its GRP: the tile from the ART file that holds
 * it, in the title's own colours. Only the GRP's directory and the parts needed are fetched,
 * with HTTP ranges.
 */
export function titlePicture(game: Game): Promise<string | undefined> {
  const key = `${game.name}:${game.grp}:${game.size}`;
  let picture = drawn.get(key);
  if (!picture) {
    picture = draw(`${GAMES}/${game.name}/${game.grp}`).catch(() => undefined);
    drawn.set(key, picture);
  }
  return picture;
}

/** Bytes `start` to `start + length` of the file at `path`. */
async function range(
  path: string,
  start: number,
  length: number,
): Promise<Uint8Array> {
  const response = await fetch(fileUrl(path), {
    headers: { range: `bytes=${start}-${start + length - 1}` },
  });
  if (!response.ok) {
    throw new Error(`${path}: ${response.status} ${response.statusText}`);
  }
  const bytes = new Uint8Array(await response.arrayBuffer());
  // A server that ignores the range sends the whole file.
  return response.status === 206 ? bytes : bytes.subarray(start, start + length);
}

/** Little-endian 32-bit numbers in `bytes`, from byte `at`. */
const int32 = (bytes: Uint8Array, at: number) => new DataView(bytes.buffer, bytes.byteOffset + at, 4).getInt32(0, true);
const int16 = (bytes: Uint8Array, at: number) => new DataView(bytes.buffer, bytes.byteOffset + at, 2).getInt16(0, true);

/**
 * A GRP is Ken Silverman's: "KenSilverman", the number of files, a 16-byte entry for each (its name
 * in 12 bytes and its size), then the files one after the other. An ART file is a header (version,
 * tiles, first and last tile), each tile's width, then height, then animation, then the tiles'
 * pixels, a palette index each, column after column.
 */
async function draw(path: string): Promise<string | undefined> {
  const header = await range(path, 0, 16);
  if (new TextDecoder().decode(header.subarray(0, 12)) !== 'KenSilverman') {
    return undefined;
  }
  const count = int32(header, 12);
  const directory = await range(path, 16, count * 16);
  const files = new Map<string, { at: number; size: number }>();
  let at = 16 + count * 16;
  for (let i = 0; i < count; i++) {
    const entry = directory.subarray(i * 16, i * 16 + 16);
    const name = new TextDecoder().decode(entry.subarray(0, 12)).replace(
      /\0.*$/s,
      '',
    ).toUpperCase();
    const size = int32(entry, 12);
    files.set(name, { at, size });
    at += size;
  }

  // The title's palette, `titlepal`, is the third of those after LOOKUP.DAT's colour lookups: their
  // number in a byte, then each one's index and its 256 entries, then the water's, the slime's and
  // the title's palettes.
  const lookups = files.get('LOOKUP.DAT');
  if (!lookups) return undefined;
  const numbered = (await range(path, lookups.at, 1))[0];
  const colours = await range(path, lookups.at + 1 + numbered * 257 + 768 * 2, 768);

  for (const [name, art] of files) {
    if (!/^TILES\d{3}\.ART$/.test(name)) continue;
    const head = await range(path, art.at, 16);
    const first = int32(head, 8);
    const last = int32(head, 12);
    if (TITLE_TILE < first || TITLE_TILE > last) continue;
    const tiles = last - first + 1;
    const sizes = await range(path, art.at + 16, tiles * 4);
    const widths = Array.from({ length: tiles }, (_, i) => int16(sizes, i * 2));
    const heights = Array.from(
      { length: tiles },
      (_, i) => int16(sizes, tiles * 2 + i * 2),
    );
    const index = TITLE_TILE - first;
    let offset = art.at + 16 + tiles * 8;
    for (let i = 0; i < index; i++) offset += widths[i] * heights[i];
    const width = widths[index];
    const height = heights[index];
    if (width <= 0 || height <= 0) return undefined;
    const pixels = await range(path, offset, width * height);

    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = height;
    const context = canvas.getContext('2d');
    if (!context) return undefined;
    const image = context.createImageData(width, height);
    for (let x = 0; x < width; x++) {
      for (let y = 0; y < height; y++) {
        const colour = pixels[x * height + y];
        const to = (y * width + x) * 4;
        // The palette's levels are 0 to 63, a VGA DAC's.
        image.data[to] = colours[colour * 3] * 255 / 63;
        image.data[to + 1] = colours[colour * 3 + 1] * 255 / 63;
        image.data[to + 2] = colours[colour * 3 + 2] * 255 / 63;
        image.data[to + 3] = 255;
      }
    }
    context.putImageData(image, 0, 0);
    return canvas.toDataURL();
  }
  return undefined;
}
