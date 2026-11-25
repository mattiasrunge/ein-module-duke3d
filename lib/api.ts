/** A game in the games folder, as `duke3d-games` answers it and the page lists it. */
export type Game = {
  /** The folder's name, which is also how the page remembers it. */
  name: string;
  /** What the list calls it: Duke Nukem 3D's edition when the module knows its GRP, else the folder's name. */
  title: string;
  /** The game's GRP, `duke3d.grp` or another `duke3d*.grp`, as named in the folder. */
  grp: string;
  /** Every file in the folder, the GRP among them: the game loads them all, as Duke did from its directory. */
  files: string[];
  /** Bytes, the files together. */
  size: number;
};
