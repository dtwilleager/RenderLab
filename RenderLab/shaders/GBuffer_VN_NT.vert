#version 440

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
//layout(location = 2) in vec2 in_tex_coord0;

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
	vec4 albedoColor;
	vec4 emmisiveColor;
	vec4 metallicRoughness;
	vec4 flags;
} objectParams;


layout(location = 0) out vec3 world_pos;
layout(location = 1) out vec3 world_normal;
layout(location = 2) out vec2 tex_coord0;

void main()
{
  gl_Position = objectParams.view_projection * objectParams.model * vec4(in_pos, 1.0);

  world_pos = (objectParams.model * vec4(in_pos, 1.0)).xyz;
  world_normal = normalize(vec3(objectParams.model * vec4(in_normal, 0.0)));
  tex_coord0 = vec2(0.0, 0.0);
}
