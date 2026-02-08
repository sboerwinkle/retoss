#version 430 core
layout(location=1, binding=0) uniform sampler2D u_tex;
layout(binding=1) uniform sampler2D u_mottle_tex;

layout(location=0) in vec3 v_color;
layout(location=1) in vec2 v_uv;
layout(location=2) in vec2 v_mottle_1;
layout(location=3) in vec2 v_mottle_2;

layout(location = 0) out vec4 out_color;

void main() {
	if (
		(mod(gl_FragCoord.x, 2) >= 1) !=
		(mod(gl_FragCoord.y, 2) >= 1)
	) {
		discard;
	}
	float brightness = 1.0 - 0.2 * texture(u_mottle_tex, v_mottle_1).r * texture(u_mottle_tex, v_mottle_2).r;
	vec4 mult = vec4(brightness, brightness, brightness, 1.0);
	out_color = texture(u_tex, v_uv)*mult + vec4(v_color, 0.0);
}
