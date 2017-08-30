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

layout(push_constant) uniform LightData {
  vec4 lightColor;
	vec4 lightPosition;
} lightData;

void main()
{  
}



