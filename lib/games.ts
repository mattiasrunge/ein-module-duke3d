import type { OfferedGame } from './api.ts';

/**
 * Where the game data is: a folder in the household's shared home, so everyone signed in plays the
 * same games and may add to them. Each folder in it with a `duke3d*.grp` is a game, as Duke Nukem
 * 3D's directory was: its GRP, and beside it whatever else the game reads, a user map, CON files or
 * demos. The games the module offers (`OFFERS`) are fetched here by `duke3d-fetch`; a GRP or a
 * folder dropped on the Games page is uploaded here.
 */
export const GAMES = '/groups/users/duke3d';

/** A base GRP, as Chocolate Duke3D looks for one: `duke3d.grp`, `DUKE3D.GRP`, `duke3d_atomic.grp`. */
export const GRP_FILE = /^duke3d.*\.grp$/i;

/** A user map beside the GRP, by its name without `.map`, which the game is started on with `-map`. */
export const MAP_FILE = /^(.+)\.map$/i;

/** The size of the shareware episode's GRP, 1.3D's. */
const SHAREWARE = 11035779;

/** The editions the module knows, by their GRP's size, as Chocolate Duke3D knows them. */
const EDITIONS: Record<number, string> = {
  [SHAREWARE]: 'Duke Nukem 3D (shareware)',
  26524524: 'Duke Nukem 3D',
  44348015: 'Duke Nukem 3D: Atomic Edition',
  44356548: 'Duke Nukem 3D: Atomic Edition',
};

/** The title of the game in folder `name` whose GRP is `size` bytes. */
export function titleOf(name: string, size: number): string {
  return EDITIONS[size] ?? name;
}

/** Whether a GRP of `size` bytes is the shareware episode's, which plays no user map, as Duke's shareware never did. */
export const isShareware = (size: number): boolean => size === SHAREWARE;

/** The shareware episode first, then the whole game, then the rest by name. */
const rank = (title: string) => title === 'Duke Nukem 3D (shareware)' ? 0 : Object.values(EDITIONS).includes(title) ? 1 : 2;

export const byRank = (a: { title: string; id: string }, b: { title: string; id: string }): number => rank(a.title) - rank(b.title) || a.id.localeCompare(b.id);

/** The games the module offers, while their folders are not there: free ones, for something to play. */
export const OFFERS: OfferedGame[] = [
  {
    id: 'shareware',
    title: 'Duke Nukem 3D (shareware)',
    state: 'offered',
    about: "L.A. Meltdown, the first episode of 3D Realms' Duke Nukem 3D 1.3D, free to play and to give away free of charge.",
    download: 5_906_712,
  },
];
