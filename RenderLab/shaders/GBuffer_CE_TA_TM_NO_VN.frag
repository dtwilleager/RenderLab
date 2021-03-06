#version 440

precision highp float;

struct Light
{
  mat4 light_view_projections[6];
  vec4 light_position;
  vec4 light_color;
};

layout(std140, set = 0, binding = 0) readonly buffer frame_param_block {
	ivec4 lightInfo;
	vec4 viewPosition;
	Light lights[6];
} frameParams;

layout(std140, set = 0, binding = 1) readonly buffer object_param_block {
	mat4 model;
	mat4 view_projection;
	vec4 albedo_color;
	vec4 emmisive_color;
	vec4 metallic_roughness;
	vec4 flags;
} objectParams;

layout(location = 0) in vec3 world_pos;
layout(location = 1) in vec3 world_normal;
layout(location = 2) in vec2 tex_coord0;

layout(binding = 2) uniform sampler2D albedo_sampler;
layout(binding = 4) uniform sampler2D metallic_roughness_sampler;
//layout(binding = 4) uniform samplerCube shadow_sampler[3];

layout (location = 0) out vec4 out_position;
layout (location = 1) out vec4 out_normal;
layout (location = 2) out vec4 out_albedo;
layout (location = 3) out vec4 out_metallic_roughness_flags;
layout (location = 4) out vec4 out_emissive;

void main()
{
  // Constant Emissive
  out_emissive = objectParams.emmisive_color;

  // Texture Albedo
  out_albedo = texture(albedo_sampler, tex_coord0);
  if (out_albedo.a == 0.0)
  {
    discard;
  }

  // Texture Metallic/Roughness
  vec4 metallic_roughness = texture(metallic_roughness_sampler, tex_coord0);
  float roughness = metallic_roughness.g;
  float metallic = metallic_roughness.r;

  out_metallic_roughness_flags.r = metallic;
  out_metallic_roughness_flags.g = roughness;
  out_metallic_roughness_flags.b = objectParams.flags.r;
  out_metallic_roughness_flags.a = 0.0;

  out_position = vec4(world_pos, 1.0); 

  // Vertex Normal
  out_normal = vec4(normalize(world_normal), 0.0);
}
