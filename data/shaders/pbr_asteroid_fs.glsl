#version 450 core
// Physically Based Rendering
// * Forked from Michał Siejak PBR project

// Physically Based shading model: Lambetrtian diffuse BRDF + Cook-Torrance microfacet specular BRDF + IBL for ambient.

// This implementation is based on "Real Shading in Unreal Engine 4" SIGGRAPH 2013 course notes by Epic Games.
// See: http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf

const float PI = 3.1415926535897932384626433832795;
const float Epsilon = 0.00001;

const int NumLights = 3;

// Constant normal incidence Fresnel factor for all dielectrics.
const vec3 Fdielectric = vec3(0.04);

struct AnalyticalLight
{
	vec3 direction;
	vec3 radiance;
};

layout(location=0) in vec2 koef_scr_diff_fs_in;
layout(location=1) in vec2 texcoord_fs_in;
layout(location=2) in vec3 position_fs_in;
layout(location=3) in vec3 mesh_pos_fs_in;
layout(location=4) in mat3 tangent_basis_fs_in;

layout(std140, binding=1) uniform ShadingUniforms
{
	AnalyticalLight lights[NumLights];
	vec3 eyePosition;
};

layout(std140, binding=2) uniform BaseInfoUniforms
{
	mat4 modelMatr;
	mat4 modelViewMat;
	// mat4 modelViewProjMat;
	int opaquePass;
};

layout(location=0) out vec4 color;
layout(location=1) out vec4 accumulation;
layout(location=2) out float counter;

layout(binding=0) uniform sampler2D albedoTexture;
// layout(binding=1) uniform sampler2D normalTexture;
// layout(binding=2) uniform sampler2D metalnessTexture;
// layout(binding=3) uniform sampler2D roughnessTexture;
layout(binding=4) uniform samplerCube specularTexture;
layout(binding=5) uniform samplerCube irradianceTexture;
layout(binding=6) uniform sampler2D specularBRDF_LUT;

#include "asteroid_base.glsl"

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
float ndfGGX(float cosLh, float roughness)
{
	float alpha   = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float cosLo, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return gaSchlickG1(cosLi, k) * gaSchlickG1(cosLo, k);
}

// Shlick's approximation of the Fresnel factor.
vec3 fresnelSchlick(vec3 F0, float cosTheta)
{
	return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
}

mat3 rotationMatrix(in vec3 axis, in float angle)
{
	float s = sin(angle);
	float c = cos(angle);
	float oc = 1.0 - c;

	axis = normalize(axis);
	return mat3(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
				oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
				oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c          );
}

void main()
{
	// Sample input textures to get shading model params.
	if (0 == opaquePass)
	{
		discard;
	}
    float levelCount = clamp(20.1 + (log(koef_scr_diff_fs_in.y)) / log(2.0), START_LEVEL, MAX_LEVEL) - START_LEVEL;
	float smoothing = exp(0.4 * log(koef_scr_diff_fs_in.x));
	vec4 noise = height_map(normalize(mesh_pos_fs_in), START_LEVEL, levelCount, smoothing);

	if (koef_scr_diff_fs_in.y > 0)
	{
		float _ang = 0.00006 / length(mesh_pos_fs_in) / koef_scr_diff_fs_in.y;
		vec3 _eyeDir = normalize(vec3(inverse(modelViewMat) * vec4(0, 0, 0, 1)) - mesh_pos_fs_in);
		vec3 _nml = normalize(tangent_basis_fs_in * vec3(0., 0., 1.));
		vec3 _axis = normalize(cross(_eyeDir, _nml));
		noise.xyz += height_map(normalize(rotationMatrix(_axis,  _ang) * mesh_pos_fs_in), START_LEVEL, levelCount, smoothing).xyz;		
// 		noise.xyz += height_map(normalize(rotationMatrix(_axis, -_ang) * mesh_pos_fs_in), START_LEVEL, levelCount, smoothing).xyz;
		if (koef_scr_diff_fs_in.x < 0.35)
		{
			noise.xyz += height_map(normalize(rotationMatrix(_axis,  1.5 * _ang) * mesh_pos_fs_in), START_LEVEL, levelCount, smoothing).xyz;
			noise.xyz += height_map(normalize(rotationMatrix(_axis, -1.5 * _ang) * mesh_pos_fs_in), START_LEVEL, levelCount, smoothing).xyz;
		}
		noise.xyz = normalize(noise.xyz);
	}

	vec3 rotAxis = cross(normalize(noise.xyz), normalize(mesh_pos_fs_in));
	float s = length(rotAxis);
	float c = sqrt(1.0 - s * s);
	float oc = 1.0 - c;
	vec3 axis = (0 == s) ? rotAxis : rotAxis / s;
    mat3 rotMat = mat3( oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
                    	oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
                    	oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c);

	vec4 albedoColor = texture(albedoTexture, texcoord_fs_in);
	vec3 albedo = vec3(6.0 * exp(2.5 * log(noise.a))) * albedoColor.rgb;
	float metalness = 0.05;//texture(metalnessTexture, vin.texcoord).r;
	float roughness = 0.9;//texture(roughnessTexture, vin.texcoord).r;

	// Outgoing light direction (vector from world-space fragment position to the "eye").
	vec3 Lo = normalize(eyePosition - position_fs_in);

	// Get current fragment's normal and transform to world space.
	vec3 N = rotMat * normalize(mat3(modelMatr) * tangent_basis_fs_in * vec3(0, 0, 1));//normalize(2.0 * texture(normalTexture, texcoord_fs_in).rgb - 1.0));

	// Angle between surface normal and outgoing light direction.
	float cosLo = max(0.001, dot(N, Lo));

	// Specular reflection vector.
	vec3 Lr = 2.0 * cosLo * N - Lo;

	// Fresnel reflectance at normal incidence (for metals use albedo color).
	vec3 F0 = mix(Fdielectric, albedo, metalness);

	// Direct lighting calculation for analytical lights.
	vec3 directLighting = vec3(0);
	for(int i = 0; i < NumLights; i++)
	{
		vec3 Li = -lights[i].direction;
		vec3 Lradiance = lights[i].radiance;

		// Half-vector between Li and Lo.
		vec3 Lh = normalize(Li + Lo);

		// Calculate angles between surface normal and various light vectors.
		float cosLi = max(0.001, dot(N, Li));
		float cosLh = max(0.001, dot(N, Lh));

		// Calculate Fresnel term for direct lighting. 
		vec3 F = fresnelSchlick(F0, max(0.0, dot(Lh, Lo)));
		// Calculate normal distribution for specular BRDF.
		float D = ndfGGX(cosLh, roughness);
		// Calculate geometric attenuation for specular BRDF.
		float G = gaSchlickGGX(cosLi, cosLo, roughness);

		// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
		// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
		// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
		vec3 kd = mix(vec3(1.0) - F, vec3(0.0), metalness);

		// Lambert diffuse BRDF.
		// We don't scale by 1/PI for lighting & material units to be more convenient.
		// See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
		vec3 diffuseBRDF = kd * albedo;

		// Cook-Torrance specular microfacet BRDF.
		vec3 specularBRDF = (F * D * G) / max(Epsilon, 4.0 * cosLi * cosLo);

		// Total contribution for this light.
		directLighting += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;
	}

	// Ambient lighting (IBL).
	vec3 ambientLighting;
	{
		// Sample diffuse irradiance at normal direction.
		vec3 irradiance = texture(irradianceTexture, N).rgb;

		// Calculate Fresnel term for ambient lighting.
		// Since we use pre-filtered cubemap(s) and irradiance is coming from many directions
		// use cosLo instead of angle with light's half-vector (cosLh above).
		// See: https://seblagarde.wordpress.com/2011/08/17/hello-world/
		vec3 F = fresnelSchlick(F0, cosLo);

		// Get diffuse contribution factor (as with direct lighting).
		vec3 kd = mix(vec3(1.0) - F, vec3(0.0), metalness);

		// Irradiance map contains exitant radiance assuming Lambertian BRDF, no need to scale by 1/PI here either.
		vec3 diffuseIBL = kd * albedo * irradiance;

		// Sample pre-filtered specular reflection environment at correct mipmap level.
		int specularTextureLevels = textureQueryLevels(specularTexture);
		vec3 specularIrradiance = textureLod(specularTexture, Lr, roughness * specularTextureLevels).rgb;

		// Split-sum approximation factors for Cook-Torrance specular BRDF.
		vec2 specularBRDF = texture(specularBRDF_LUT, vec2(cosLo, roughness)).rg;

		// Total specular IBL contribution.
		vec3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularIrradiance;

		// Total ambient lighting contribution.
		ambientLighting = diffuseIBL + specularIBL;
	}

	// Final fragment color.
	if (0 != opaquePass)
	{
		color = vec4(directLighting + ambientLighting, 1.0);
	}
}
