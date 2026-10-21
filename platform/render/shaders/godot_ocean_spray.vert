#version 430 core

layout(location = 0) in vec3 a_position;

out float v_alpha;

uniform mat4 u_pv;
uniform vec4 u_map_scales[2];
uniform float u_wind_radians[2];
uniform vec2 u_dist_origin;
uniform vec2 u_dist_span;
uniform float u_shoal_depth;
uniform float u_point_scale;
uniform sampler2DArray u_displacements;
uniform sampler2DArray u_normals;
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

float sample_foam(vec2 local, vec4 scales, float layer, float cosine, float sine) {
	vec2 spun = vec2(cosine * local.x + sine * local.y, -sine * local.x + cosine * local.y);
	return texture(u_normals, vec3(spun * scales.xy, layer)).a;
}

void main() {
	vec2 xz = a_position.xz;
	vec4 field = texture(u_land_distance, (xz - u_dist_origin) / u_dist_span);
	float open = 0.0;
	float amplitude = coastal_amount(field, open);
	vec2 landward = field.gb;
	if (length(landward) < 0.2) {
		landward = vec2(0.0, 1.0);
	} else {
		landward = normalize(landward);
	}
	vec2 local = vec2(-field.r, xz.x);
	float cosine0 = 1.0;
	float sine0 = 0.0;
	float cosine1 = 1.0;
	float sine1 = 0.0;
	wind_align(u_wind_radians[0], cosine0, sine0);
	wind_align(u_wind_radians[1], cosine1, sine1);
	vec3 large_local = sample_wave(local, u_map_scales[0], 0.0, cosine0, sine0);
	vec3 displacement = vec3(landward.x * large_local.x + large_local.z, large_local.y,
		landward.y * large_local.x) * amplitude;
	float foam = mix(sample_foam(local, u_map_scales[1], 1.0, cosine1, sine1),
		sample_foam(local, u_map_scales[0], 0.0, cosine0, sine0), open);
	float alive = smoothstep(0.55, 0.92, foam) * amplitude;

	vec3 world = a_position + displacement;
	world.y += 0.15;
	vec4 clip = u_pv * vec4(world, 1.0);
	gl_PointSize = 0.0;
	v_alpha = 0.0;
	if (alive >= 0.04) {
		float world_size = mix(0.22, 4.4, open) * alive;
		gl_PointSize = clamp(world_size * u_point_scale / max(clip.w, 0.05), 0.0, 96.0);
		clip.z -= 0.0008 * clip.w;
		gl_Position = clip;
		v_alpha = alive * mix(0.55, 1.0, open);
	} else {
		gl_Position = vec4(0.0, 0.0, -2.0, 1.0);
	}
}
