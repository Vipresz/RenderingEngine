#version 430 core

in vec3 v_world;
in vec2 v_rest_xz;
in float v_open;
in float v_amplitude;

layout(location = 0) out vec4 out_color;

uniform vec3 u_camera_pos;
uniform vec3 u_sun_direction;
uniform vec4 u_map_scales[2];
uniform float u_wind_radians[2];
uniform vec2 u_dist_origin;
uniform vec2 u_dist_span;
uniform sampler2DArray u_normals;
uniform sampler2D u_land_distance;

void wind_align(float wind_angle, out float cosine, out float sine) {
	vec2 propagation = vec2(cos(wind_angle), sin(wind_angle));
	cosine = propagation.x;
	sine = -propagation.y;
}

vec3 sample_slope(vec2 local, vec4 scales, float layer, float cosine, float sine) {
	vec2 spun = vec2(cosine * local.x + sine * local.y, -sine * local.x + cosine * local.y);
	vec3 sample_value = texture(u_normals, vec3(spun * scales.xy, layer)).xyw;
	vec2 gradient = vec2(cosine * sample_value.x - sine * sample_value.y,
		sine * sample_value.x + cosine * sample_value.y);
	return vec3(gradient, sample_value.z);
}

void main() {
	vec4 field = texture(u_land_distance, (v_rest_xz - u_dist_origin) / u_dist_span);
	vec2 landward = field.gb;
	if (length(landward) < 0.2) {
		landward = vec2(0.0, 1.0);
	} else {
		landward = normalize(landward);
	}
	vec2 local = vec2(-field.r, v_rest_xz.x);
	float cosine0 = 1.0;
	float sine0 = 0.0;
	float cosine1 = 1.0;
	float sine1 = 0.0;
	wind_align(u_wind_radians[0], cosine0, sine0);
	wind_align(u_wind_radians[1], cosine1, sine1);
	vec3 large_sample = sample_slope(local, u_map_scales[0], 0.0, cosine0, sine0);
	vec3 small_sample = sample_slope(local, u_map_scales[1], 1.0, cosine1, sine1);
	vec2 local_gradient = small_sample.xy * u_map_scales[1].w * 1.15
		+ large_sample.xy * u_map_scales[0].w * 0.45;
	vec2 gradient = landward * local_gradient.x + vec2(local_gradient.y, 0.0);
	gradient *= v_amplitude;
	float foam = mix(small_sample.z, large_sample.z, v_open);
	foam = smoothstep(0.08, 0.9, foam) * mix(0.4, 1.2, v_open) * v_amplitude;
	float surf = smoothstep(0.12, 0.8, field.a) * (1.0 - smoothstep(0.5, 3.5, field.a));
	foam = clamp(foam + surf * (1.0 - v_open) * 0.8, 0.0, 1.0);

	vec3 normal = normalize(vec3(-gradient.x, 1.0, -gradient.y));
	vec3 view_dir = normalize(u_camera_pos - v_world);
	float ndotv = clamp(dot(normal, view_dir), 0.0, 1.0);
	float fresnel = 0.02 + 0.98 * pow(1.0 - ndotv, 5.0);

	vec3 water = vec3(0.02, 0.07, 0.10);
	vec3 sky = vec3(0.55, 0.72, 0.82);
	vec3 foam_color = vec3(0.86, 0.90, 0.93);
	vec3 sun = normalize(-u_sun_direction);
	float diffuse = clamp(dot(normal, sun), 0.0, 1.0);
	vec3 halfway = normalize(sun + view_dir);
	float spec = pow(clamp(dot(normal, halfway), 0.0, 1.0), 96.0);

	vec3 color = mix(water, sky, fresnel);
	color *= 0.35 + 0.9 * diffuse;
	color += vec3(1.0) * spec * fresnel * 0.85;
	color = mix(color, foam_color, clamp(foam, 0.0, 1.0));
	out_color = vec4(color, 1.0);
}
