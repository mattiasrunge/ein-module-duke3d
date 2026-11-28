/**
 * What the page starts the module's engine worker with (`EngineStart.payload`), and where the game's
 * files are in the engine's file system. Free of the DOM, as everything the worker imports is.
 */
import type { EmscriptenBuild } from '@games/engine';
import type { Hosted } from './net.ts';

/** Where the game's files are in Emscripten's file system: its working directory, as Duke's own was, its files and the player's saves and settings. */
export const GAME_DIR = '/duke3d';

/**
 * The engine's build in `assets/` by its URLs, and the game's command line without the program's
 * name: its GRP, and `-map` and a user map; the level to start on alone, Duke's volume and level
 * from 0; and, in a hosted game, the game hosted (`online`) and the player's name.
 */
export type DukeStart = { build: EmscriptenBuild; args: string[]; level?: { volume: number; level: number }; online?: Hosted; name?: string };
