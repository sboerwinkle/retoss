#version 430
layout(location=0) uniform mat4 u_modelview;

// skip location=1 b/c it's used by the texture in the frag shader
// TODO do I need to have that explicitly?
layout(location=2) uniform float u_texscale;
layout(location=3) uniform vec2 u_texoffset;
layout(location=4) uniform mat3 u_rot;
// layout(location=5) uniform vec3 u_tint;

// I don't think I *need* explicit locations on these, since graphics.c asks GL what location they got anyway.
// However, it is nice to have them explicit, as I do re-use attribute locations between programs that share
// this vertex shader.
layout(location=0) in vec3 a_pos;
layout(location=1) in vec3 a_norm;
layout(location=2) in vec2 a_tex_st;

layout(location=0) out vec3 v_color;
layout(location=1) out vec2 v_uv;
layout(location=2) out vec2 v_mottle_1;
layout(location=3) out vec2 v_mottle_2;

void main()
{
	gl_Position = u_modelview * vec4(a_pos, 1.0);
	v_uv = a_tex_st;
	v_mottle_1 = u_texoffset + u_texscale*a_tex_st;
	v_mottle_2 = u_texoffset + 1.618034*u_texscale*a_tex_st;

	// I want my faces to look a bit different based on angle.
	// Normally I'd put them in shadow, but I already have this code here,
	// and it's literally 00:02AM right now.
	float lighting_dot = dot(vec3(0.6, 0, 0.8), u_rot*a_norm);
	vec3 glare = lighting_dot * vec3(0.15,0.15,0.15);

	// v_color = glare + (0.75 + 0.25*lighting_dot)*u_tint;
	v_color = glare;
}
