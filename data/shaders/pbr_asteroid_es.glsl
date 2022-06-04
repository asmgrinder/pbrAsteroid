#version 450 core

layout(triangles, equal_spacing, ccw) in;

layout(location=0) in vec3 position_es_in[];
layout(location=1) in vec3 normal_es_in[];
layout(location=2) in vec2 texcoord_es_in[];
layout(location=3) in vec3 tangent_es_in[];
layout(location=4) in vec3 bitangent_es_in[];

layout(std140, binding=3) uniform ViewProjectionMatrixUniform
{
    mat4 modelMat;
    mat4 viewMatrix;
    mat4 modelViewProjectionMat;
    vec4 viewport;
};

layout(location=0) out vec2 koef_scr_diff_fs_in;
layout(location=1) out vec2 texcoord_fs_in;
layout(location=2) out vec3 position_fs_in;
layout(location=3) out vec3 mesh_pos_fs_in;
layout(location=4) out mat3 tangent_basis_fs_in;

#include "asteroid_base.glsl"

vec2 interpolate2D(vec2 v0, vec2 v1, vec2 v2)
{
    return gl_TessCoord.x * v0 + gl_TessCoord.y * v1 + gl_TessCoord.z * v2;
}

vec3 interpolate3D(vec3 v0, vec3 v1, vec3 v2)
{
    return gl_TessCoord.x * v0 + gl_TessCoord.y * v1 + gl_TessCoord.z * v2;
}

vec3 interpolateHeight(vec3 v0, vec3 v1, vec3 v2)
{
	float newHeight = gl_TessCoord.x * length(v0) + gl_TessCoord.y * length(v1) + gl_TessCoord.z * length(v2);
	return newHeight * normalize(gl_TessCoord.x * v0 + gl_TessCoord.y * v1 + gl_TessCoord.z * v2);
}

vec2 getScreenPos(in vec4 ViewPoint)
{
    vec2 vp = 0.5 * (ViewPoint.xy / max(ViewPoint.w, 0.000001) + 1.0);
    return vp * viewport.zw + viewport.xy;
}

void main()
{
    vec3 normal = normalize(interpolate3D(normal_es_in[0], normal_es_in[1], normal_es_in[2]));
	vec3 tangent = normalize(interpolate3D(tangent_es_in[0], tangent_es_in[1], tangent_es_in[2]));
	vec3 bitangent = normalize(interpolate3D(bitangent_es_in[0], bitangent_es_in[1], bitangent_es_in[2]));

	texcoord_fs_in = interpolate2D(texcoord_es_in[0], texcoord_es_in[1], texcoord_es_in[2]);

    mesh_pos_fs_in = interpolateHeight(position_es_in[0], position_es_in[1], position_es_in[2]);

    vec4 noise = height_map(normalize(mesh_pos_fs_in), START_LEVEL, MAX_LEVEL - 5, 1.0);//vec4(normalize(mesh_pos_fs_in), 0.5);//

	mesh_pos_fs_in *= height_mapping(noise.a);

    tangent_basis_fs_in = mat3(tangent, bitangent, normal);
	vec3 N = mat3(modelMat) * normalize(tangent_basis_fs_in * vec3(0, 0, 1));
	vec3 eyeDir = normalize(vec3(viewMatrix * modelMat * vec4(mesh_pos_fs_in, 1.0)));
    koef_scr_diff_fs_in.x = max(0.01, dot(normalize(mat3(viewMatrix) * N), normalize(-eyeDir)));

	vec4 position = vec4(mesh_pos_fs_in, 1.0);
    position_fs_in = vec3(modelMat * position);
    gl_Position = position = modelViewProjectionMat * position;
    const vec2 screenPos = getScreenPos(position);

	const float rotAng = 1.0 / 256.0 / length(mesh_pos_fs_in);
    vec4 vertex1 = vec4(rotate2D(mesh_pos_fs_in.xy, rads(rotAng)), mesh_pos_fs_in.z, 1.0);
    vec2 diff1 = abs(getScreenPos(modelViewProjectionMat * vertex1) - screenPos);

    vec4 vertex2 = vec4(mesh_pos_fs_in.x, rotate2D(mesh_pos_fs_in.yz, rads(rotAng)), 1.0);
    vec2 diff2 = abs(getScreenPos(modelViewProjectionMat * vertex2) - screenPos);

    vec4 vertex3 = vec4(mesh_pos_fs_in.xyz, 1.0);
    vertex3.xz = rotate2D(mesh_pos_fs_in.xz, rads(rotAng));
    vec2 diff3 = abs(getScreenPos(modelViewProjectionMat * vertex3) - screenPos);

    koef_scr_diff_fs_in.y = max(0.0001, max(length(diff1), max(length(diff2), length(diff3))));
}
