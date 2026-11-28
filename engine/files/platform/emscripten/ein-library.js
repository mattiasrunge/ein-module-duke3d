// EIN builds: what einlayer.cpp needs of Emscripten's own JavaScript.
addToLibrary({
  // The worker's canvas (Module.canvas, an OffscreenCanvas) as "!canvas", where Emscripten's WebGL
  // and canvas functions find it.
  ein_canvas__deps: ['$specialHTMLTargets'],
  ein_canvas: () => {
    specialHTMLTargets['!canvas'] = Module.canvas;
  },
  // The legacy GL emulation starts on the context made current: Emscripten's own context makers
  // (Browser.createContext, EGL) run these callbacks, emscripten_webgl_create_context does not.
  ein_gl_started__deps: ['$Browser'],
  ein_gl_started: () => {
    Browser.useWebGL = true;
    Browser.moduleContextCreatedCallbacks.forEach((callback) => callback());
  },
});
