#version 430
layout(location=0) uniform mat4 u_modelview;

// skip location=1 b/c it's used by the texture in the frag shader
// TODO do I need to have that explicitly?
layout(location=2) uniform vec3 u_object_scale;
layout(location=3) uniform vec2 u_tex_offset;
layout(location=4) uniform vec2 u_tex_scale;
layout(location=5) uniform mat3 u_rot;
// uniform vec3 u_tint;

in vec3 a_pos;
in vec3 a_norm;
in mat3x2 a_tex;

layout(location=0) out vec3 v_color;
layout(location=1) out vec2 v_uv;
layout(location=2) out vec2 v_mottle_1;
layout(location=3) out vec2 v_mottle_2;
out vec3 v_pos;

void main()
{
	gl_Position = u_modelview * vec4(a_pos, 1.0);
	v_pos = gl_Position.xyw;
	v_uv = u_tex_offset + u_tex_scale*(a_tex*u_object_scale);
	// Previously the noise (mottle) could be scaled independently,
	// may add that back later.
	v_mottle_1 = v_uv;
	v_mottle_2 = 1.618034*v_uv;

	// I want my faces to look a bit different based on angle.
	// Normally I'd put them in shadow, but I already have this code here,
	// and it's literally 00:02AM right now.
	float lighting_dot = dot(vec3(0.44, 0, 0.9), u_rot*a_norm);
	vec3 glare = lighting_dot * vec3(0.15,0.15,0.15);

	// v_color = glare + (0.75 + 0.25*lighting_dot)*u_tint;
	v_color = glare;
}
