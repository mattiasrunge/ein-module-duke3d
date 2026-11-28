/**
 * Playing together, in the engine worker: the wire of EDuke32's NetDuke32 netcode
 * (`net_transport.h`), carried through the games module's relay. Its players play as a star: one
 * hosts, slot 0, and plays too; each other player, a guest, plays through the host alone, at a slot
 * the host gives. The host is the relay's, the player seated longest; when they leave, the guests'
 * games end to the menu and the next host starts the hosted game again, which the others join.
 *
 * Everything goes as datagrams on `PORT`, each one message whose first byte says what it is: a
 * netcode frame on its channel (`FRAME`), the transport's own word (`CONTROL`, JSON), a late joiner's
 * snapshot of the running game (`SNAPSHOT`), or a part of a message longer than a datagram
 * (`PART`). The relay's link loses nothing and keeps the order, so nothing is acknowledged or sent
 * again, and a long message's parts come one after the other.
 *
 * The netcode is called at the game's yield points, as it was from the browser's events: `step`
 * runs between ticks, reads what came and answers the transport's word there, and `poll`, the
 * netcode's own `net_poll`, hands it the frames and the players who came and went, in order.
 */
import type { EngineNetwork } from '@games/engine';
import { addressNumber, type RelayRoster } from '@games/net';

/** The port the game's datagrams go to and from, Duke Nukem 3D's own. */
export const PORT = 23513;

/** What a datagram is, by its first byte: a netcode frame is its channel, 0 to 2. */
const CONTROL = 3;
const SNAPSHOT = 4;
const PART = 5;

/** The longest message a datagram carries whole, less than a relayed frame's room. */
const DATAGRAM = 60_000;

/** How long a guest waits for the host's answer before asking to join again: the host's game may still be starting. */
const ASK_MS = 1000;

/** How long a guest whose host has gone waits for its game to leave (`leave`), going back to its menu, before it finds a host again itself. */
const LEAVING_MS = 5000;

/** A player coming or going, in what the netcode is told (`net_peerevent_t`). */
const PEER_DOWN = 0;
const PEER_UP = 1;

/** The most players NetDuke32 seats. */
const MAX_SLOTS = 16;

/** What the netcode's file system holds of a running game for a late joiner (oldnet.cpp `LATEJOIN_SAVE`), in the game's directory. */
const SNAPSHOT_FILE = 'latejoin.esv';

/**
 * The hosted game, as the host's engine starts it: Duke's volume and level from 0 (a user map is
 * volume 0, level 7), its game type (`ud.m_coop`), the user map's file when it is one, and how many
 * may play.
 */
export type Hosted = { volume: number; level: number; mode: number; file?: string; maxPlayers: number };

/** What the transport tells another player. */
type Control =
  | { t: 'join'; name?: string }
  | { t: 'joined'; slot: number }
  | { t: 'refused'; reason: string }
  | { t: 'kicked' }
  | { t: 'snapshot'; mask: number; plc: number; join: number; spawns: string };

/** What the netcode is handed at its next poll: a frame from a slot, or a player at a slot coming or going. */
type Inbound = { frame: Uint8Array; slot: number; channel: number } | { event: number; slot: number };

/** What of the game's module the transport calls. */
export type NetModule = {
  FS: { readFile(path: string): Uint8Array; writeFile(path: string, data: Uint8Array): void };
  HEAPU8: Uint8Array;
  _malloc(size: number): number;
  _free(at: number): void;
  stringToNewUTF8(text: string): number;
  UTF8ToString(at: number): string;
  _Net_ReceiveFrame(slot: number, channel: number, at: number, length: number): void;
  _Net_PeerEvent(slot: number, event: number): void;
  _Net_SetLocalIndex(slot: number): void;
  _Net_SnapshotReady(mask: number, plc: number, join: number): void;
  _Net_GetSpawnTable(): number;
  _Net_SetSpawnTable(at: number): void;
  _Net_GetBotMask(): number;
  _NetMenu_SetStatus(at: number): void;
  _NetMenu_OnJoined(slot: number): void;
  _ein_board(at: number): void;
};

const encoder = new TextEncoder();
const decoder = new TextDecoder();

/** `kind` and `body` as one message. */
function message(kind: number, body: Uint8Array): Uint8Array {
  const whole = new Uint8Array(1 + body.length);
  whole[0] = kind;
  whole.set(body, 1);
  return whole;
}

/**
 * The game's wire, `Module.einDuke`, which `ein-library.js` hands the netcode's sends to and its
 * polls, and which the game's menus call (`menus.cpp`): `launch`, as the menu comes up,
 * `sendSnapshot` for a late joiner, and `leave`.
 */
export class DukeLink {
  /** Whether this player hosts, is a guest, or neither yet: the next step decides by the relay's roster. */
  #role?: 'host' | 'guest';
  /** The address each slot plays from: the guests' for the host, the host's at 0 for a guest. */
  readonly #addresses = new Map<number, number>();
  readonly #slots = new Map<number, number>();
  /** A guest's host, whether the host has seated them, and when they last asked. */
  #host?: number;
  #joined = false;
  #asked = -Infinity;
  /** Since when a seated guest's host has gone, or dropped them, while their game goes back to its menu. */
  #leaving?: number;
  /** The hosted game, for the host's game to start once (`launch`). */
  #launch?: Hosted;
  readonly #inbound: Inbound[] = [];
  /** The parts of a long message from each address, until its last. */
  readonly #parts = new Map<number, Uint8Array[]>();
  /** What the host said of the snapshot that follows. */
  #snapshot?: Extract<Control, { t: 'snapshot' }>;

  constructor(
    private readonly duke: NetModule,
    private readonly network: EngineNetwork,
    private readonly hosted: Hosted,
    private readonly name: string | undefined,
    private readonly log: (line: string) => void,
  ) {
    network.open(PORT);
  }

  /**
   * Between ticks: follow the relay's roster, read what came, and answer the transport's word. The
   * game's menu must be up (`ready`) before a player hosts or joins, so the netcode has its players.
   */
  step(ready: boolean): void {
    const roster = this.network.relay;
    if (!roster) return;
    this.#follow(roster, ready);
    for (let datagram = this.network.recv(PORT); datagram; datagram = this.network.recv(PORT)) this.#heard(datagram.ip, datagram.data);
    if (this.#role === 'guest' && !this.#joined && this.#host !== undefined && performance.now() - this.#asked >= ASK_MS) {
      this.#asked = performance.now();
      this.#tell(this.#host, { t: 'join', ...(this.name ? { name: this.name } : {}) });
    }
  }

  /** `net_poll`: hand the netcode what came since, in order. */
  poll(): void {
    for (const item of this.#inbound.splice(0)) {
      if ('event' in item) {
        this.duke._Net_PeerEvent(item.slot, item.event);
        continue;
      }
      const at = this.duke._malloc(item.frame.length || 1);
      this.duke.HEAPU8.set(item.frame, at);
      this.duke._Net_ReceiveFrame(item.slot, item.channel, at, item.frame.length);
      this.duke._free(at);
    }
  }

  /** `net_send`: a frame to the player at `slot`. */
  send(slot: number, channel: number, data: Uint8Array): void {
    const address = this.#addresses.get(slot);
    if (address !== undefined) this.#post(address, message(channel, data));
  }

  /** `net_broadcast`: a frame to every other player the netcode has, which is the host alone for a guest. */
  broadcast(channel: number, data: Uint8Array): void {
    const whole = message(channel, data);
    for (const address of this.#addresses.values()) this.#post(address, whole);
  }

  /** `net_kick`: the host lets go of a guest the netcode has dropped, who joins again when they can. */
  kick(slot: number): void {
    const address = this.#addresses.get(slot);
    if (address === undefined || this.#role !== 'host') return;
    this.log(`net: slot ${slot} dropped`);
    this.#tell(address, { t: 'kicked' });
    this.#forget(slot);
  }

  /** `net_transport_shutdown`: the game ends. */
  shutdown(): void {
    this.#reset();
  }

  /** The hosted game, once, for the host's game to start as its menu comes up (`ein_net_launch`). */
  launch(): Hosted | undefined {
    const launch = this.#launch;
    this.#launch = undefined;
    return launch;
  }

  /**
   * The host sends the late joiner at `slot` the running game, which the netcode has just saved
   * (`netmenu_send_snapshot_to`), with the premap's spawn table it does not hold; `slot` below 0 is
   * every guest.
   */
  sendSnapshot(mask: number, slot: number, plc: number, join: number): void {
    if (this.#role !== 'host') return;
    let data: Uint8Array;
    try {
      data = this.duke.FS.readFile(SNAPSHOT_FILE);
    } catch (error) {
      this.log(`net: no snapshot to send: ${error}`);
      return;
    }
    const spawns = this.duke.UTF8ToString(this.duke._Net_GetSpawnTable());
    const to = slot < 0 ? [...this.#addresses.values()] : [this.#addresses.get(slot)].filter((address) => address !== undefined);
    this.log(`net: snapshot of ${data.length} bytes to ${to.length} player(s)`);
    for (const address of to) {
      this.#tell(address, { t: 'snapshot', mask, plc, join, spawns });
      this.#post(address, message(SNAPSHOT, data));
    }
  }

  /** The game left the hosted game, as it does to its menu when its host has gone: the next step finds the host again. */
  leave(): void {
    this.log('net: left');
    this.#reset();
  }

  /** Decide whom to play with by the roster, and let go of those who left it. */
  #follow(roster: RelayRoster, ready: boolean): void {
    const here = new Set(roster.players.map((player) => addressNumber(player.address)));
    const host = addressNumber(roster.host);
    const self = addressNumber(roster.you);
    if (this.#role === 'host') {
      for (const [slot, address] of this.#addresses) {
        if (here.has(address)) continue;
        this.log(`net: slot ${slot} left`);
        this.#inbound.push({ event: PEER_DOWN, slot });
        this.#forget(slot);
      }
      return;
    }
    if (this.#role === 'guest') {
      if (this.#leaving !== undefined) {
        if (performance.now() - this.#leaving >= LEAVING_MS) this.#reset();
        return;
      }
      if (host === this.#host) return;
      // The host left: a seated guest's game hears it and goes back to its menu, which leaves; one
      // still asking asks the next host.
      this.log('net: the host left');
      if (this.#joined) this.#gone();
      else this.#host = host;
      return;
    }
    if (!ready) return;
    if (this.hosted.file) this.#board(this.hosted.file);
    if (host === self) {
      this.log('net: hosting');
      this.#role = 'host';
      this.duke._Net_SetLocalIndex(0);
      this.#launch = this.hosted;
    } else {
      this.log('net: joining the host');
      this.#role = 'guest';
      this.#host = host;
      this.#joined = false;
      this.#asked = -Infinity;
      this.#status('Joining the game');
    }
  }

  /** A datagram came from `address`. */
  #heard(address: number, data: Uint8Array): void {
    if (data.length === 0) return;
    if (data[0] === PART) {
      const parts = this.#parts.get(address) ?? [];
      parts.push(data.slice(2));
      if (data[1] === 0) {
        this.#parts.set(address, parts);
        return;
      }
      this.#parts.delete(address);
      const whole = new Uint8Array(parts.reduce((length, part) => length + part.length, 0));
      let at = 0;
      for (const part of parts) {
        whole.set(part, at);
        at += part.length;
      }
      this.#heard(address, whole);
      return;
    }
    const kind = data[0];
    const body = data.subarray(1);
    if (kind === CONTROL) {
      let control: Control;
      try {
        control = JSON.parse(decoder.decode(body)) as Control;
      } catch {
        return;
      }
      this.#control(address, control);
    } else if (kind === SNAPSHOT) {
      this.#landed(address, body);
    } else if (kind < CONTROL) {
      const slot = this.#slots.get(address);
      if (slot !== undefined) this.#inbound.push({ frame: body.slice(), slot, channel: kind });
    }
  }

  #control(address: number, control: Control): void {
    if (this.#role === 'host' && control.t === 'join') return this.#seat(address, control.name);
    if (this.#role !== 'guest' || address !== this.#host) return;
    if (control.t === 'joined' && !this.#joined && this.#leaving === undefined) {
      this.log(`net: joined at slot ${control.slot}`);
      this.#joined = true;
      this.duke._Net_SetLocalIndex(control.slot);
      this.#addresses.set(0, address);
      this.#slots.set(address, 0);
      this.#inbound.push({ event: PEER_UP, slot: 0 });
      this.duke._NetMenu_OnJoined(control.slot);
      this.#status('Joined: waiting for the game');
    } else if (control.t === 'refused') {
      this.log(`net: refused: ${control.reason}`);
      this.#status(`!${control.reason}`);
    } else if (control.t === 'kicked' && this.#joined && this.#leaving === undefined) {
      // Dropped by the host's netcode, as a page hidden too long is: the game goes back to its menu and joins again.
      this.log('net: dropped by the host');
      this.#gone();
    } else if (control.t === 'snapshot') {
      this.#snapshot = control;
    }
  }

  /** The host seats the guest at `address` at the first free slot, or tells them again where they sit. */
  #seat(address: number, name: string | undefined): void {
    const seated = this.#slots.get(address);
    if (seated !== undefined) return this.#tell(address, { t: 'joined', slot: seated });
    const bots = this.duke._Net_GetBotMask();
    let slot = 1;
    while (slot < Math.min(this.hosted.maxPlayers, MAX_SLOTS) && (this.#addresses.has(slot) || bots & (1 << slot))) slot++;
    if (slot >= Math.min(this.hosted.maxPlayers, MAX_SLOTS)) return this.#tell(address, { t: 'refused', reason: 'The game is full' });
    this.log(`net: ${name ?? 'a player'} joins at slot ${slot}`);
    this.#addresses.set(slot, address);
    this.#slots.set(address, slot);
    this.#inbound.push({ event: PEER_UP, slot });
    this.#tell(address, { t: 'joined', slot });
  }

  /** The host's snapshot of the running game came: the netcode loads it at its next frame. */
  #landed(address: number, data: Uint8Array): void {
    const snapshot = this.#snapshot;
    this.#snapshot = undefined;
    if (!snapshot || this.#role !== 'guest' || address !== this.#host) return;
    this.log(`net: snapshot of ${data.length} bytes`);
    this.duke.FS.writeFile(SNAPSHOT_FILE, data);
    if (snapshot.spawns) {
      const at = this.duke.stringToNewUTF8(snapshot.spawns);
      this.duke._Net_SetSpawnTable(at);
      this.duke._free(at);
    }
    this.duke._Net_SnapshotReady(snapshot.mask, snapshot.plc, snapshot.join);
  }

  #tell(address: number, control: Control): void {
    this.#post(address, message(CONTROL, encoder.encode(JSON.stringify(control))));
  }

  /** Send `whole` to `address`: in parts when it is longer than a datagram carries. */
  #post(address: number, whole: Uint8Array): void {
    if (whole.length <= DATAGRAM) return this.network.send(PORT, address, PORT, whole);
    for (let at = 0; at < whole.length; at += DATAGRAM) {
      const part = whole.subarray(at, at + DATAGRAM);
      const datagram = new Uint8Array(2 + part.length);
      datagram[0] = PART;
      datagram[1] = at + DATAGRAM >= whole.length ? 1 : 0;
      datagram.set(part, 2);
      this.network.send(PORT, address, PORT, datagram);
    }
  }

  /** A seated guest has lost their host: their netcode hears it, and their game goes back to its menu and leaves. */
  #gone(): void {
    this.#inbound.push({ event: PEER_DOWN, slot: 0 });
    this.#leaving = performance.now();
    this.#addresses.clear();
    this.#slots.clear();
  }

  #forget(slot: number): void {
    const address = this.#addresses.get(slot);
    this.#addresses.delete(slot);
    if (address !== undefined) this.#slots.delete(address);
  }

  #reset(): void {
    this.#role = undefined;
    this.#host = undefined;
    this.#joined = false;
    this.#leaving = undefined;
    this.#launch = undefined;
    this.#snapshot = undefined;
    this.#addresses.clear();
    this.#slots.clear();
    this.#parts.clear();
    this.#inbound.length = 0;
  }

  /** The user map the hosted game is played on, for the netcode's User Map level. */
  #board(file: string): void {
    const at = this.duke.stringToNewUTF8(file);
    this.duke._ein_board(at);
    this.duke._free(at);
  }

  /** A line for the menus' multiplayer screens: `!` first for a failure. */
  #status(text: string): void {
    const at = this.duke.stringToNewUTF8(text);
    this.duke._NetMenu_SetStatus(at);
    this.duke._free(at);
  }
}
