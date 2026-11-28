// EIN builds: what einlayer.cpp needs of Emscripten's own JavaScript.
addToLibrary({
  // The worker's canvas (Module.canvas, an OffscreenCanvas) as "!canvas", where Emscripten's WebGL
  // and canvas functions find it.
  ein_canvas__deps: ['$specialHTMLTargets'],
  ein_canvas: () => {
    specialHTMLTargets['!canvas'] = Module.canvas;
  },

  // net_transport.h: the NetDuke32 netcode's wire, the worker's `Module.einDuke` (ui/duke3d/net.ts),
  // which carries it through the games module's relay. What is sent is copied out of the heap at
  // once; net_poll hands the netcode what came, in the order it came.
  net_send: (peer, channel, _reliable, data, length) => Module.einDuke?.send(peer, channel, HEAPU8.slice(data, data + length)),
  net_broadcast: (channel, _reliable, data, length) => Module.einDuke?.broadcast(channel, HEAPU8.slice(data, data + length)),
  net_poll: () => Module.einDuke?.poll(),
  net_kick: (peer) => Module.einDuke?.kick(peer),
  net_transport_init: () => {},
  net_transport_shutdown: () => Module.einDuke?.shutdown(),
});
