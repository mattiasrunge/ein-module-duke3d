import { ActionIcon, Box, Button, Loader, Menu, Paper, ScrollArea, Stack, Text, Tooltip, UnstyledButton } from '@mantine/core';
import { EntityDetailHeader, TablerIcon } from '@ein/ui/components';
import { usePhone } from '@ein/ui/hooks';
import { formatBytes } from '@ein/ui/lib';
import { currentUser, notify, uploadBatchDone } from '@ein/ui/signals';
import { useCallback, useEffect, useRef, useState } from 'react';
import { type Engine, startEngine } from '../duke3d/engine.ts';
import { deleteGame, fetchGame, type Game, GAMES, listGames, type Offer, OFFERS } from '../duke3d/games.ts';
import { mouseBit, scancode, WHEEL_DOWN, WHEEL_UP } from '../duke3d/keys.ts';
import { useGameLoop } from '../duke3d/loop.ts';
import { keeper, loadKept, savesFolder } from '../duke3d/saves.ts';
import { openSound, type Sound } from '../duke3d/sound.ts';
import { titlePicture } from '../duke3d/titlepic.ts';

export const handle = {
  fullBleed: true,
  title: 'Duke Nukem 3D',
  pattern: 'mixed',
};

/** Where the game is: waiting for the click that starts it, loading, playing, or stopped by an error. */
type State =
  | { kind: 'idle' }
  | { kind: 'loading'; game: Game }
  | { kind: 'running'; game: Game }
  | { kind: 'error'; game: Game; message: string };

/** How many of the game's last printed lines an error shows, since the game prints its reason before it exits. */
const LOG_TAIL = 6;

/** The browser remembers the game last chosen and whether the sound was off. */
const CHOSEN_KEY = 'duke3d.game';
const MUTED_KEY = 'duke3d.muted';

const remembered = (key: string): string | null => {
  try {
    return globalThis.localStorage.getItem(key);
  } catch {
    return null;
  }
};

const remember = (key: string, value: string) => {
  try {
    globalThis.localStorage.setItem(key, value);
  } catch {
    // A browser that keeps nothing chooses again next time.
  }
};

/** The bar's round buttons: finger-sized on a phone, as the app header's are. */
const BAR_BUTTON = { size: 36, icon: 22 };

/**
 * The module's page: the games there are in a box at the left, as Chat's recent chats are, and the
 * game beside them. A click on a game plays it, which is also the click a browser wants before a
 * page may make sound. A folder with a GRP dropped anywhere on the page is uploaded into `GAMES` as
 * a game, and the free games not here yet are offered to fetch. A click on the running game hands
 * it the mouse (the browser's pointer lock), which aims and fires; while the game has the focus or
 * the mouse it takes every key it knows before the shell does. A game saved from the game's menu,
 * and the player's settings, are kept in their home (`savesFolder`), and Load lists the saves next
 * time.
 */
export default function Duke3D() {
  const phone = usePhone();
  const frame = useRef<HTMLDivElement>(null);
  const canvas = useRef<HTMLCanvasElement>(null);
  const engine = useRef<Engine | null>(null);
  const sound = useRef<Sound | null>(null);
  const log = useRef<string[]>([]);
  const [games, setGames] = useState<Game[] | null>(null);
  const [listError, setListError] = useState<string>();
  const [chosen, setChosen] = useState(() => remembered(CHOSEN_KEY));
  const [state, setState] = useState<State>({ kind: 'idle' });
  const [fullscreen, setFullscreen] = useState(false);
  const [locked, setLocked] = useState(false);
  const [muted, setMuted] = useState(() => remembered(MUTED_KEY) === '1');

  // The game to offer: the one chosen last, else the first, the shareware episode when it is there.
  const selected = games?.find((game) => game.name === chosen) ?? games?.[0];
  const [cover, setCover] = useState<string>();
  // What the header names: the game playing or loading, else the one Play would start.
  const shown = state.kind === 'loading' || state.kind === 'running' ? state.game : selected;

  // The header shows the title screen of the game chosen, and keeps the last one while the next is read.
  useEffect(() => {
    if (!selected) return;
    let current = true;
    void titlePicture(selected).then((picture) => {
      if (current && picture) setCover(picture);
    });
    return () => {
      current = false;
    };
  }, [selected?.name, selected?.size]);

  const refresh = useCallback(() => {
    listGames().then((found) => {
      setGames(found);
      setListError(undefined);
    }).catch((error) => setListError(error instanceof Error ? error.message : String(error)));
  }, []);
  useEffect(refresh, [refresh]);
  // Uploads land in the games folder: a game dropped on the page.
  useEffect(() =>
    uploadBatchDone.subscribe((batch) => {
      if (
        batch?.dirs.some((dir) => dir === GAMES || dir.startsWith(`${GAMES}/`))
      ) refresh();
    }), [refresh]);

  // The offered games being fetched, by name: each is a command that runs as long as its download.
  const [fetching, setFetching] = useState<string[]>([]);
  const fetchOffer = useCallback(async (offer: Offer) => {
    setFetching((names) => [...names, offer.name]);
    try {
      await fetchGame(offer.name);
    } catch (error) {
      notify({
        message: `${offer.title} was not fetched: ${error instanceof Error ? error.message : String(error)}`,
        color: 'red',
      });
    } finally {
      setFetching((names) => names.filter((name) => name !== offer.name));
      refresh();
    }
  }, [refresh]);

  const stop = useCallback(() => {
    // The player's settings are kept as quitting keeps them, when the game is still there to write them.
    try {
      engine.current?.writeConfig();
    } catch {
      // A game that already ended wrote them when it did.
    }
    // The game is given no more turns, and goes with its last references.
    engine.current = null;
    sound.current?.close();
    sound.current = null;
    if (document.pointerLockElement === frame.current) {
      document.exitPointerLock();
    }
  }, []);
  useEffect(() => stop, [stop]);
  // Closing or reloading the tab unmounts nothing, so the player's settings are written as the page
  // goes; the game runs on in a page brought back from the browser's cache.
  useEffect(() => {
    const onHide = () => {
      try {
        engine.current?.writeConfig();
      } catch {
        // A game that already ended wrote them when it did.
      }
    };
    globalThis.addEventListener('pagehide', onHide);
    return () => globalThis.removeEventListener('pagehide', onHide);
  }, []);

  const play = useCallback(async (game: Game) => {
    stop();
    setChosen(game.name);
    remember(CHOSEN_KEY, game.name);
    log.current = [];
    setState({ kind: 'loading', game });
    frame.current?.focus();
    // Opened in the click, before anything is awaited: a browser lets a page make sound only then.
    const audio = openSound();
    audio.setMuted(muted);
    sound.current = audio;
    try {
      // Someone signed in plays with their saves and settings of this game; without an account they
      // last as long as the page.
      const username = currentUser.value?.username;
      const folder = username ? savesFolder(username, game.name) : undefined;
      const kept = folder ? await loadKept(folder) : [];
      const started = await startEngine({
        game,
        sound: audio,
        kept,
        ...(folder
          ? {
            onKeep: keeper(folder, (error) =>
              notify({
                message: `The game was not saved: ${error instanceof Error ? error.message : String(error)}`,
                color: 'red',
              })),
          }
          : {}),
        log: (line) => {
          log.current = [...log.current.slice(-(LOG_TAIL - 1)), line];
          console.info(`[duke3d] ${line}`);
        },
        onEnd: (end) => {
          // Another game was chosen since, or the page stopped this one.
          if (sound.current !== audio) return;
          // The game has ended: it wrote its settings itself when the player quit.
          engine.current = null;
          stop();
          // Quit from the game's menu leaves the page as it was before Play.
          if ('status' in end && end.status === 0) setState({ kind: 'idle' });
          else {setState({
              kind: 'error',
              game,
              message: describe(end, log.current),
            });}
        },
      });
      // Another game was chosen while this one loaded, or it ended while it started.
      if (sound.current !== audio) return;
      engine.current = started;
      setState({ kind: 'running', game });
      frame.current?.focus();
    } catch (error) {
      if (sound.current !== audio) return;
      stop();
      setState({
        kind: 'error',
        game,
        message: describe({ error }, log.current),
      });
    }
  }, [muted, stop]);

  const remove = useCallback(async (game: Game) => {
    if (
      !globalThis.confirm(
        `Delete ${game.title} (${game.name})? Everyone who plays here loses it.`,
      )
    ) return;
    if (state.kind !== 'idle' && state.game.name === game.name) {
      stop();
      setState({ kind: 'idle' });
    }
    try {
      await deleteGame(game.name);
    } catch (error) {
      setListError(error instanceof Error ? error.message : String(error));
    }
    refresh();
  }, [state, stop, refresh]);

  useGameLoop(state.kind === 'running' ? engine.current : null, canvas);

  // A hidden page draws no frames, so the game stands still; its sound waits with it.
  useEffect(() => {
    const onVisibility = () => sound.current?.hold(document.hidden);
    document.addEventListener('visibilitychange', onVisibility);
    return () => document.removeEventListener('visibilitychange', onVisibility);
  }, []);

  // Keys and the mouse, in the capture phase on the window so the shell's own listeners (on the
  // document, and the window's bubbling phase) never see them while the game has the focus or the mouse.
  useEffect(() => {
    if (state.kind !== 'running') return;
    const node = frame.current;
    const heldKeys = new Set<number>();
    const heldButtons = new Set<number>();
    const captured = () => !!node && document.pointerLockElement === node;
    const focused = () => captured() || (!!node && node.contains(document.activeElement));
    const onKey = (event: KeyboardEvent) => {
      if (!engine.current || !focused() || event.metaKey) return;
      const key = scancode(event.code);
      if (key === undefined) return;
      event.preventDefault();
      event.stopImmediatePropagation();
      const pressed = event.type === 'keydown';
      if (pressed && event.repeat) return;
      if (pressed) heldKeys.add(key);
      else heldKeys.delete(key);
      engine.current.key(pressed, key);
    };
    // The first click hands the game the mouse; after that the buttons are the game's.
    const onMouseDown = (event: MouseEvent) => {
      if (!node?.contains(event.target as Node)) return;
      if (!captured()) {
        node.focus();
        void Promise.resolve(node.requestPointerLock()).catch(() => undefined);
        return;
      }
      const bit = mouseBit(event.button);
      if (bit === undefined) return;
      event.preventDefault();
      heldButtons.add(bit);
      engine.current?.mouseButton(true, bit);
    };
    const onMouseUp = (event: MouseEvent) => {
      const bit = mouseBit(event.button);
      if (bit === undefined || !heldButtons.has(bit)) return;
      heldButtons.delete(bit);
      engine.current?.mouseButton(false, bit);
    };
    const onMouseMove = (event: MouseEvent) => {
      if (captured()) engine.current?.mouse(event.movementX, event.movementY);
    };
    // The wheel is a click that only goes down, as BUILD read it.
    const onWheel = (event: WheelEvent) => {
      if (!captured() || event.deltaY === 0) return;
      event.preventDefault();
      engine.current?.mouseButton(
        true,
        event.deltaY < 0 ? WHEEL_UP : WHEEL_DOWN,
      );
    };
    const onContextMenu = (event: MouseEvent) => {
      if (node?.contains(event.target as Node)) event.preventDefault();
    };
    // A key or button held while the game loses the focus or the mouse would never come up for it: let go of them all.
    const release = () => {
      for (const key of heldKeys) engine.current?.key(false, key);
      for (const bit of heldButtons) engine.current?.mouseButton(false, bit);
      heldKeys.clear();
      heldButtons.clear();
    };
    const onLockChange = () => {
      setLocked(captured());
      if (!captured()) release();
    };
    globalThis.addEventListener('keydown', onKey, { capture: true });
    globalThis.addEventListener('keyup', onKey, { capture: true });
    globalThis.addEventListener('mousedown', onMouseDown, { capture: true });
    globalThis.addEventListener('mouseup', onMouseUp, { capture: true });
    globalThis.addEventListener('mousemove', onMouseMove, { capture: true });
    globalThis.addEventListener('wheel', onWheel, {
      capture: true,
      passive: false,
    });
    globalThis.addEventListener('contextmenu', onContextMenu, {
      capture: true,
    });
    globalThis.addEventListener('blur', release);
    document.addEventListener('pointerlockchange', onLockChange);
    node?.addEventListener('focusout', release);
    return () => {
      globalThis.removeEventListener('keydown', onKey, { capture: true });
      globalThis.removeEventListener('keyup', onKey, { capture: true });
      globalThis.removeEventListener('mousedown', onMouseDown, {
        capture: true,
      });
      globalThis.removeEventListener('mouseup', onMouseUp, { capture: true });
      globalThis.removeEventListener('mousemove', onMouseMove, {
        capture: true,
      });
      globalThis.removeEventListener('wheel', onWheel, { capture: true });
      globalThis.removeEventListener('contextmenu', onContextMenu, {
        capture: true,
      });
      globalThis.removeEventListener('blur', release);
      document.removeEventListener('pointerlockchange', onLockChange);
      node?.removeEventListener('focusout', release);
      setLocked(false);
    };
  }, [state]);

  useEffect(() => {
    const onChange = () => setFullscreen(document.fullscreenElement === frame.current);
    document.addEventListener('fullscreenchange', onChange);
    return () => document.removeEventListener('fullscreenchange', onChange);
  }, []);

  const toggleFullscreen = () => {
    if (document.fullscreenElement) void document.exitFullscreen();
    else {
      void frame.current?.requestFullscreen().then(() => frame.current?.requestPointerLock()).catch(() => undefined);
    }
    frame.current?.focus();
  };

  const toggleMuted = () => {
    const next = !muted;
    setMuted(next);
    remember(MUTED_KEY, next ? '1' : '0');
    sound.current?.setMuted(next);
    frame.current?.focus();
  };

  const list = {
    games,
    error: listError,
    active: state.kind === 'loading' || state.kind === 'running' ? state.game.name : selected?.name,
    onPlay: play,
    fetching,
    onFetch: fetchOffer,
  };

  return (
    <Paper
      radius={0}
      bg='var(--mrx-surface-base)'
      style={{
        display: 'flex',
        flexDirection: 'column',
        height: 'calc(100dvh - var(--app-shell-header-offset, 48px))',
        minHeight: phone ? undefined : 420,
        overflow: 'hidden',
      }}
    >
      <EntityDetailHeader
        eyebrow={shown ? 'Duke Nukem 3D' : undefined}
        title={shown?.title ?? 'Duke Nukem 3D'}
        name='Duke Nukem 3D'
        pattern='mixed'
        flowContent
        // The game's own title screen, read out of its GRP.
        coverArt={cover
          ? (
            <img
              src={cover}
              alt=''
              style={{
                width: '100%',
                height: '100%',
                objectFit: 'cover',
                imageRendering: 'pixelated',
              }}
            />
          )
          : undefined}
        controls={phone ? <GameMenu {...list} /> : undefined}
        actions={{
          upload: { path: GAMES, name: 'Duke Nukem 3D', keepFolders: true },
          items: [
            {
              label: muted ? 'Sound on' : 'Sound off',
              icon: muted ? 'volume-off' : 'volume',
              onClick: toggleMuted,
              button: true,
              iconOnly: true,
              stays: true,
            },
            {
              label: 'Fullscreen',
              icon: 'maximize',
              onClick: toggleFullscreen,
              disabled: state.kind !== 'running',
              button: true,
              iconOnly: true,
              stays: true,
            },
          ],
        }}
      />
      <Box style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        <GamePane {...list} onDelete={remove} />
        <Box
          ref={frame}
          tabIndex={0}
          data-duke3d-state={state.kind}
          onMouseDown={() => frame.current?.focus()}
          p={fullscreen ? 0 : 'md'}
          style={{
            flex: 1,
            minWidth: 0,
            outline: 'none',
            containerType: 'size',
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            gap: 'var(--mantine-spacing-xs)',
            background: fullscreen ? '#000' : undefined,
          }}
        >
          {/* 4:3, as large as fits beside the line under it, whatever mode the game draws in: 320 by 200 was shown at 4:3 too. */}
          <Box
            pos='relative'
            style={{
              width: fullscreen ? 'min(100cqw, 133.333cqh)' : 'min(100cqw, calc((100cqh - 2.5rem) * 4 / 3))',
              aspectRatio: '4 / 3',
              background: '#000',
              flex: 'none',
            }}
          >
            <canvas
              ref={canvas}
              width={640}
              height={480}
              // The pointer would stand over the game's view; once the game has the mouse the browser hides it anyway.
              style={{
                width: '100%',
                height: '100%',
                imageRendering: 'pixelated',
                display: 'block',
                cursor: state.kind === 'running' ? 'none' : undefined,
              }}
            />
            {state.kind !== 'running' && (
              <Stack
                pos='absolute'
                inset={0}
                align='center'
                justify='center'
                gap='md'
                p='md'
                // While a game loads, and after it quit or stopped, its last frame stays dimmed behind.
                bg='rgba(0, 0, 0, 0.75)'
                style={{ overflow: 'auto' }}
              >
                {state.kind === 'error' && (
                  <Stack gap={4} maw={640} w='100%' data-duke3d-error>
                    <Text c='red.5' fw={600}>{state.game.title} stopped</Text>
                    <Text
                      c='gray.3'
                      size='sm'
                      style={{
                        whiteSpace: 'pre-wrap',
                        fontFamily: 'monospace',
                      }}
                    >
                      {state.message}
                    </Text>
                  </Stack>
                )}
                {state.kind === 'loading' ? <Loader color='yellow' /> : selected
                  ? (
                    <UnstyledButton
                      onClick={() => void play(selected)}
                      data-duke3d-play
                      style={{ textAlign: 'center' }}
                    >
                      <Stack align='center' gap={6}>
                        <TablerIcon
                          name='player-play'
                          size={56}
                          color='var(--mantine-color-yellow-5)'
                        />
                        <Text c='gray.3' fw={600}>
                          {state.kind === 'error' ? 'Play again' : 'Play'} {selected.title}
                        </Text>
                      </Stack>
                    </UnstyledButton>
                  )
                  : (
                    <Text c='gray.4' ta='center' size='sm'>
                      {games ? 'No games yet. Fetch the free one from the list of games, or drop a folder with a DUKE3D.GRP on the page.' : ''}
                    </Text>
                  )}
              </Stack>
            )}
          </Box>
          {!fullscreen && (
            <Text c='dimmed' size='xs' ta='center' lineClamp={2} maw={960}>
              {state.kind === 'running' && !locked
                ? 'Click the game to give it the mouse and the keyboard. '
                : state.kind === 'running'
                ? 'Escape gives the mouse back. '
                : ''}
              The mouse aims and fires, the arrows move, Ctrl fires, Space opens, A jumps, Z crouches, Shift runs, F1 is the help, Escape the menu.
            </Text>
          )}
        </Box>
      </Box>
    </Paper>
  );
}

type ListProps = {
  games: Game[] | null;
  error?: string;
  /** The game playing, loading, or chosen to play next. */
  active?: string;
  onPlay: (game: Game) => void;
  /** The offered games being fetched, by name. */
  fetching: string[];
  onFetch: (offer: Offer) => void;
};

/**
 * The games, each a row that plays it, and after them the offered games not here yet, each with a
 * button that fetches it. In the pane beside the game (`onDelete`, which gives each game a button
 * that deletes it) a game's row is its title screen with its title under it; in the phone's menu,
 * its title alone. `onPick` hears a game chosen.
 */
function GameList(
  { games, error, active, onPlay, onDelete, onPick, fetching, onFetch }:
    & ListProps
    & { onDelete?: (game: Game) => void; onPick?: () => void },
) {
  if (error) return <Text size='sm' c='red' p='sm'>{error}</Text>;
  if (!games) return <Loader size='sm' m='sm' />;
  const offers = OFFERS.filter((offer) => !games.some((game) => game.name === offer.name));
  return (
    <Stack gap='md'>
      {games.length === 0
        ? (
          <Text size='sm' c='dimmed' p='sm' pb={0}>
            No games yet. Fetch the free one below, or drop a folder with your own DUKE3D.GRP on the page.
          </Text>
        )
        : (
          <Games
            games={games}
            active={active}
            onPlay={onPlay}
            onDelete={onDelete}
            onPick={onPick}
          />
        )}
      {offers.length > 0 && (
        <Stack gap='xs' px={onDelete ? 0 : 'xs'} pb='xs'>
          <Text size='xs' c='dimmed' fw={600} tt='uppercase'>
            Free to fetch
          </Text>
          {offers.map((offer) => (
            <Stack key={offer.name} gap={4} data-duke3d-offer={offer.name}>
              <Text size='sm'>{offer.title}</Text>
              <Text size='xs' c='dimmed'>{offer.about}</Text>
              <Button
                size='xs'
                variant='light'
                color='yellow'
                leftSection={<TablerIcon name='download' size={14} />}
                loading={fetching.includes(offer.name)}
                onClick={() => onFetch(offer)}
                style={{ alignSelf: 'flex-start' }}
              >
                Fetch · {formatBytes(offer.download)}
              </Button>
            </Stack>
          ))}
        </Stack>
      )}
    </Stack>
  );
}

/** The games' rows: `GameList`'s. */
function Games(
  { games, active, onPlay, onDelete, onPick }:
    & Pick<ListProps, 'active' | 'onPlay'>
    & {
      games: Game[];
      onDelete?: (game: Game) => void;
      onPick?: () => void;
    },
) {
  const detail = (game: Game) => `${game.name} · ${formatBytes(game.size)}`;
  return (
    <Stack gap={onDelete ? 'xs' : 2}>
      {games.map((game) => (
        <UnstyledButton
          key={game.name}
          onClick={() => {
            onPick?.();
            onPlay(game);
          }}
          data-duke3d-game={game.name}
          style={{
            padding: onDelete ? 6 : '8px 10px',
            borderRadius: 'var(--mantine-radius-md)',
            background: game.name === active ? 'var(--mrx-surface-raised)' : undefined,
            outline: game.name === active && onDelete ? '2px solid var(--mantine-color-yellow-5)' : undefined,
            outlineOffset: -2,
          }}
        >
          {onDelete && (
            <Box pos='relative' mb={6}>
              <TitleArt game={game} />
              <Tooltip label='Delete game'>
                <ActionIcon
                  pos='absolute'
                  top={6}
                  right={6}
                  variant='filled'
                  color='dark'
                  size='sm'
                  component='span'
                  role='button'
                  onClick={(event) => {
                    event.stopPropagation();
                    onDelete(game);
                  }}
                  aria-label={`Delete ${game.name}`}
                >
                  <TablerIcon name='trash' size={14} />
                </ActionIcon>
              </Tooltip>
            </Box>
          )}
          <Text size='sm' truncate fw={game.name === active ? 600 : 400}>
            {game.title}
          </Text>
          <Text size='xs' c='dimmed' truncate>{detail(game)}</Text>
        </UnstyledButton>
      ))}
    </Stack>
  );
}

/** A game's title screen at 8:5, its 320 by 200, or a dark tile when it has none to show. */
function TitleArt({ game }: { game: Game }) {
  const [picture, setPicture] = useState<string | null>();
  useEffect(() => {
    let current = true;
    void titlePicture(game).then((found) => {
      if (current) setPicture(found ?? null);
    });
    return () => {
      current = false;
    };
  }, [game.name, game.size]);
  return (
    <Box
      style={{
        aspectRatio: '8 / 5',
        borderRadius: 'var(--mantine-radius-sm)',
        overflow: 'hidden',
        background: '#000',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      {picture
        ? (
          <img
            src={picture}
            alt=''
            style={{
              width: '100%',
              height: '100%',
              objectFit: 'cover',
              imageRendering: 'pixelated',
              display: 'block',
            }}
          />
        )
        : picture === null && (
          <TablerIcon
            name='/usr/modules/duke3d/assets/icon.svg'
            size={40}
            color='var(--mantine-color-dark-3)'
          />
        )}
    </Box>
  );
}

/** The games beside the game, in a bordered box in the page's margin as Chat's recent chats are. */
function GamePane(props: ListProps & { onDelete: (game: Game) => void }) {
  return (
    <Box
      w={300}
      h='100%'
      p='md'
      pr={0}
      style={{ flex: '0 0 300px', overflow: 'hidden' }}
      visibleFrom='sm'
    >
      <Paper h='100%' p='sm' radius='md' withBorder>
        <Stack gap='xs' h='100%'>
          <ScrollArea
            type='auto'
            scrollbars='y'
            offsetScrollbars='y'
            style={{ flex: 1, minHeight: 0 }}
          >
            <GameList {...props} />
          </ScrollArea>
          <Text size='xs' c='dimmed'>
            Drop a folder with a game's GRP anywhere on the page to add it.
          </Text>
        </Stack>
      </Paper>
    </Box>
  );
}

/** On a phone, where there is no room for the box beside the game, the games are a menu in the bar. */
function GameMenu(props: ListProps) {
  const [opened, setOpened] = useState(false);
  return (
    <Menu
      shadow='md'
      width={320}
      position='bottom-end'
      opened={opened}
      onChange={setOpened}
    >
      <Menu.Target>
        <Tooltip label='Games'>
          <ActionIcon
            variant='subtle'
            size={BAR_BUTTON.size}
            aria-label='Games'
          >
            <TablerIcon name='list' size={BAR_BUTTON.icon} />
          </ActionIcon>
        </Tooltip>
      </Menu.Target>
      <Menu.Dropdown>
        <Menu.Label>Games</Menu.Label>
        <ScrollArea.Autosize
          mah={420}
          type='auto'
          scrollbars='y'
          px={4}
          styles={{ content: { minWidth: 0 } }}
        >
          <GameList {...props} onPick={() => setOpened(false)} />
        </ScrollArea.Autosize>
      </Menu.Dropdown>
    </Menu>
  );
}

/** How the game ended, and the last lines it printed, which say why when the game itself gave up. */
function describe(
  end: { status: number } | { error: unknown },
  lines: string[],
): string {
  // The game ends with exit(1) when it gives up (`Error`), and says why in the lines before.
  const message = 'status' in end ? `The game exited with status ${end.status}` : end.error instanceof Error ? end.error.message : String(end.error);
  return [message, ...lines].join('\n');
}
