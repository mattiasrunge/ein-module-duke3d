/**
 * A game in the games folder, as `duke3d-games` answers it: what the Games page reads of it (its
 * `GameEntry`), and what the adapter needs to start it: its GRP and the files beside it.
 */
export type InstalledGame = {
  /** The folder's name, which is also how the game's page and a card remember it. */
  id: string;
  /** What the Games page calls it: Duke Nukem 3D's edition when the module knows its GRP, else the folder's name. */
  title: string;
  state: 'installed';
  /** The game's folder. */
  path: string;
  /** Its GRP, its size and its user maps, as the Games page shows them under its title. */
  detail: string;
  /** What the game is, for a game the module knows, shown under its title on its page. */
  description?: string;
  /** Its title screen, read out of its GRP, as a PNG `data:` URL; none when the GRP has none. */
  cover?: string;
  /** The game's GRP, `duke3d.grp` or another `duke3d*.grp`, as named in the folder. */
  grp: string;
  /** Every file in the folder, the GRP among them: the game loads them all, as Duke did from its directory. */
  files: string[];
  /** Bytes, the files together. */
  size: number;
  /** Its user maps, the `.map` files in its folder, by name without `.map`; none when it has none, or is the shareware episode, which plays none. */
  maps?: string[];
};

/** A free game the module fetches with `duke3d-fetch`, while its folder is not there. */
export type OfferedGame = {
  /** The folder it is fetched into, and what `duke3d-fetch` takes. */
  id: string;
  title: string;
  state: 'offered';
  /** What it is, and the bytes fetching it downloads. */
  about: string;
  download: number;
};

/** A game as `duke3d-games` answers it: in the games folder, or offered to fetch. */
export type Game = InstalledGame | OfferedGame;

/** A pack as `duke3d-packs` answers it: what it is, and whether it is fetched, which every game then loads. */
export type Pack = {
  /** What `duke3d-fetch` takes. */
  id: string;
  title: string;
  about: string;
  /** Its file, in the packs folder when fetched. */
  file: string;
  /** Bytes, fetched or to fetch. */
  size: number;
  /** Its definitions files, which the game loads as modules of their own. */
  defs?: string[];
  fetched: boolean;
};
