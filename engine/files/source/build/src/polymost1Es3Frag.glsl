#version 300 es
// polymost1Frag.glsl in GLSL ES 3.00, for WebGL 2: the fog, the alpha test and the texture
// coordinates come from glbuild_ein.cpp, through polymost1Es3Vert.glsl and uniforms.
precision highp float;

//include an additional space here so that we can programmatically search for and disable this preprocessor definition easily
 #define POLYMOST1_EXTENDED

//s_texture points to an indexed color texture
uniform sampler2D s_texture;
//s_palswap is the palette swap texture where u is the color index and v is the shade
uniform sampler2D s_palswap;
//s_palette is the base palette texture where u is the color index
uniform sampler2D s_palette;

#ifdef POLYMOST1_EXTENDED
uniform sampler2D s_detail;
uniform sampler2D s_glow;
#endif

//u_texturePosSize is the texture position & size packaged into a single vec4 as {pos.x, pos.y, size.x, size.y}
uniform vec4 u_texturePosSize;
uniform vec2 u_halfTexelSize;
uniform vec2 u_palswapPos;
uniform vec2 u_palswapSize;

uniform vec2 u_clamp;

uniform float u_shade;
uniform vec2 u_numShades;
uniform float u_visFactor;
uniform float u_fogEnabled;

uniform float u_useColorOnly;
uniform float u_usePalette;
uniform vec4 u_npotEmulation;
uniform float u_shadeInterpolate;

#ifdef POLYMOST1_EXTENDED
uniform float u_useDetailMapping;
uniform float u_useGlowMapping;
#endif

in vec4 v_color;
in float v_distance;
in vec4 v_texCoord0;
in vec4 v_texCoord3;
in vec4 v_texCoord4;
in float v_fogCoord;
//the fog's end and 1 / (end - start)
flat in vec2 v_fog;

uniform vec4 u_fogColor;

out vec4 o_color;

const float c_basepalScale = 255.0/256.0;
const float c_basepalOffset = 0.5/256.0;

const float c_zero = 0.0;
const float c_one = 1.0;
const float c_two = 2.0;
const vec4 c_vec4_one = vec4(c_one);
const float c_wrapThreshold = 0.9;

uniform vec4 u_colorCorrection;
//alpha test reference: a fragment at or below it is discarded; negative when the test is off
uniform float u_alphaRef;
const vec2 c_vec2_zero_one = vec2(c_zero, c_one);
const vec4 c_vec4_luma_709 = vec4(0.2126, 0.7152, 0.0722, 0.0);

void main()
{
    vec2 coord = mix(v_texCoord0.xy,v_texCoord0.yx,u_usePalette);
    float modCoordYnpotEmulationFactor = mod(coord.y,u_npotEmulation.y);
    coord.xy = vec2(floor(modCoordYnpotEmulationFactor)*u_npotEmulation.x+coord.x, floor(coord.y*u_npotEmulation.w)+modCoordYnpotEmulationFactor);
    vec2 newCoord = mix(v_texCoord0.xy,mix(coord.xy,coord.yx,u_usePalette),u_npotEmulation.z);
    vec2 texCoord = mix(fract(newCoord.xy), clamp(newCoord.xy, c_zero, c_one), u_clamp);
    texCoord = clamp(u_texturePosSize.zw*texCoord, u_halfTexelSize, u_texturePosSize.zw-u_halfTexelSize);
    vec4 color = textureGrad(s_texture, u_texturePosSize.xy+texCoord, dFdx(v_texCoord0.xy), dFdy(v_texCoord0.xy));

    float shade = clamp((u_shade+clamp(u_visFactor*v_distance-0.5*u_shadeInterpolate,c_zero,u_numShades.x)), c_zero, u_numShades.x-c_one);
    float shadeFrac = mod(shade, c_one);
    float colorIndex = texture(s_palswap, vec2(color.r, floor(shade)*u_numShades.y)*u_palswapSize+u_palswapPos).r * c_basepalScale + c_basepalOffset;
    float colorIndexNext = texture(s_palswap, u_palswapSize*vec2(color.r, (floor(shade)+c_one)*u_numShades.y)+u_palswapPos).r * c_basepalScale + c_basepalOffset;
    vec4 palettedColor = texture(s_palette, vec2(colorIndex, c_zero));
    vec4 palettedColorNext = texture(s_palette, vec2(colorIndexNext, c_zero));
    palettedColor.rgb = mix(palettedColor.rgb, palettedColorNext.rgb, shadeFrac*u_shadeInterpolate);
    float fullbright = mix(u_usePalette*palettedColor.a, c_zero, u_useColorOnly);
    palettedColor.a = c_one-floor(color.r);
    color = mix(color, palettedColor, u_usePalette);

#ifdef POLYMOST1_EXTENDED
    vec4 detailColor = texture(s_detail, v_texCoord3.xy);
    detailColor = mix(c_vec4_one, 2.0*detailColor, u_useDetailMapping*detailColor.a);
    color.rgb *= detailColor.rgb;
#endif

    color = mix(color, c_vec4_one, u_useColorOnly);

    // DEBUG
    //color = texture(s_palswap, v_texCoord0.xy);
    //color = texture(s_palette, v_texCoord0.xy);
    //color = texture(s_texture, v_texCoord0.yx);

    color.rgb = mix(v_color.rgb*color.rgb, color.rgb, fullbright);

    fullbright = clamp(-(u_fogEnabled-c_one), fullbright, c_one);
    float fogFactor = clamp((v_fog.x-v_fogCoord)*v_fog.y, fullbright, c_one);
    //float fogFactor = clamp(v_fogCoord, fullbright, c_one);
    color.rgb = mix(u_fogColor.rgb, color.rgb, fogFactor);

#ifdef POLYMOST1_EXTENDED
    vec4 glowColor = texture(s_glow, v_texCoord4.xy);
    color.rgb = mix(color.rgb, glowColor.rgb, u_useGlowMapping * glowColor.a * -(u_useColorOnly-c_one));
#endif

    color.a *= v_color.a;

    vec4 v_cc = vec4(u_colorCorrection.x - c_one, 0.5 * -(u_colorCorrection.y-c_one), -(u_colorCorrection.z-c_one), 1.0);
    vec4 outColor = mat4(c_vec2_zero_one.yxxx, c_vec2_zero_one.xyxx, c_vec2_zero_one.xxyx, v_cc.xxxw)
                   * mat4(u_colorCorrection.ywww, u_colorCorrection.wyww, u_colorCorrection.wwyw, v_cc.yyyw)
                   * mat4((c_vec4_luma_709.xxxw * v_cc.z) + u_colorCorrection.zwww,
                          (c_vec4_luma_709.yyyw * v_cc.z) + u_colorCorrection.wzww,
                          (c_vec4_luma_709.zzzw * v_cc.z) + u_colorCorrection.wwzw,
                          c_vec2_zero_one.xxxy)
                   * color;
    if (outColor.a <= u_alphaRef)
        discard;
    o_color = outColor;
}
