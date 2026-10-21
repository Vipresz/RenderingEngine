#version 430 core

layout(location = 0) in vec3 a_position;

out vec3 v_world;
out vec2 v_rest_xz;
out float v_open;
out float v_amplitude;

uniform mat4 u_pv;
uniform vec4 u_map_scales[2];
uniform float u_wind_radians[2];
uniform vec2 u_dist_origin;
uniform vec2 u_dist_span;
uniform float u_shoal_depth;
uniform sampler2DArray u_displacements;
uniform sampler2D u_land_distance;

float coastal_amount(vec4 field, out float open) {
	float depth = field.a;
	float wet = smoothstep(0.05, 1.0, depth);
	float beach = smoothstep(0.0, 40.0, field.r);
	float shoal = clamp(pow(u_shoal_depth / max(depth, 0.5), 0.25), 1.0, 1.6);
	float unbroken = smoothstep(0.25, 2.2, depth);
	open = smoothstep(40.0, 240.0, field.r);
	return wet * mix(0.2, 1.0, beach) * unbroken * shoal;
}

void shore_axes(vec2 xz, vec4 field, out vec2 local, out vec2 landward) {
	landward = field.gb;
	if (length(landward) < 0.2) {
		landward = vec2(0.0, 1.0);
	} else {
		landward = normalize(landward);
	}
	local = vec2(-field.r, xz.x);
}

void wind_align(float wind_angle, out float cosine, out float sine) {
	vec2 propagation = vec2(cos(wind_angle), sin(wind_angle));
	cosine = propagation.x;
	sine = -propagation.y;
}

vec3 sample_wave(vec2 local, vec4 scales, float layer, float cosine, float sine) {
	vec2 spun = vec2(cosine * local.x + sine * local.y, -sine * local.x + cosine * local.y);
	vec3 wave = texture(u_displacements, vec3(spun * scales.xy, layer)).xyz * scales.z;
	return vec3(cosine * wave.x - sine * wave.z, wave.y, sine * wave.x + cosine * wave.z);
}

vec3 to_world(vec3 local_wave, vec2 landward) {
	return vec3(landward.x * local_wave.x + local_wave.z, local_wave.y, landward.y * local_wave.x);
}

void main() {
	vec4 field = texture(u_land_distance, (a_position.xz - u_dist_origin) / u_dist_span);
	float open = 0.0;
	float amplitude = coastal_amount(field, open);
	vec2 local = vec2(0.0);
	vec2 landward = vec2(0.0, 1.0);
	shore_axes(a_position.xz, field, local, landward);
	float cosine0 = 1.0;
	float sine0 = 0.0;
	wind_align(u_wind_radians[0], cosine0, sine0);
	vec3 large_wave = to_world(sample_wave(local, u_map_scales[0], 0.0, cosine0, sine0), landward);

	vec3 position = a_position + large_wave * amplitude;
	v_world = position;
	v_rest_xz = a_position.xz;
	v_open = open;
	v_amplitude = amplitude;
	gl_Position = u_pv * vec4(position, 1.0);
}
