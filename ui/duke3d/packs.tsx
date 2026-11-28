import { Badge, Button, Group, Loader, Stack, Text } from '@mantine/core';
import { Dialog } from '@ein/ui/components';
import { formatBytes, shq } from '@ein/ui/lib';
import { currentUser, execCommand, notify } from '@ein/ui/signals';
import { useCallback, useEffect, useState } from 'react';
import type { Game, Pack } from '../../lib/api.ts';

/** Where the packs fetched are (`lib/packs.ts`). */
const PACKS = '/groups/users/duke3d/.packs';

const messageOf = (error: unknown): string => error instanceof Error ? error.message : String(error);

/**
 * Packs, which ⋮ on a game's page opens: the add-on packs the module offers (`duke3d-packs`), what
 * each is and its size, and whether it is fetched; Fetch for an administrator, whom only fetching
 * from the internet is for, and Remove. Every game loads the packs fetched, from its next start on.
 */
export function PacksDialog({ opened, onClose }: { game: Game; opened: boolean; onClose(): void }) {
  const admin = currentUser.value?.admin === true;
  const [packs, setPacks] = useState<Pack[]>();
  const [busy, setBusy] = useState<string>();
  const refresh = useCallback(() => {
    execCommand<Pack[]>('duke3d-packs').then(setPacks, (error) => notify({ message: `The packs were not listed: ${messageOf(error)}`, color: 'red' }));
  }, []);
  useEffect(() => {
    if (opened) refresh();
  }, [opened, refresh]);

  const run = async (pack: Pack, command: string, failed: string) => {
    setBusy(pack.id);
    try {
      await execCommand(command);
    } catch (error) {
      notify({ message: `${failed}: ${messageOf(error)}`, color: 'red' });
    } finally {
      setBusy(undefined);
      refresh();
    }
  };

  return (
    <Dialog opened={opened} onClose={onClose} title='Packs' icon='package' size='lg'>
      <Stack gap='md' data-duke3d-packs>
        <Text size='sm' c='dimmed'>
          Add-ons made for EDuke32 by Duke's players, fetched from their makers' downloads only when chosen here. Every game loads the packs fetched, from the next time it
          starts.
        </Text>
        {!packs ? <Loader size='sm' /> : packs.map((pack) => (
          <Stack key={pack.id} gap={4} data-duke3d-pack={pack.id}>
            <Group justify='space-between' wrap='nowrap'>
              <Group gap='xs' wrap='nowrap'>
                <Text fw={600}>{pack.title}</Text>
                <Text size='sm' c='dimmed'>{formatBytes(pack.size)}</Text>
                {pack.fetched && <Badge size='sm' variant='light' color='green'>Fetched</Badge>}
              </Group>
              {pack.fetched
                ? (
                  <Button
                    size='xs'
                    variant='subtle'
                    color='red'
                    loading={busy === pack.id}
                    onClick={() => void run(pack, `rm ${shq(`${PACKS}/${pack.file}`)}`, `${pack.title} was not removed`)}
                  >
                    Remove
                  </Button>
                )
                : admin
                ? (
                  <Button size='xs' variant='light' loading={busy === pack.id} onClick={() => void run(pack, `duke3d-fetch ${shq(pack.id)}`, `${pack.title} was not fetched`)}>
                    Fetch
                  </Button>
                )
                : <Text size='xs' c='dimmed' fs='italic'>An administrator can fetch it</Text>}
            </Group>
            <Text size='sm'>{pack.about}</Text>
          </Stack>
        ))}
      </Stack>
    </Dialog>
  );
}
