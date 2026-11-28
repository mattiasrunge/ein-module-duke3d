/**
 * What the page starts the module's engine worker with (`EngineStart.payload`), and where the game's
 * files are in the engine's file system. Free of the DOM, as everything the worker imports is.
 */
import type { ServerInfo } from '@games/api';
import type { EmscriptenBuild } from '@games/engine';
import type { Level } from '../../lib/api.ts';
import { MODES } from '../../lib/games.ts';
import type { Hosted } from './net.ts';

/** Where the game's files are in Emscripten's file system: its working directory, as Duke's own was, its files and the player's saves and settings. */
export const GAME_DIR = '/duke3d';

/**
 * The engine's build in `assets/` by its URLs, and the game's command line without the program's
 * name: its GRP, and `-map` and a user map; the level to start on alone, Duke's volume and level
 * from 0; the game's levels, for the hosted games it plays in, and the one it joins from the start
 * (`server`); and the player's name.
 */
export type DukeStart = {
  build: EmscriptenBuild;
  args: string[];
  level?: { volume: number; level: number };
  levels?: Level[];
  server?: ServerInfo;
  name?: string;
};

/** The user map's level: Duke's User Map, volume 1, level 8, which plays `boardfilename`. */
const USER_MAP = { volume: 0, level: 7 };

/** The hosted game `server`, as its host's engine starts it: on one of the game's `levels`, or on a user map. */
export function hostedOf(levels: Level[], server: ServerInfo): Hosted {
  const map = server.map.toLowerCase();
  const level = levels.find((level) => level.map === map);
  const at = level ? { volume: level.volume, level: level.level } : { ...USER_MAP, file: `${map}.map` };
  return { ...at, mode: MODES[server.mode] ?? 0, maxPlayers: server.maxPlayers };
}
