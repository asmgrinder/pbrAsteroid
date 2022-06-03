#version 450 core
// Physically Based Rendering
// * Forked from Michał Siejak PBR project

// Physically Based shading model: Vertex program.

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 tangent;
layout(location=3) in vec3 bitangent;
layout(location=4) in vec2 texcoord;

layout(std140, binding=0) uniform ModelMatrixUniform
{
	mat4 modelViewProjectionMatrix;
};

layout(location=0) out vec3 position_cs_in;
layout(location=1) out vec3 normal_cs_in;
layout(location=2) out vec2 texcoord_cs_in;
layout(location=3) out vec3 tangent_cs_in;
layout(location=4) out vec3 bitangent_cs_in;

#include "asteroid_base.glsl"

void main()
{
	texcoord_cs_in = vec2(texcoord.x, 1.0 - texcoord.y);
	normal_cs_in = normalize(normal);
	tangent_cs_in = tangent;
	bitangent_cs_in = bitangent;

	position_cs_in = position;
	vec4 noise = height_map(normalize(position_cs_in), START_LEVEL, MAX_LEVEL - 5, 1.0);
	vec4 proj = modelViewProjectionMatrix * vec4(height_mapping(noise.a) * position_cs_in, 1.0);
	gl_Position = proj;
}
