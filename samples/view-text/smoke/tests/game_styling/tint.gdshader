shader_type canvas_item;
render_mode blend_premul_alpha;

uniform float strength = 0.0;
uniform vec4 tint = vec4(1.0);

void fragment() {
	COLOR = texture(TEXTURE, UV) * mix(vec4(1.0), tint, strength);
}
