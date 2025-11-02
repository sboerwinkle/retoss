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

	float lighting_dot = dot(vec3(0.6, 0, 0.8), u_rot*a_norm);

	// I'm sure there's a more correct/technical term for this.
	// Point is, if it's facing the light directly we add a little of the light's color.
	// Uh... this will still be subjected to the texture stuff though, not fixing that rn.
	vec3 glare = max(lighting_dot - 0.6, 0) * vec3(1,1,1);

	// v_color = glare + (0.75 + 0.25*lighting_dot)*u_tint;
	v_color = glare;
}
