import { type RefObject, useEffect } from 'react';
import type { Engine } from './engine.ts';

/**
 * Run `engine` while it is set: on every animation frame, a blit onto `canvas` when the game drew,
 * at the frame's size, then the game's next turn. A hidden page draws no frames, so the game stands
 * still while it is hidden.
 */
export function useGameLoop(engine: Engine | null, canvas: RefObject<HTMLCanvasElement | null>) {
  useEffect(() => {
    const context = canvas.current?.getContext('2d');
    if (!engine || !context) return;
    let image: ImageData | undefined;
    let drawn = -1;
    let request = 0;
    const step = () => {
      const frames = engine.frames();
      const width = engine.width();
      const height = engine.height();
      if (frames !== drawn && width > 0 && height > 0) {
        drawn = frames;
        if (image?.width !== width || image.height !== height) {
          image = context.createImageData(width, height);
          context.canvas.width = width;
          context.canvas.height = height;
        }
        engine.draw(image);
        context.putImageData(image, 0, 0);
      }
      engine.turn();
      request = requestAnimationFrame(step);
    };
    request = requestAnimationFrame(step);
    return () => cancelAnimationFrame(request);
  }, [engine]);
}
