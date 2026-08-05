#version 460

#define RAYTRACE 1

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

struct ShadeData
{
	vec3 cameraPosition;
	vec3 sunDirection;

	mat4 inverseViewProjection;

	vec2 imageSize;
};

layout(push_constant) uniform block
{
	ShadeData shadeData;
};

layout(binding = 0) uniform writeonly image2D outImage;
layout(binding = 1) uniform sampler2D gBufferImage0;
layout(binding = 2) uniform sampler2D gBufferImage1;
layout(binding = 3) uniform sampler2D depthImage;

#if RAYTRACE
#extension GL_EXT_ray_query: require

layout(binding = 4) uniform accelerationStructureEXT tlas;
#endif

void main()
{
	uvec2 pos = gl_GlobalInvocationID.xy;
	vec2 uv = (vec2(pos) + 0.5) / shadeData.imageSize;

	vec4 gBuffer0 = texture(gBufferImage0, uv);
	vec4 gBuffer1 = texture(gBufferImage1, uv);
	float depth = texture(depthImage, uv).r;

	vec3 albedo = gBuffer0.rgb;
	vec3 emissive = vec3(gBuffer0.a);
	vec3 normal = gBuffer1.rgb * 2 - 1;

	float ndotl = max(dot(normal, shadeData.sunDirection), 0.0);

	vec4 clip = vec4(uv.x * 2 - 1, 1 - uv.y * 2, depth, 1);
	vec4 wPosh = shadeData.inverseViewProjection * clip;
	vec3 wPos = wPosh.xyz / wPosh.w;

	vec3 view = normalize(shadeData.cameraPosition - wPos);
	vec3 halfV = normalize(view + shadeData.sunDirection);
	float ndoth = max(dot(normal, halfV), 0.0);
	float specular = pow(ndoth, 64);

	float shadow = 1;

#if RAYTRACE
	uint rayflags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsCullNoOpaqueEXT;
	uint cullMask = 0xff; // 0xff is faster on amdvlk

	rayQueryEXT rq;
	rayQueryInitializeEXT(rq, tlas, rayflags, cullMask, wPos, 1e-2, shadeData.sunDirection, 1e3);
	rayQueryProceedEXT(rq);

	shadow = (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT) ? 1.0 : 0.0;
#endif

	vec3 outputColor = albedo.rgb * sqrt(ndotl * shadow + 0.05) + vec3(specular * shadow) + emissive;
	//outputColor = albedo;

	imageStore(outImage, ivec2(pos), vec4(outputColor, 1.0));
}