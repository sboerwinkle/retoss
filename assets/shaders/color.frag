#version 430 core
layout(binding=0) uniform sampler2D u_tex;
layout(binding=1) uniform sampler2D u_mottle_tex;
uniform vec4 u_tint;
uniform float u_transparency;
uniform vec3 u_fog;
uniform float u_fog_mult;

in vec3 v_color;
in vec2 v_uv;
in vec2 v_mottle_1;
in vec2 v_mottle_2;
in vec3 v_pos;

layout(location = 0) out vec4 out_color;

void main() {
	vec4 texColor = texture(u_tex, v_uv);
	if (texColor.a == 0) {
		// Think I need this to prevent drawing to the Z-buffer
		// Probably only used for billboards, seems silly
		discard;
	}


	float brightness = 1.0 - 0.2 * texture(u_mottle_tex, v_mottle_1).r * texture(u_mottle_tex, v_mottle_2).r;

	vec3 color = brightness*(texColor.rgb*u_tint.a + u_tint.rgb) + v_color;
	float fogFactor = max(0, exp(length(v_pos)*u_fog_mult) * (1 + 1.0/16) - 1.0/16);
	color = mix(u_fog, color, fogFactor);

	out_color = vec4(color, u_transparency * texColor.a);
}
