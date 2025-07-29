#version 430

// layout(location=0) uniform mat3 u_camera;
uniform vec2 u_offset;
uniform vec2 u_tex_offset;
uniform vec2 u_size;
uniform vec2 u_scale;
uniform vec2 u_tex_scale;

layout(location=0) in vec2 a_loc;

layout(location=0) out vec3 v_color;
layout(location=1) out vec2 v_uv;

void main()
{
	vec2 v = u_scale * (u_offset + u_size*a_loc);
	gl_Position = vec4(v.xy, 0, 1.0);

	v_color = vec3(0, 0, 0);
	v_uv = u_tex_scale * (u_tex_offset + u_size*a_loc);
}
