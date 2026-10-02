#version 430
uniform mat4 u_modelview;

// Ultimately I did figure out how to get rid of u_rot, if I want.
// Basically the idea is to use u_modelview for the same purpose.
// It just requires adding one vec3 for scaling (now out of u_modelview),
// and one vec3 to give the light direction in camera space (once per frame).
// This saves 6 floats of transmission per cube, but costs more math here.
uniform mat3 u_rot;

uniform vec3 u_skin_size;
uniform vec2 u_tex_offset;
uniform float u_mottle_scale;

// Having exactly 32 floats of uniforms in this shader speeds things up
// for my (Intel) GPU, and for now I'm unwilling to give that up!
// If you don't want that, just comment out the usage of this uniform
// (but not the definition) and it should get optimized away.
uniform float u_one;

in vec3 a_pos;
in vec3 a_norm;
in mat3x2 a_tex;

out vec3 v_color;
out vec2 v_uv;
out vec2 v_mottle_1;
out vec2 v_mottle_2;
out vec3 v_pos;

void main()
{
	gl_Position = u_modelview * vec4(a_pos, 1.0);
	v_pos = gl_Position.xyw;

	vec2 texBase = a_tex*u_skin_size;
	v_uv = u_tex_offset + texBase*u_one;
	v_mottle_1 = texBase*u_mottle_scale;
	v_mottle_2 = 1.618034*v_mottle_1;

	// I want my faces to look a bit different based on angle.
	// Normally I'd put them in shadow, but I already have this code here,
	// and it's literally 00:02AM right now.
	float lighting_dot = dot(vec3(0.44, 0, 0.9), u_rot*a_norm);
	vec3 glare = lighting_dot * vec3(0.15,0.15,0.15);

	// v_color = glare + (0.75 + 0.25*lighting_dot)*u_tint;
	v_color = glare;
}
