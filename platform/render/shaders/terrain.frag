#version 330 core

in vec4 vCol;
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 DirectionalLightSpacePos;

out vec4 colour;

struct Light
{
	vec3 colour;
	float ambientIntensity;
	float diffuseIntensity;
};

struct DirectionalLight
{
	Light base;
	vec3 direction;
};

uniform vec3 eyePosition;
uniform sampler2D directionalShadowMap;
uniform DirectionalLight directionalLight;

uniform sampler2D albedoMap[4];
uniform sampler2D normalMap[4];
uniform sampler2D ormMap[4];

uniform vec3 tint;
uniform float roughnessBias;
uniform float uvScale;
uniform float terrainHeight;
uniform float slopeRock;
uniform float heightCover;
uniform float coverAmount;
uniform float accentAmount;
uniform float moss;
uniform float snowAmount;
uniform float ash;
uniform float dust;
uniform float wet;
uniform float cracks;
uniform float exoticAmount;
uniform float exoticPulse;
uniform vec3 exoticColor;
uniform float exoticTime;
uniform float paintAmount;
uniform sampler2D materialMask;
uniform sampler2D flowMask;
uniform float useMasks;

float Hash(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float ValueNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = Hash(i);
	float b = Hash(i + vec2(1.0, 0.0));
	float c = Hash(i + vec2(0.0, 1.0));
	float d = Hash(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float ShadowFactor(DirectionalLight light)
{
	vec3 projCoords = DirectionalLightSpacePos.xyz / DirectionalLightSpacePos.w;
	projCoords = projCoords * 0.5 + 0.5;
	float current = projCoords.z;
	vec3 normal = normalize(Normal);
	vec3 lightDir = normalize(-light.direction);
	float ndotl = clamp(dot(normal, lightDir), 0.0, 1.0);
	float bias = clamp(0.003 * (1.0 - ndotl), 0.0008, 0.008);
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(directionalShadowMap, 0);
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(directionalShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += current - bias > pcfDepth ? 1.0 : 0.0;
		}
	}
	shadow /= 9.0;
	if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
	{
		shadow = 0.0;
	}
	return shadow;
}

mat3 CotangentFrame(vec3 n, vec3 p, vec2 uv)
{
	vec3 dp1 = dFdx(p);
	vec3 dp2 = dFdy(p);
	vec2 duv1 = dFdx(uv);
	vec2 duv2 = dFdy(uv);
	vec3 dp2perp = cross(dp2, n);
	vec3 dp1perp = cross(n, dp1);
	vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;
	float invmax = inversesqrt(max(max(dot(t, t), dot(b, b)), 1e-8));
	return mat3(t * invmax, b * invmax, n);
}

void main()
{
	vec3 geometricNormal = normalize(Normal);
	vec2 uv = FragPos.xz * uvScale;
	float slope = 1.0 - clamp(geometricNormal.y, 0.0, 1.0);
	float height01 = clamp(FragPos.y / max(terrainHeight, 0.001), 0.0, 1.0);
	float breakup = ValueNoise(FragPos.xz * 0.17);

	float rockW = smoothstep(slopeRock, slopeRock + 0.22, slope);
	float coverH = smoothstep(heightCover, heightCover + 0.18, height01);
	float coverW = (1.0 - rockW) * coverAmount * mix(0.35, 1.0, coverH);
	float accentW = (1.0 - rockW) * accentAmount * smoothstep(0.42, 0.78, breakup);
	float groundW = (1.0 - rockW) * (1.0 - clamp(coverW, 0.0, 0.85));
	float weightSum = max(groundW + coverW + rockW + accentW, 0.001);
	groundW /= weightSum;
	coverW /= weightSum;
	rockW /= weightSum;
	accentW /= weightSum;

	vec4 paintedMask = texture(materialMask, TexCoord);
	float maskBlend = clamp(useMasks, 0.0, 1.0);
	groundW = mix(groundW, paintedMask.g, maskBlend);
	coverW = mix(coverW, paintedMask.b, maskBlend);
	rockW = mix(rockW, paintedMask.r, maskBlend);
	accentW = mix(accentW, paintedMask.a, maskBlend);
	weightSum = max(groundW + coverW + rockW + accentW, 0.001);
	groundW /= weightSum;
	coverW /= weightSum;
	rockW /= weightSum;
	accentW /= weightSum;
	float wetness = clamp(wet + texture(flowMask, TexCoord).r * maskBlend, 0.0, 1.0);

	vec3 albedo =
		texture(albedoMap[0], uv).rgb * groundW +
		texture(albedoMap[1], uv).rgb * coverW +
		texture(albedoMap[2], uv).rgb * rockW +
		texture(albedoMap[3], uv).rgb * accentW;
	vec3 tangentNormal =
		texture(normalMap[0], uv).rgb * groundW +
		texture(normalMap[1], uv).rgb * coverW +
		texture(normalMap[2], uv).rgb * rockW +
		texture(normalMap[3], uv).rgb * accentW;
	vec3 orm =
		texture(ormMap[0], uv).rgb * groundW +
		texture(ormMap[1], uv).rgb * coverW +
		texture(ormMap[2], uv).rgb * rockW +
		texture(ormMap[3], uv).rgb * accentW;

	albedo *= tint;
	if (paintAmount > 0.001 && length(vCol.rgb) > 0.01)
	{
		albedo = mix(albedo, albedo * vCol.rgb, paintAmount);
	}

	float up = 1.0 - slope;
	float mossNoise = smoothstep(0.45, 0.75, ValueNoise(uv * 3.0));
	albedo = mix(albedo, albedo * vec3(0.35, 0.55, 0.22), moss * mossNoise * up);
	float snowNoise = smoothstep(0.35, 0.7, ValueNoise(uv * 1.7));
	albedo = mix(albedo, vec3(0.92, 0.94, 0.96), snowAmount * snowNoise * up * coverH);
	albedo = mix(albedo, albedo * vec3(0.55, 0.54, 0.52), ash * ValueNoise(uv * 5.0));
	albedo = mix(albedo, albedo * vec3(0.72, 0.42, 0.28), dust * up * ValueNoise(uv * 2.2));

	float crackNoise = smoothstep(0.72, 0.9, 1.0 - abs(ValueNoise(uv * 8.0) * 2.0 - 1.0));
	albedo *= 1.0 - cracks * crackNoise * 0.55;
	albedo *= 1.0 - wetness * 0.28;

	float occlusion = orm.r;
	float roughness = clamp(orm.g + roughnessBias - wetness * 0.35, 0.04, 1.0);
	float metallic = clamp(orm.b, 0.0, 1.0);

	float exoNoise = ValueNoise(uv * 2.4 + exoticTime * 0.05);
	float veins = smoothstep(0.68, 0.9, exoNoise);
	float pulse = 0.65 + 0.35 * sin(exoticTime * exoticPulse + exoNoise * 6.28318);
	float exo = exoticAmount * pulse;
	vec3 emission = exoticColor * veins * exo * 1.6;
	albedo = mix(albedo, albedo * exoticColor + exoticColor * veins, exo * (0.35 + veins));
	roughness = mix(roughness, 0.22, exo * veins);
	metallic = mix(metallic, 0.65, exo * veins);
	tangentNormal = normalize(tangentNormal * 2.0 - 1.0 + vec3(veins * exo * 0.35, 0.0, veins * exo * 0.2));

	vec3 mappedNormal = normalize(CotangentFrame(geometricNormal, FragPos, uv) * normalize(tangentNormal));
	vec3 lightDir = normalize(-directionalLight.direction);
	vec3 viewDir = normalize(eyePosition - FragPos);
	float shadow = ShadowFactor(directionalLight);
	float specPower = mix(160.0, 6.0, roughness);
	float spec = pow(max(dot(reflect(-lightDir, mappedNormal), viewDir), 0.0), specPower);
	spec *= (1.0 - roughness) * mix(0.04, 1.0, metallic);

	vec3 lightColour = directionalLight.base.colour;
	float sun = clamp(dot(mappedNormal, lightDir) * 0.65 + 0.35, 0.0, 1.0);
	float sky = clamp(mappedNormal.y * 0.5 + 0.5, 0.0, 1.0);
	vec3 fill = lightColour * directionalLight.base.ambientIntensity * mix(0.75, 1.25, sky) * occlusion;
	vec3 diffuse = lightColour * directionalLight.base.diffuseIntensity * sun * 2.6 * (1.0 - shadow);
	vec3 lit = albedo * (1.0 - metallic) * (fill + diffuse);
	lit += lightColour * spec * directionalLight.base.diffuseIntensity * 2.6 * (1.0 - shadow);
	lit += emission;
	lit = lit / (vec3(1.0) + lit);
	colour = vec4(lit, 1.0);
}
