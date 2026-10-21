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
uniform sampler2D u_scene;
uniform sampler2D u_scene_depth;
uniform mat4 u_pv;
uniform mat4 u_inv_pv;
uniform vec4 u_viewport;
uniform float u_sea_level;
uniform int u_has_scene;

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
	foam = clamp(foam, 0.0, 1.0);

	vec3 normal = normalize(vec3(-gradient.x, 1.0, -gradient.y));
	vec3 view_dir = normalize(u_camera_pos - v_world);
	float ndotv = clamp(dot(normal, view_dir), 0.0, 1.0);
	float fresnel = 0.02 + 0.98 * pow(1.0 - ndotv, 5.0);

	vec3 sky = vec3(0.55, 0.72, 0.82);
	vec3 foam_color = vec3(0.86, 0.90, 0.93);
	vec3 sun = normalize(-u_sun_direction);
	vec3 halfway = normalize(sun + view_dir);
	float spec = pow(clamp(dot(normal, halfway), 0.0, 1.0), 96.0);

	vec3 transmitted = vec3(0.02, 0.07, 0.10);
	vec3 reflected = sky;
	if (u_has_scene != 0) {
		vec2 uv = (gl_FragCoord.xy - u_viewport.xy) / max(u_viewport.zw, vec2(1.0));
		vec2 refractUv = clamp(uv + normal.xz * 0.018 * (1.0 - fresnel), vec2(0.001), vec2(0.999));
		transmitted = texture(u_scene, refractUv).rgb;
		transmitted = mix(transmitted, transmitted * vec3(0.35, 0.85, 0.92), 0.35);

		vec3 reflDir = reflect(-view_dir, normal);
		if (reflDir.y > 0.05) {
			float travel = 6.0;
			for (int i = 0; i < 10; ++i) {
				vec3 samplePos = v_world + reflDir * travel;
				vec4 clip = u_pv * vec4(samplePos, 1.0);
				if (clip.w <= 0.05) {
					break;
				}
				vec2 reflectUv = clip.xy / clip.w * 0.5 + 0.5;
				if (reflectUv.x < 0.0 || reflectUv.y < 0.0 || reflectUv.x > 1.0 || reflectUv.y > 1.0) {
					break;
				}
				float sceneDepth = texture(u_scene_depth, reflectUv).r;
				vec4 sceneH = u_inv_pv * vec4(reflectUv * 2.0 - 1.0, sceneDepth * 2.0 - 1.0, 1.0);
				vec3 sceneWorld = sceneH.xyz / max(sceneH.w, 0.0001);
				if (sceneWorld.y > u_sea_level + 0.2 && abs(sceneWorld.y - samplePos.y) < 2.5) {
					reflected = texture(u_scene, reflectUv).rgb;
					break;
				}
				travel += 4.0 + float(i);
			}
		}
	}

	vec3 color = mix(transmitted, reflected, fresnel);
	color += vec3(1.0) * spec * fresnel * 0.85;
	color = mix(color, foam_color, clamp(foam, 0.0, 1.0));
	out_color = vec4(color, 1.0);
}
