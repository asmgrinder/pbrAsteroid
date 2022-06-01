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
#define MAX_TESS 24

vec2 getScreenPos(in vec4 ViewPoint)
{
    vec2 vp = 0.5 * (ViewPoint.xy / max(ViewPoint.w, 0.000001) + 1.0);
    return vp * viewport_cs.zw + viewport_cs.xy;
}

int getTessLevel(int Id)
{
    vec3 pos1 = position_cs_in[(Id + 1) % 3];
    vec3 pos2 = position_cs_in[(Id + 2) % 3];

    vec4 p1 = modelViewMatrix * vec4(pos1, 1.0);
    vec4 p2 = modelViewMatrix * vec4(pos2, 1.0);

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
        gl_TessLevelOuter[0] = getTessLevel(0);
        gl_TessLevelOuter[1] = getTessLevel(1);
        gl_TessLevelOuter[2] = getTessLevel(2);
        float maxLevel = max(gl_TessLevelOuter[0], max(gl_TessLevelOuter[1], gl_TessLevelOuter[2]));
        gl_TessLevelInner[0] = int(0.5 + min(MAX_TESS, max(1, maxLevel - 1)));
    }
}
