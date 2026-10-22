#version 330 core

in vec4 vCol;
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 DirectionalLightSpacePos;

out vec4 colour;

const int MAX_POINT_LIGHTS = 3;
const int MAX_SPOT_LIGHTS = 3;

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

struct PointLight
{
	Light base;
	vec3 position;
	float constant;
	float linear;
	float exponent;
};

struct SpotLight
{
	PointLight base;
	vec3 direction;
	float edge;
};

struct OmniShadowMap
{
	samplerCube shadowMap;
	float farPlane;
};

struct Material
{
	float specularIntensity;
	float shininess;
};

uniform int pointLightCount;
uniform int spotLightCount;

uniform DirectionalLight directionalLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];

uniform sampler2D theTexture;
uniform int useTexture;

uniform sampler2D directionalShadowMap;
uniform OmniShadowMap omniShadowMaps[MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS];

uniform Material material;
uniform float materialOpacity;

uniform vec3 eyePosition;

vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
); 

float CalcDirectionalShadowFactor(DirectionalLight light)
{
	vec3 projCoords = DirectionalLightSpacePos.xyz / DirectionalLightSpacePos.w;
	projCoords = (projCoords * 0.5) + 0.5;
	
	float current = projCoords.z;
	
	vec3 normal = normalize(Normal);
	vec3 lightDir = normalize(-directionalLight.direction);
	float ndotl = clamp(dot(normal, lightDir), 0.0, 1.0);
	float bias = clamp(0.003 * (1.0 - ndotl), 0.0008, 0.008);

	
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(directionalShadowMap, 0);
	for(int x = -1; x <= 1; ++x)
	{
		for(int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(directionalShadowMap, projCoords.xy + vec2(x,y) * texelSize).r;
			shadow += current - bias > pcfDepth ? 1.0 : 0.0;
		}
	}

	shadow /= 9.0;
	
	if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
	{
		shadow = 0.0;
	}

	return shadow;
}

float CalcOmniShadowFactor(PointLight light, int shadowIndex)
{
	vec3 fragToLight = FragPos - light.position;
	float currentDepth = length(fragToLight);
	
	float shadow = 0.0;
	float bias = 0.05;
	int samples = 20;
	
	float viewDistance = length(eyePosition - FragPos);
	float diskRadius = (1.0 + (viewDistance/omniShadowMaps[shadowIndex].farPlane)) / 25.0;
	
	for(int i = 0; i < samples; i++)
	{
		float closestDepth = texture(omniShadowMaps[shadowIndex].shadowMap, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
		closestDepth *= omniShadowMaps[shadowIndex].farPlane;
		if(currentDepth -  bias > closestDepth)
		{
			shadow += 1.0;
		}
	}
	
	
	shadow /= float(samples);
	return shadow;
}

vec4 CalcLightByDirection(Light light, vec3 direction, float shadowFactor)
{
	vec4 ambientColour = vec4(light.colour, 1.0f) * light.ambientIntensity;
	
	float diffuseFactor = max(dot(normalize(Normal), normalize(-direction)), 0.0f);
	vec4 diffuseColour = vec4(light.colour * light.diffuseIntensity * diffuseFactor, 1.0f);
	
	vec4 specularColour = vec4(0, 0, 0, 0);
	
	if(diffuseFactor > 0.0f)
	{
		vec3 fragToEye = normalize(eyePosition - FragPos);
		vec3 reflectedVertex = normalize(reflect(direction, normalize(Normal)));
		
		float specularFactor = dot(fragToEye, reflectedVertex);
		if(specularFactor > 0.0f)
		{
			specularFactor = pow(specularFactor, material.shininess);
			specularColour = vec4(light.colour * material.specularIntensity * specularFactor, 1.0f);
		}
	}

	return (ambientColour + (1.0 - shadowFactor) * (diffuseColour + specularColour));
}

vec4 CalcDirectionalLight()
{
	vec3 normal = Normal;
	if (length(normal) > 0.001) {
		normal = normalize(normal);
	} else {
		normal = vec3(0.0, 1.0, 0.0);
	}

	vec3 toSun = normalize(-directionalLight.direction);
	float ndotl = max(dot(normal, toSun), 0.0);
	float shadowFactor = CalcDirectionalShadowFactor(directionalLight);
	float lit = ndotl * (1.0 - shadowFactor);
	vec3 sunColour = directionalLight.base.colour;
	vec3 key = sunColour * directionalLight.base.diffuseIntensity * lit * 2.4;

	float skyUp = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
	vec3 skyColour = mix(vec3(0.42, 0.58, 0.82), sunColour, 0.28);
	vec3 scatter = skyColour * directionalLight.base.ambientIntensity * (1.7 + 2.1 * skyUp);
	scatter += sunColour * directionalLight.base.diffuseIntensity * pow(max(lit, 0.0), 0.55) * 0.9;

	vec3 specular = vec3(0.0);
	if (ndotl > 0.0) {
		vec3 fragToEye = normalize(eyePosition - FragPos);
		vec3 reflected = normalize(reflect(-toSun, normal));
		float specularFactor = max(dot(fragToEye, reflected), 0.0);
		specularFactor = pow(specularFactor, max(material.shininess, 1.0));
		specular = sunColour * material.specularIntensity * specularFactor * (1.0 - shadowFactor);
	}

	return vec4(key + scatter + specular, 1.0);
}

vec4 CalcPointLight(PointLight pLight, int shadowIndex)
{
	vec3 direction = FragPos - pLight.position;
	float distance = length(direction);
	direction = normalize(direction);
	
	float shadowFactor = CalcOmniShadowFactor(pLight, shadowIndex);
	
	vec4 colour = CalcLightByDirection(pLight.base, direction, shadowFactor);
	float attenuation = pLight.exponent * distance * distance +
						pLight.linear * distance +
						pLight.constant;
	
	return (colour / attenuation);
}

vec4 CalcSpotLight(SpotLight sLight, int shadowIndex)
{
	vec3 rayDirection = normalize(FragPos - sLight.base.position);
	float slFactor = dot(rayDirection, sLight.direction);
	
	if(slFactor > sLight.edge)
	{
		vec4 colour = CalcPointLight(sLight.base, shadowIndex);
		
		return colour * (1.0f - (1.0f - slFactor)*(1.0f/(1.0f - sLight.edge)));
		
	} else {
		return vec4(0, 0, 0, 0);
	}
}

vec4 CalcPointLights()
{
	vec4 totalColour = vec4(0, 0, 0, 0);
	for(int i = 0; i < pointLightCount; i++)
	{		
		totalColour += CalcPointLight(pointLights[i], i);
	}
	
	return totalColour;
}

vec4 CalcSpotLights()
{
	vec4 totalColour = vec4(0, 0, 0, 0);
	for(int i = 0; i < spotLightCount; i++)
	{		
		totalColour += CalcSpotLight(spotLights[i], i + pointLightCount);
	}
	
	return totalColour;
}

void main()
{
	vec3 norm = Normal;
	if (length(norm) > 0.001) {
		norm = normalize(norm);
	} else {
		norm = vec3(0.0, 1.0, 0.0);
	}

	vec3 surfaceColor;
	float texAlpha = 1.0;
	if (useTexture != 0) {
		vec4 texSample = texture(theTexture, TexCoord);
		vec3 vertColor = (length(vCol.rgb) > 0.01) ? vCol.rgb : vec3(0.7, 0.7, 0.7);
		// Alpha marks a real texel. Unfilled atlas samples stay transparent; use the
		// vertex color and keep the fragment opaque so black albedo is not discarded.
		if (texSample.a < 0.5) {
			surfaceColor = vertColor;
		} else {
			surfaceColor = texSample.rgb;
			texAlpha = texSample.a;
		}
	} else if (length(vCol.rgb) > 0.01) {
		surfaceColor = vCol.rgb;
	} else {
		surfaceColor = vec3(0.7, 0.7, 0.7);
	}

	vec4 lit = CalcDirectionalLight() + CalcPointLights() + CalcSpotLights();
	float alpha = clamp(materialOpacity * texAlpha, 0.0, 1.0);
	vec3 radiance = surfaceColor * lit.rgb;
	radiance = radiance / (vec3(1.0) + radiance);
	colour = vec4(radiance, alpha);
}



