// EIN builds: what einlayer.cpp needs of Emscripten's own JavaScript.
addToLibrary({
  // The worker's canvas (Module.canvas, an OffscreenCanvas) as "!canvas", where Emscripten's WebGL
  // and canvas functions find it.
  ein_canvas__deps: ['$specialHTMLTargets'],
  ein_canvas: () => {
    specialHTMLTargets['!canvas'] = Module.canvas;
  },
});
