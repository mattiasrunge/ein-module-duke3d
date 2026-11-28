/**
 * The add-on packs for EDuke32 the module offers to fetch, from their makers' own downloads: each is
 * a zip the game loads beside its GRP (`-g`), its files replacing or adding to the GRP's. They are
 * fetched only when someone chooses to, into `PACKS`, and every game of the module loads them.
 */
import { GAMES } from './games.ts';

/** Where the packs fetched are, beside the games: a folder of the games folder's own, which is no game. */
export const PACKS = `${GAMES}/.packs`;

/** A pack the module knows: what it is, and where it is fetched from. */
export type KnownPack = {
  /** What `duke3d-fetch` takes. */
  id: string;
  title: string;
  /** What it is and who made it. */
  about: string;
  /** Its file in `PACKS`. */
  file: string;
  url: string;
  sha256: string;
  /** Bytes. */
  size: number;
  /**
   * Its definitions files, which say what its files replace and add: each loaded as a module of
   * its own (`-mh`), as the `duke3d.def` packs bring would hide each other's.
   */
  defs?: string[];
};

/** The packs, in the order the game loads them, later ones over earlier ones' files; `bin/duke3d-fetch` has their URLs and SHA-256 too. */
export const KNOWN_PACKS: KnownPack[] = [
  {
    id: 'sc55',
    title: 'Roland SC-55 music',
    about: "Duke's songs as a Roland Sound Canvas played them, recorded by MusicallyInspired, in place of the Sound Blaster's FM synthesis.",
    file: 'duke3d_music-sc55.zip',
    url: 'https://www.duke4.org/files/nightfright/music/duke3d_music-sc55.zip',
    sha256: 'e123574fb5629b4871338a83a4ecb882aa6c8f5993f2e90e08439e5faf139087',
    size: 155_441_427,
  },
  {
    id: 'psx',
    title: 'PlayStation sounds',
    about: "The sound effects in the PlayStation release's better quality, by Marphy Black.",
    file: 'duke3d_psx.zip',
    url: 'https://www.duke4.org/files/nightfright/related/duke3d_psx.zip',
    sha256: '20b04f240c78153b824ecb6327bc14493dee61a6ce535c616b25972270fb75e4',
    size: 7_636_016,
  },
  {
    id: 'voxels',
    title: 'Voxels',
    about: 'Items, weapons and props as 3D voxel models in place of flat sprites, by ReaperMan, Borion and others. Each model is built the first time a level needs it.',
    file: 'duke3d_voxels.zip',
    url: 'https://github.com/NightFright2k19/duke3d_voxelpack/releases/download/2.0-rc2/duke3d_voxels.zip',
    sha256: 'cc2abd00a0fc72a6cbc32026bd7c68db58140f840cb8b94755f07b8e2c25e253',
    size: 4_432_485,
    defs: ['duke3d_voxels.def', 'duke3d_maphacks.def'],
  },
  {
    id: 'widescreen',
    title: 'Widescreen fixes',
    about: 'The status bar, the weapons and the menus drawn for widescreen, and the 1.3D menu font, by NightFright and Fox.',
    file: 'duke3d_widefix.zip',
    url: 'https://github.com/NightFright2k19/duke3d_widefix/releases/download/1.6/duke3d_widefix.zip',
    sha256: 'd5bb376f36d00e08734901e051d8b98108c67d9b239cd221a0643545dbbdb216',
    size: 1_327_269,
    defs: ['duke3d_widefix.def'],
  },
  {
    id: 'hrp-models',
    title: 'HRP models',
    about:
      "The enemies, Duke and the other characters as 3D models in place of flat sprites, from the Duke Nukem 3D High Resolution Pack's Polymost edition, by its team. A 1 GB download, of which a game reads about 60 MB, the first time in a browser: that start takes longer, the next ones read it from the browser.",
    file: 'duke3d_hrp-r750.zip',
    url: 'https://www.duke4.org/files/nightfright/hrp/duke3d_hrp-r750.zip',
    sha256: 'e1fca8bec182006a8ef683e5da204e54e6dbc2575d6510e9738825adcb270b04',
    size: 1_004_721_371,
    // The models of the pack's Polymost edition, the frozen ones after the rest; its HUD weapons
    // would replace the widescreen fixes' and its root duke3d.def is the Polymer edition, which this
    // build leaves out.
    defs: ['highres/sprites/characters_polymost.def', 'highres/sprites/monsters_polymost.def', 'highres/sprites/ice_polymost.def'],
  },
];
