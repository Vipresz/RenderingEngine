#version 430 core

in float v_alpha;

layout(location = 0) out vec4 out_color;

void main() {
	vec2 point = gl_PointCoord * 2.0 - 1.0;
	float radius = dot(point, point);
	if (radius > 1.0) {
		discard;
	}
	float soft = smoothstep(1.0, 0.15, radius);
	out_color = vec4(0.94, 0.97, 0.99, soft * v_alpha);
}
