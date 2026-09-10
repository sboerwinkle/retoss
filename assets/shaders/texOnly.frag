#version 430 core
layout(location=1) uniform sampler2D u_tex;
uniform vec4 u_c_mult;
uniform vec4 u_c_add;

layout(location=0) in vec2 v_uv;

layout(location = 0) out vec4 out_color;

void main()
{
	out_color = texture(u_tex, v_uv) * u_c_mult + u_c_add;
}
