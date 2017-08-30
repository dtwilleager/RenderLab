#version 440

layout(location = 0) in vec3 in_pos;

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
} objectParams;

layout(push_constant) uniform LightData {
  vec4 lightColor;
	vec4 lightPosition;
} lightData;

void main()
{
  gl_Position = objectParams.view_projection * objectParams.model * vec4(in_pos, 1.0);
}
