/**
 * Where the game data is: a folder in the household's shared home, so everyone signed in plays the
 * same games and may add to them. Each folder in it with a `duke3d*.grp` is a game, as Duke Nukem
 * 3D's directory was: its GRP, and beside it whatever else the game reads, a user map, CON files or
 * demos. The games the page offers (`OFFERS`) are fetched here by `duke3d-fetch`; a folder dropped
 * on the page is uploaded here.
 */
export const GAMES = '/groups/users/duke3d';

/** A base GRP, as Chocolate Duke3D looks for one: `duke3d.grp`, `DUKE3D.GRP`, `duke3d_atomic.grp`. */
export const GRP_FILE = /^duke3d.*\.grp$/i;

/** The editions the module knows, by their GRP's size, as Chocolate Duke3D knows them. */
const EDITIONS: Record<number, string> = {
  11035779: 'Duke Nukem 3D (shareware)',
  26524524: 'Duke Nukem 3D',
  44348015: 'Duke Nukem 3D: Atomic Edition',
  44356548: 'Duke Nukem 3D: Atomic Edition',
};

/** The title of the game in folder `name` whose GRP is `size` bytes. */
export function titleOf(name: string, size: number): string {
  return EDITIONS[size] ?? name;
}

/** The shareware episode first, then the whole game, then the rest by name. */
const rank = (title: string) => title === 'Duke Nukem 3D (shareware)' ? 0 : Object.values(EDITIONS).includes(title) ? 1 : 2;

export const byRank = (a: { title: string; name: string }, b: { title: string; name: string }): number => rank(a.title) - rank(b.title) || a.name.localeCompare(b.name);

/** A game the page offers to fetch with `duke3d-fetch`, by the folder it is fetched into. */
export type Offer = { name: string; title: string; about: string; download: number };

/** The games the page offers, while their folders are not there: free ones, for something to play. */
export const OFFERS: Offer[] = [
  {
    name: 'shareware',
    title: 'Duke Nukem 3D (shareware)',
    about: "L.A. Meltdown, the first episode of 3D Realms' Duke Nukem 3D 1.3D, free to play and to give away free of charge.",
    download: 5_906_712,
  },
];
