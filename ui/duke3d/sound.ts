/**
 * The game's sound on Web Audio. The game mixes its sound effects and its music itself, the effects
 * with Apogee's mixer and the music on an emulated OPL3, as a Sound Blaster 16 played them
 * (`engine/dsl_ein.c`, `engine/music_ein.c`); a processor node asks it for samples as it needs them.
 */

/** Frames the sound is asked for at a time: about 43 ms at 48 kHz. */
const MIX_FRAMES = 2048;

/** The game's audio: the samples once they can be pulled, and the controls the page has. */
export type Sound = {
  context: AudioContext;
  /** Pull the sound from `render`, which fills `left` and `right` with the next samples. */
  effects(render: (left: Float32Array, right: Float32Array) => void): void;
  muted(): boolean;
  setMuted(muted: boolean): void;
  /** Hold the sound while the page is hidden, and go on when it shows again. */
  hold(held: boolean): void;
  close(): void;
};

/**
 * Open the game's audio. Call it from the click that starts the game: a browser lets a page make
 * sound only after the person has done something on it.
 */
export function openSound(): Sound {
  const context = new AudioContext();
  const master = context.createGain();
  master.connect(context.destination);
  let processor: ScriptProcessorNode | undefined;

  return {
    context,
    effects(render) {
      // A ScriptProcessorNode runs on the page's own thread, where the engine is: an AudioWorklet
      // would need the engine on the audio thread too.
      processor = context.createScriptProcessor(MIX_FRAMES, 0, 2);
      processor.onaudioprocess = (event) => render(event.outputBuffer.getChannelData(0), event.outputBuffer.getChannelData(1));
      processor.connect(master);
    },
    muted: () => master.gain.value === 0,
    setMuted(muted) {
      master.gain.value = muted ? 0 : 1;
    },
    hold(held) {
      if (held) void context.suspend();
      else void context.resume();
    },
    close() {
      if (processor) {
        processor.onaudioprocess = null;
        processor.disconnect();
      }
      void context.close();
    },
  };
}
