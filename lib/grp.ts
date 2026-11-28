/**
 * Reading a GRP, Ken Silverman's: `KenSilverman`, the number of files, a 16-byte entry for each (its
 * name in 12 bytes and its size), then the files one after the other. Only the bytes asked for are
 * read.
 */
import type { Command } from '@ein/client';

/** Seek from the start of the file: core's `ResourceSeekMode.Start`, which `@ein/client` does not export. */
const SEEK_START = 0;

/** Little-endian numbers in `bytes`, from byte `at`. */
export const int32 = (bytes: Uint8Array, at: number) => new DataView(bytes.buffer, bytes.byteOffset + at, 4).getInt32(0, true);
export const int16 = (bytes: Uint8Array, at: number) => new DataView(bytes.buffer, bytes.byteOffset + at, 2).getInt16(0, true);

/** A GRP open: its files by their names in upper case, where each starts and its size, and `length` bytes from `at` on, fewer when the GRP ends sooner. */
export type Grp = { files: Map<string, { at: number; size: number }>; read(at: number, length: number): Promise<Uint8Array> };

/** `use` the GRP at `path`, which is closed again after; undefined when the file is no GRP. */
export async function withGrp<T>(cmd: Command, path: string, use: (grp: Grp) => Promise<T>): Promise<T | undefined> {
  const handle = await cmd.sys.vfs.open(cmd.access, path, { mode: 'r' });
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
    return await use({ files, read });
  } finally {
    await handle.close();
  }
}
