#version 300 es
// polymost1Vert.glsl in GLSL ES 3.00, for WebGL 2: the fixed-function inputs are the
// attributes and uniforms glbuild_ein.cpp feeds; a_fog is the fog's end and 1 / (end - start).

in vec4 a_vertex;
in vec4 a_texcoord0;
in vec4 a_texcoord3;
in vec4 a_texcoord4;
in vec4 a_color;
in vec2 a_fog;

uniform mat4 u_modelView;
uniform mat4 u_projection;
uniform mat4 u_textureMatrix0;
uniform mat4 u_textureMatrix3;
uniform mat4 u_textureMatrix4;

out vec4 v_color;
out float v_distance;
out vec4 v_texCoord0;
out vec4 v_texCoord3;
out vec4 v_texCoord4;
out float v_fogCoord;
flat out vec2 v_fog;

//u_texturePosSize is the texture position & size packaged into a single vec4 as {pos.x, pos.y, size.x, size.y}
uniform vec4 u_texturePosSize;
uniform float u_usePalette;
uniform mat4 u_rotMatrix;

void main()
{
   vec4 vertex = u_rotMatrix * a_vertex;
   vec4 eyeCoordPosition = u_modelView * vertex;
   gl_Position = (u_projection * u_modelView) * vertex;

   eyeCoordPosition.xyz /= eyeCoordPosition.w;

   v_texCoord0 = u_textureMatrix0 * a_texcoord0;
   v_texCoord0 = mix(v_texCoord0.xyzw, v_texCoord0.yxzw, u_usePalette);

   v_texCoord3 = u_textureMatrix3 * a_texcoord3;
   v_texCoord4 = u_textureMatrix4 * a_texcoord4;

   v_fogCoord = abs(eyeCoordPosition.z);
   v_fog = a_fog;

   v_color = a_color;
   v_distance = a_vertex.z;
}
