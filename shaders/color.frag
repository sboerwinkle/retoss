#version 430 core
layout(location=1) uniform sampler2D u_tex;

layout(location=0) in vec3 v_color;
layout(location=1) in vec2 v_uv;

layout(location = 0) out vec4 out_color;

void main()
{
	// Could maybe have some way to tint the sprites? IDK
	// float tex = texture(u_tex, v_uv).r;
	// out_color = vec4(tex*v_color, 1.0);
	out_color = texture(u_tex, v_uv);
}
