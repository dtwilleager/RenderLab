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

layout (binding = 2) uniform sampler2D position_sampler;
layout (binding = 3) uniform sampler2D normal_sampler;
layout (binding = 4) uniform sampler2D albedo_sampler;
layout (binding = 5) uniform sampler2D metallic_roughness_sampler;
layout (binding = 6) uniform sampler2D emissive_sampler;
//layout (binding = 7) uniform samplerCube shadow_sampler[3];


layout(location = 0) out vec4 fragcolor;

#define shadowAmbient 0.1
#define EPSILON 0.15

#define ambientIntensity 0.1
#define PI 3.14159265359f

vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
); 

//float computeShadow(int shadowIndex, vec3 world_pos)
//{
//  float shadow = 0.0;
//  float bias = 0.15; 
//  float samples = 20;
//  float far_plane = 1000.0;
//
//  float viewDistance = length(frameParams.viewPosition.xyz - world_pos);
//  float diskRadius = (1.0 + (viewDistance / far_plane)) / 25.0;
//
//  vec3 light_dir = world_pos - frameParams.lights[shadowIndex].light_position.xyz;
//  float currentDepth = length(light_dir);
//
//  for(int i = 0; i < samples; i++)
//  {
//    float closestDepth = texture(shadow_sampler[shadowIndex], light_dir + sampleOffsetDirections[i] * diskRadius).r; 
//    closestDepth *= far_plane;   // Undo mapping [0;1]
//    if(currentDepth - bias > closestDepth)
//       shadow += 1.0;
//  }
//  shadow /= float(samples);
//  return 1.0 - shadow;
//}

vec3 Fresnel_Schlick(vec3 specularColor, vec3 l, vec3 h)
{
    return (specularColor + (1.0f - specularColor) * pow((1.0f - dot(l, h)), 5));
}

vec3 Specular_F(vec3 specularColor, vec3 l, vec3 h)
{
    //return Fresnel_None(specularColor);
    return Fresnel_Schlick(specularColor, l, h);
    //return Fresnel_CookTorrance(specularColor, h, v);
}

float NormalDistribution_GGX(float a, float NdH)
{
    // Isotropic ggx.
    float a2 = a*a;
    float NdH2 = NdH * NdH;

    float denominator = NdH2 * (a2 - 1.0f) + 1.0f;
    denominator *= denominator;
    denominator *= PI;

    return a2 / denominator;
}

float NormalDistribution_BlinnPhong(float a, float NdH)
{
    return (1 / (PI * a * a)) * pow(NdH, 2 / (a * a) - 2);
}

float NormalDistribution_Beckmann(float a, float NdH)
{
    float a2 = a * a;
    float NdH2 = NdH * NdH;

    return (1.0f/(PI * a2 * NdH2 * NdH2 + 0.001)) * exp( (NdH2 - 1.0f) / ( a2 * NdH2));
}

float Specular_D(float a, float NdH)
{
    //return NormalDistribution_BlinnPhong(a, NdH);
    //return NormalDistribution_Beckmann(a, NdH);
    return NormalDistribution_GGX(a, NdH);
}

//-------------------------- Geometric shadowing -------------------------------------------
float Geometric_Implicit(float a, float NdV, float NdL)
{
    return NdL * NdV;
}

float Geometric_Neumann(float a, float NdV, float NdL)
{
    return (NdL * NdV) / max(NdL, NdV);
}

float Geometric_CookTorrance(float a, float NdV, float NdL, float NdH, float VdH)
{
    return min(1.0f, min((2.0f * NdH * NdV)/VdH, (2.0f * NdH * NdL)/ VdH));
}

float Geometric_Kelemen(float a, float NdV, float NdL, float LdV)
{
    return (2 * NdL * NdV) / (1 + LdV);
}

float Geometric_Beckman(float a, float dotValue)
{
    float c = dotValue / ( a * sqrt(1.0f - dotValue * dotValue));

    if ( c >= 1.6f )
    {
        return 1.0f;
    }
    else
    {
        float c2 = c * c;
        return (3.535f * c + 2.181f * c2) / ( 1 + 2.276f * c + 2.577f * c2);
    }
}

float Geometric_Smith_Beckmann(float a, float NdV, float NdL)
{
    return Geometric_Beckman(a, NdV) * Geometric_Beckman(a, NdL);
}

float Geometric_GGX(float a, float dotValue)
{
    float a2 = a * a;
    return (2.0f * dotValue) / (dotValue + sqrt(a2 + ((1.0f - a2) * (dotValue * dotValue))));
}

float Geometric_Smith_GGX(float a, float NdV, float NdL)
{
    return Geometric_GGX(a, NdV) * Geometric_GGX(a, NdL);
}

float Geometric_Smith_Schlick_GGX(float a, float NdV, float NdL)
{
        // Smith schlick-GGX.
    float k = a * 0.5f;
    float GV = NdV / (NdV * (1 - k) + k);
    float GL = NdL / (NdL * (1 - k) + k);

    return GV * GL;
}

float Specular_G(float a, float NdV, float NdL, float NdH, float VdH, float LdV)
{
  //return Geometric_Implicit(a, NdV, NdL);
  //return Geometric_Neumann(a, NdV, NdL);
  //return Geometric_CookTorrance(a, NdV, NdL, NdH, VdH);
  //return Geometric_Kelemen(a, NdV, NdL, LdV);
  //return Geometric_Smith_Beckmann(a, NdV, NdL);
  //return Geometric_Smith_GGX(a, NdV, NdL);
  return Geometric_Smith_Schlick_GGX(a, NdV, NdL);
}

vec3 Specular(vec3 specularColor, vec3 h, vec3 v, vec3 l, float a, float NdL, float NdV, float NdH, float VdH, float LdV)
{
  vec3 fresnel = Specular_F(specularColor, l, h);
  float normalDistribution = Specular_D(a, NdH);
  float visibility = Specular_G(a, NdV, NdL, NdH, VdH, LdV) / (4.0f * NdL * NdV + 0.0001f);
  return (fresnel * normalDistribution * visibility);
}

void main()
{  
  vec2 tex_coord;

  tex_coord.x = gl_FragCoord.x / lightData.lightColor.a;
  tex_coord.y = gl_FragCoord.y / lightData.lightPosition.w;

	vec4 position_sample = texture(position_sampler, tex_coord);
	vec3 world_normal = texture(normal_sampler, tex_coord).rgb;
	vec3 albedo = texture(albedo_sampler, tex_coord).rgb;

  vec3 world_position = position_sample.rgb;
  float roughness = position_sample.a;

  //int numLights = frameParams.lightInfo.y;
  vec3 ambient = vec3(0.0f, 0.0f, 0.0f); //albedo * ambientIntensity;
  vec3 specularColor = vec3(0.04, 0.04, 0.04);

	vec3 finalColor = ambient;

  //for (int i=0; i<numLights; i++) 
	//{
		//vec3 lightColor = frameParams.lights[i].light_color.rgb;
    //vec3 lightDirection = frameParams.lights[i].light_position.xyz - world_position;
    vec3 lightDirection = lightData.lightPosition.xyz - world_position;
    float lightDistance = length(lightDirection);
    lightDirection = normalize(lightDirection);

    vec3 viewDirection = normalize(frameParams.viewPosition.xyz - world_position);

		float attenuation = 30 * PI/(lightDistance * lightDistance);;
		
		vec3 diffuse = albedo / PI;

    float NdL = clamp(dot(world_normal, lightDirection), 0.0, 1.0);
    float NdV = clamp(dot(world_normal, viewDirection), 0.0, 1.0);
    vec3 h = normalize(lightDirection + viewDirection);
    float NdH = clamp(dot(world_normal, h), 0.0, 1.0);
    float VdH = clamp(dot(viewDirection, h), 0.0, 1.0);
    float LdV = clamp(dot(lightDirection, viewDirection), 0.0, 1.0);
    float a = max(0.001f, roughness * roughness);

    vec3 spec = Specular(specularColor, h, viewDirection, lightDirection, a, NdL, NdV, NdH, VdH, LdV);

		float shadow = 1.0; //computeShadow(i, world_position);
		
		finalColor += (attenuation * shadow * NdL * lightData.lightColor.rgb * (diffuse * (1.0 - spec) + spec));
	//}
	
  fragcolor = vec4(finalColor, 1.0);
}



