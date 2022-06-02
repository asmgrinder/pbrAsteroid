#version 450 core

layout (vertices = 3) out;

layout(location=0) in vec3 position_cs_in[];
layout(location=1) in vec3 normal_cs_in[];
layout(location=2) in vec2 texcoord_cs_in[];
layout(location=3) in vec3 tangent_cs_in[];
layout(location=4) in vec3 bitangent_cs_in[];

layout(std140, binding=4) uniform TessControlMatrixUniform
{
	mat4 modelViewMatrix;
    mat4 projectionMatrix;
	vec4 viewport_cs;
};

layout(location=0) out vec3 position_es_in[];
layout(location=1) out vec3 normal_es_in[];
layout(location=2) out vec2 texcoord_es_in[];
layout(location=3) out vec3 tangent_es_in[];
layout(location=4) out vec3 bitangent_es_in[];

#include "asteroid_base.glsl"

#define MAX_EDGE_LENGTH 10.0
#define MAX_TESS 28

vec2 getScreenPos(in vec4 ViewPoint)
{
    vec2 vp = 0.5 * (ViewPoint.xy / max(ViewPoint.w, 0.000001) + 1.0);
    return vp * viewport_cs.zw + viewport_cs.xy;
}

int getTessLevel(vec4 p1, vec4 p2)
{
    vec4 pc = 0.5 * (p1 + p2);
    float len = 0.5 * length(vec3(p1) - vec3(p2));
    vec4 pp1 = pc - vec4(len, 0.0, 0.0, 0.0);
    vec4 pp2 = pc + vec4(len, 0.0, 0.0, 0.0);

    float scrLen = length(getScreenPos(projectionMatrix * pp1)
                            - getScreenPos(projectionMatrix * pp2));
    return int(0.5 + min(MAX_TESS, max(1, scrLen / MAX_EDGE_LENGTH)));
}

void main()
{
    texcoord_es_in[gl_InvocationID] = texcoord_cs_in[gl_InvocationID];
    normal_es_in[gl_InvocationID] = normal_cs_in[gl_InvocationID];
    tangent_es_in[gl_InvocationID] = tangent_cs_in[gl_InvocationID];
    bitangent_es_in[gl_InvocationID] = bitangent_cs_in[gl_InvocationID];
    position_es_in[gl_InvocationID] = position_cs_in[gl_InvocationID];

    if (0 == gl_InvocationID)
    {
        vec4 noise0 = height_map(normalize(position_cs_in[0]), START_LEVEL, MAX_LEVEL - 5, 1.0);
        vec4 noise1 = height_map(normalize(position_cs_in[1]), START_LEVEL, MAX_LEVEL - 5, 1.0);
        vec4 noise2 = height_map(normalize(position_cs_in[2]), START_LEVEL, MAX_LEVEL - 5, 1.0);

        vec4 pp[3];
        pp[0] = modelViewMatrix * vec4(height_mapping(noise0.a) * position_cs_in[0], 1.0);
        pp[1] = modelViewMatrix * vec4(height_mapping(noise1.a) * position_cs_in[1], 1.0);
        pp[2] = modelViewMatrix * vec4(height_mapping(noise2.a) * position_cs_in[2], 1.0);

        vec4 p0 = projectionMatrix * pp[0];
        vec4 p1 = projectionMatrix * pp[1];
        vec4 p2 = projectionMatrix * pp[2];

        p0 *= sign(p0.w);
        p1 *= sign(p1.w);
        p2 *= sign(p2.w);

        const float mult = 1.4;
        if (p0.x >  mult * p0.w && p1.x >  mult * p1.w && p2.x >  mult * p2.w
            || p0.x < -mult * p0.w && p1.x < -mult * p1.w && p2.x < -mult * p2.w
            || p0.y >  mult * p0.w && p1.y >  mult * p1.w && p2.y >  mult * p2.w
            || p0.y < -mult * p0.w && p1.y < -mult * p1.w && p2.y < -mult * p2.w)
        {
            gl_TessLevelOuter[0] = gl_TessLevelOuter[1] = gl_TessLevelOuter[2] = 0;
            gl_TessLevelInner[0] = 0;
        }
        else
        {
            gl_TessLevelOuter[0] = getTessLevel(pp[1], pp[2]);
            gl_TessLevelOuter[1] = getTessLevel(pp[2], pp[0]);
            gl_TessLevelOuter[2] = getTessLevel(pp[0], pp[1]);
            float maxLevel = max(gl_TessLevelOuter[0], max(gl_TessLevelOuter[1], gl_TessLevelOuter[2]));
            gl_TessLevelInner[0] = int(0.5 + min(MAX_TESS, max(1, maxLevel - 1)));
        }
    }
}
