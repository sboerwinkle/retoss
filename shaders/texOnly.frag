#version 430 core
layout(location=1) uniform sampler2D u_tex;

layout(location=0) in vec2 v_uv;

layout(location = 0) out vec4 out_color;

void main()
{
	// Could maybe have some way to tint the sprites? IDK
	out_color = texture(u_tex, v_uv);
}
