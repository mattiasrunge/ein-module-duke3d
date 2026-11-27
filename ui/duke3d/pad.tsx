import { Box, Text } from '@mantine/core';
import { homePath } from '@ein/paths';
import { fileUrl } from '@ein/ui/lib';
import { currentUser } from '@ein/ui/signals';
import type { Kept, Written } from '@games/adapter';
import { useEffect, useSyncExternalStore } from 'react';
import type { Game } from '../../lib/api.ts';

/**
 * The keyboard and the mouse Duke Nukem 3D is played with, drawn with the player's keys on them:
 * the keys they move, open, jump, crouch and run with, and what the mouse's buttons and wheel do.
 * The keys are the game's own, which the player changes in its menu, so the drawing only shows
 * them: they are read from the player's duke3d.cfg in their saves folder of the game, and again each
 * time the game writes it.
 */

/** The file the game keeps the player's keys and settings in, in their saves folder of the game. */
const CONFIG = 'duke3d.cfg';

/** The keys of a player who has not played yet, by their function's name: Duke's own (`_functio.h`) with WASD's (`engine/new-player.patch`). */
const DEFAULT_KEYS: Record<string, string[]> = {
  Move_Forward: ['W', 'Up'],
  Move_Backward: ['S', 'Down'],
  Turn_Left: ['Left', 'Kpad4'],
  Turn_Right: ['Right', 'Kpad6'],
  Strafe_Left: ['A', ','],
  Strafe_Right: ['D', '.'],
  Fire: ['LCtrl', 'RCtrl'],
  Open: ['E'],
  Run: ['LShift', 'RShift'],
  Jump: ['Space', '/'],
  Crouch: ['C'],
};

/** A new player's mouse buttons (`engine/new-player.patch`), from the left button: the left fires, the right opens, the middle walks forward. */
const DEFAULT_BUTTONS = ['Fire', 'Open', 'Move_Forward'];

/** The wheel, buttons 3 and 4, of a player who has bound it to nothing, as the game starts with it (`engine/new-player.patch`). */
const WHEEL = ['Next_Weapon', 'Previous_Weapon'];

/** What a function does, as the drawing names it; a function not here is named by its name, its underscores spaces. */
const ACTIONS: Record<string, string> = {
  Move_Forward: 'Forward',
  Move_Backward: 'Back',
  Turn_Left: 'Turn left',
  Turn_Right: 'Turn right',
  Strafe_Left: 'Left',
  Strafe_Right: 'Right',
  Next_Weapon: 'Next weapon',
  Previous_Weapon: 'Prev weapon',
  Quick_Kick: 'Kick',
  Inventory: 'Use item',
  Holo_Duke: 'Holoduke',
  NightVision: 'Night vision',
  MedKit: 'Medkit',
  TurnAround: 'Turn around',
  AutoRun: 'Always run',
};

const actionOf = (name: string): string => ACTIONS[name] ?? name.replace(/_/g, ' ');

/** The player's keys and mouse in a configuration's text: each function's keys, by its name (`[KeyDefinitions]`), and each mouse button's function (`[Controls]`). */
type Binds = { keys: Map<string, string[]>; buttons: string[] };

/** The keys and the mouse buttons in duke3d.cfg's text, its `Move_Forward = "Up" "Kpad8"` and `MouseButton0 = "Fire"` lines. */
function bindsOf(config: string): Binds {
  const keys = new Map<string, string[]>();
  const buttons: string[] = [];
  for (const [, name, quoted] of config.matchAll(/^\s*(\w+)\s*=\s*((?:"[^"]*"\s*)+)$/gm)) {
    const values = [...quoted.matchAll(/"([^"]*)"/g)].map(([, value]) => value);
    const button = /^MouseButton(\d)$/.exec(name);
    if (button) buttons[Number(button[1])] = values[0];
    else if (values.length === 2) keys.set(name, values.filter(Boolean));
  }
  return { keys, buttons };
}

/**
 * The player's keys and mouse: their configuration's, or the defaults without one, and the mouse as
 * the game starts with it (`engine/new-player.patch`): a wheel bound to nothing changes weapon, and a
 * right button bound to nothing or to DOS's strafe opens.
 */
function bindsFor(config: string | null | undefined): Binds {
  const binds = config ? bindsOf(config) : { keys: new Map(Object.entries(DEFAULT_KEYS)), buttons: [...DEFAULT_BUTTONS] };
  if (!binds.buttons[3] && !binds.buttons[4]) [binds.buttons[3], binds.buttons[4]] = WHEEL;
  if (!binds.buttons[1] || binds.buttons[1] === 'Strafe') binds.buttons[1] = 'Open';
  return binds;
}

/** Each game's configuration, by the game's id: its text, `null` when the player has none yet. */
const configs = new Map<string, string | null>();
const loading = new Set<string>();
const listeners = new Set<() => void>();

const setConfig = (id: string, config: string | null) => {
  configs.set(id, config);
  listeners.forEach((listener) => listener());
};

/** Read the player's configuration of `game` from their saves folder of it (the games module's `~/.duke3d/<game>`), once a page. */
function loadConfig(game: Game) {
  const username = currentUser.value?.username;
  if (!username || configs.has(game.id) || loading.has(game.id)) return;
  loading.add(game.id);
  fetch(fileUrl(`${homePath(username)}/.duke3d/${game.id}/${CONFIG}`))
    .then(async (response) => response.ok ? await response.text() : null, () => null)
    .then((config) => setConfig(game.id, config))
    .finally(() => loading.delete(game.id));
}

/** The game started with the player's files `kept`: its configuration among them, or none. */
export function heardKept(game: Game, kept: Kept[]) {
  const config = kept.find((file) => file.name === CONFIG);
  setConfig(game.id, config ? new TextDecoder().decode(config.data) : null);
}

/** The game wrote `written`: its configuration among them, the keys the player changed in its menu. */
export function heardWritten(game: Game, written: Written) {
  const config = written.files.find((file) => file.name === CONFIG);
  if (config) setConfig(game.id, new TextDecoder().decode(config.data));
}

/** The player's configuration of `game`, rendering again as the game writes it; undefined while it is read. */
function useConfig(game: Game): string | null | undefined {
  useEffect(() => loadConfig(game), [game]);
  return useSyncExternalStore((listener) => (listeners.add(listener), () => listeners.delete(listener)), () => configs.get(game.id));
}

/** How usual a key is for an action, the most usual first: a letter, Space or a modifier, then another character's key (`,` strafes), an arrow, the rest (the keypad's). */
const usual = (key: string): number => /^([A-Z0-9]|Space|[LR](Shift|Ctrl|Alt))$/.test(key) ? 0 : key.length === 1 ? 1 : /^(Up|Down|Left|Right)$/.test(key) ? 2 : 3;

/** The keys bound to `name`, the most usual first. */
const keysOf = (binds: Binds, name: string): string[] => [...binds.keys.get(name) ?? []].sort((a, b) => usual(a) - usual(b));

/** A key's name short enough for a key cap: `A`, `↑` for Up, `⇧` for LShift, `N8` for Kpad8. */
function keyLabel(key: string): string {
  const named: Record<string, string> = {
    Up: '↑',
    Down: '↓',
    Left: '←',
    Right: '→',
    Space: 'Space',
    LShift: '⇧',
    RShift: '⇧',
    LCtrl: 'Ctrl',
    RCtrl: 'Ctrl',
    LAlt: 'Alt',
    RAlt: 'Alt',
    Enter: '⏎',
    KpdEnt: '⏎',
    Tab: '⇥',
    BakSpc: '⌫',
    CapLck: '⇪',
    Escape: 'Esc',
    Insert: 'Ins',
    Delete: 'Del',
  };
  return named[key] ?? key.replace(/^Kpad/, 'N').slice(0, 4);
}

/** A key's name in full, for a tooltip: `A`, `up arrow`, `left Shift`, `keypad 8`. */
function keyName(key: string): string {
  if (/^(Up|Down|Left|Right)$/.test(key)) return `${key.toLowerCase()} arrow`;
  const side = /^([LR])(Shift|Ctrl|Alt)$/.exec(key);
  if (side) return `${side[1] === 'L' ? 'left' : 'right'} ${side[2]}`;
  const named: Record<string, string> = { KpdEnt: 'keypad Enter', BakSpc: 'Backspace', CapLck: 'Caps Lock', ScrLck: 'Scroll Lock', NumLck: 'Num Lock' };
  return named[key] ?? key.replace(/^Kpad/, 'keypad ');
}

/** A key cap at `x`,`y`, `w` wide: the player's key for the action on it, the action's name under it, dim when no key does it. */
function Cap({ x, y, w, action, keys }: { x: number; y: number; w: number; action: string; keys: string[] }) {
  const label = keys.length ? keyLabel(keys[0]) : '—';
  const size = label.length > 2 ? 9 : 13;
  return (
    <g data-duke3d-pad-key={action} opacity={keys.length ? 1 : 0.5}>
      <title>{keys.length ? `${action}: ${keys.map(keyName).join(' or ')}` : `${action}: no key`}</title>
      <rect x={x} y={y} width={w} height={24} rx={4} fill='#2b2b30' stroke='#55555e' strokeWidth={1.5} />
      <text
        x={x + w / 2}
        y={y + 12}
        dy='0.36em'
        textAnchor='middle'
        fontSize={size}
        fontWeight={700}
        fill='#fff'
        style={{ fontFamily: 'var(--mantine-font-family-monospace)' }}
      >
        {label}
      </text>
      <Name x={x + w / 2} y={y + 33}>{action}</Name>
    </g>
  );
}

/** An action's name in the drawing. */
function Name({ x, y, children }: { x: number; y: number; children: string }) {
  return (
    <text
      x={x}
      y={y}
      textAnchor='middle'
      fontSize={8}
      fontWeight={700}
      fill='#c4c4d0'
      style={{ letterSpacing: 0.4, paintOrder: 'stroke', stroke: 'rgba(0,0,0,0.55)', strokeWidth: 2.5 }}
    >
      {children.toUpperCase()}
    </text>
  );
}

/** The mouse at `x`,`y`: what its buttons do on them, and its wheel's two ways and its click under them. */
function Mouse({ x, y, buttons }: { x: number; y: number; buttons: string[] }) {
  const w = 86;
  const h = 116;
  const mid = x + w / 2;
  const button = (index: number) => buttons[index] ? actionOf(buttons[index]) : '';
  const click = button(2);
  return (
    <g data-duke3d-pad-mouse>
      <title>
        {`Left button: ${button(0) || 'nothing'}. Right button: ${button(1) || 'nothing'}. Middle button: ${click || 'nothing'}. Wheel up: ${button(3) || 'nothing'}, down: ${
          button(4) || 'nothing'
        }.`}
      </title>
      <rect x={x} y={y} width={w} height={h} rx={40} fill='#1f1f23' stroke='#55555e' strokeWidth={1.5} />
      <path d={`M ${x} ${y + 50} H ${x + w} M ${mid} ${y} V ${y + 50}`} stroke='#55555e' strokeWidth={1.5} />
      <rect x={mid - 5} y={y + 12} width={10} height={24} rx={5} fill='#55555e' />
      <Name x={x + 21} y={y + 32}>{button(0)}</Name>
      <Name x={x + w - 21} y={y + 32}>{button(1)}</Name>
      <Name x={mid} y={y + 66}>{`▲ ${button(3) || 'nothing'}`}</Name>
      <Name x={mid} y={y + 79}>{`▼ ${button(4) || 'nothing'}`}</Name>
      {click && <Name x={mid} y={y + 95}>{`● ${click}`}</Name>}
    </g>
  );
}

/**
 * The keys and the mouse `game` is played with, the player's keys on them. Beside forward and back
 * are the keys that strafe when the player strafes with letters, as WASD does, and those that turn
 * otherwise, as Duke's arrows do. An `editable` one, in the Keys dialog, says under it where in the
 * game's menu they are changed.
 */
export function DukePad({ editable, game }: { game: Game; editable: boolean }) {
  const binds = bindsFor(useConfig(game));
  const keys = (name: string) => keysOf(binds, name);
  const strafes = /^[A-Z]$/.test(keys('Strafe_Left')[0] ?? '');
  const left = strafes ? 'Strafe_Left' : 'Turn_Left';
  const right = strafes ? 'Strafe_Right' : 'Turn_Right';
  return (
    <Box data-duke3d-pad style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 4 }}>
      <svg viewBox='0 0 320 124' width='100%' style={{ display: 'block', overflow: 'visible', filter: 'drop-shadow(0 4px 10px rgba(0,0,0,0.45))' }}>
        <Cap x={78} y={2} w={34} action='Forward' keys={keys('Move_Forward')} />
        <Cap x={0} y={42} w={34} action='Fire' keys={keys('Fire')} />
        <Cap x={40} y={42} w={34} action={strafes ? 'Left' : 'Turn L'} keys={keys(left)} />
        <Cap x={78} y={42} w={34} action='Back' keys={keys('Move_Backward')} />
        <Cap x={116} y={42} w={34} action={strafes ? 'Right' : 'Turn R'} keys={keys(right)} />
        <Cap x={162} y={42} w={34} action='Jump' keys={keys('Jump')} />
        <Cap x={0} y={82} w={60} action='Run' keys={keys('Run')} />
        <Cap x={66} y={82} w={90} action='Open' keys={keys('Open')} />
        <Cap x={162} y={82} w={34} action='Crouch' keys={keys('Crouch')} />
        <Mouse x={222} y={4} buttons={binds.buttons} />
      </svg>
      {editable && (
        <Text size='xs' c='gray.3' ta='center' data-duke3d-pad-hint>
          The keys are changed in the game, in Options › Setup Keyboard, the mouse in Options › Setup Mouse › Button Setup.
        </Text>
      )}
    </Box>
  );
}
