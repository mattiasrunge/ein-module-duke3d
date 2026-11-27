/**
 * What the page starts the module's engine worker with (`EngineStart.payload`), and where the game's
 * files are in the engine's file system. Free of the DOM, as everything the worker imports is.
 */
import type { EmscriptenBuild } from '@games/engine';

/** Where the game's files are in Emscripten's file system: its working directory, as Duke's own was, its files and the player's saves and duke3d.cfg. */
export const GAME_DIR = '/duke3d';

/** The engine's build in `assets/` by its URLs, and the game's command line without the program's name: `-map` and a user map, or nothing. */
export type DukeStart = { build: EmscriptenBuild; args: string[] };
