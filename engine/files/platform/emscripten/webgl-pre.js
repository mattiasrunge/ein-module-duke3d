// EM_GL builds: fix up shader sources at the WebGL boundary, after the legacy GL
// emulation has rewritten them. The emulation prepends declarations, so a desktop
// "#version 120" or "#extension" line ends up after code, which ESSL rejects.
// Desktop versions and ARB extension lines are dropped and the rest move to the top.
// On WebGL 2 the resulting ESSL 1.00 source is turned into ESSL 3.00, since WebGL 2
// offers no derivatives (fwidth) to ESSL 1.00; there texture2DGradARB becomes the
// core textureGrad. On WebGL 1, shaders using derivatives get GL_OES_standard_derivatives.
(function () {
  function fix(src, webgl2, fragment) {
    var version = '', exts = [], body = [];
    src.split('\n').forEach(function (line) {
      var t = line.trim();
      if (/^#\s*version\b/.test(t)) {
        if (/300\s+es/.test(t)) version = t + '\n';
      } else if (/^#\s*extension\b/.test(t)) {
        if (!/GL_ARB_/.test(t)) exts.push(t);
      } else {
        body.push(line);
      }
    });
    body = body.join('\n');
    if (webgl2 && !version) {
      version = '#version 300 es\n';
      exts = exts.filter(function (e) { return !/standard_derivatives|shader_texture_lod/.test(e); });
      body = body.replace(/#\s*ifdef\s+GL_ARB_shader_texture_lod/g, '#if 1');
      if (fragment) {
        body = body.replace(/\bvarying\b/g, 'in')
          .replace(/\btexture2DGrad(ARB|EXT)\b/g, 'textureGrad')
          .replace(/\btexture2DLod(EXT)?\b/g, 'textureLod')
          .replace(/\b(texture2D|textureCube)\b/g, 'texture')
          .replace(/\bgl_FragData\s*\[\s*0\s*\]|\bgl_FragColor\b/g, 'fragColor0');
        exts.push('out highp vec4 fragColor0;');
      } else {
        body = body.replace(/\battribute\b/g, 'in').replace(/\bvarying\b/g, 'out');
      }
    } else if (
      !version && /\b(fwidth|dFdx|dFdy)\s*\(/.test(body) &&
      !exts.some(function (e) {
        return /standard_derivatives/.test(e);
      })
    ) {
      exts.push('#extension GL_OES_standard_derivatives : enable');
    }
    return version + (exts.length ? exts.join('\n') + '\n' : '') + body;
  }
  [typeof WebGLRenderingContext != 'undefined' && WebGLRenderingContext, typeof WebGL2RenderingContext != 'undefined' && WebGL2RenderingContext].forEach(function (C) {
    if (!C) return;
    var orig = C.prototype.shaderSource;
    C.prototype.shaderSource = function (shader, src) {
      var webgl2 = typeof WebGL2RenderingContext != 'undefined' && this instanceof WebGL2RenderingContext;
      var out = fix(src, webgl2, this.getShaderParameter(shader, this.SHADER_TYPE) == this.FRAGMENT_SHADER);
      if (Module.webglShaderLog) Module.webglShaderLog.push(out);
      return orig.call(this, shader, out);
    };
    var origCompile = C.prototype.compileShader;
    C.prototype.compileShader = function (shader) {
      origCompile.call(this, shader);
      if (!this.getShaderParameter(shader, this.COMPILE_STATUS)) {
        console.error('[webgl] shader compile failed: ' + this.getShaderInfoLog(shader) + '\n' + this.getShaderSource(shader));
      }
    };
    var origLink = C.prototype.linkProgram;
    C.prototype.linkProgram = function (program) {
      origLink.call(this, program);
      if (!this.getProgramParameter(program, this.LINK_STATUS)) {
        console.error('[webgl] program link failed: ' + this.getProgramInfoLog(program));
      }
    };
  });
})();

// The GL emulation calls warnOnce (glShadeModel, glTexEnv...) without declaring it as
// a dependency, so it is missing from release builds.
var warnOnce = function (text) {
  warnOnce.shown = warnOnce.shown || {};
  if (!warnOnce.shown[text]) {
    warnOnce.shown[text] = 1;
    console.warn(text);
  }
};
