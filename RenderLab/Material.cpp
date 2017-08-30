#include "stdafx.h"
#include "Material.h"

namespace RenderLab
{
  Material::Material(string name, Type materialType):
    m_name(name),
    m_materialType(materialType),
    m_lightingEnable(true),
    m_blendEnable(false),
    m_albedoColor(vec4(1.0f)),
    m_emissiveColor(vec3(0.0f)),
    m_twoSided(false),
    m_albedoTexture(nullptr),
    m_normalTexture(nullptr),
    m_emissiveTexture(nullptr),
    m_metallicRoughnessTexture(nullptr),
    m_occlusionTexture(nullptr),
    m_noTexture(true),
    m_graphicsData(nullptr),
    m_metallic(1.0f),
    m_roughness(1.0f),
    m_dirty(true)
  {
  }


  Material::~Material()
  {
  }

  Material::Type Material::getMaterialType()
  {
    return m_materialType;
  }

  void Material::setAlbedoTexture(shared_ptr<Texture> texture)
  {
    m_albedoTexture = texture;
    m_noTexture = false;
  }

  shared_ptr<Texture> Material::getAlbedoTexture()
  {
    return m_albedoTexture;
  }

  void Material::setNormalTexture(shared_ptr<Texture> texture)
  {
    m_normalTexture = texture;
    m_noTexture = false;
  }

  shared_ptr<Texture> Material::getNormalTexture()
  {
    return m_normalTexture;
  }

  void Material::setMetallicRoughnessTexture(shared_ptr<Texture> texture)
  {
    m_metallicRoughnessTexture = texture;
    m_noTexture = false;
  }

  shared_ptr<Texture> Material::getMetallicRoughnessTexture()
  {
    return m_metallicRoughnessTexture;
  }

  void Material::setEmissiveTexture(shared_ptr<Texture> texture)
  {
    m_emissiveTexture = texture;
    m_noTexture = false;
  }

  shared_ptr<Texture> Material::getEmissiveTexture()
  {
    return m_emissiveTexture;
  }

  void Material::setOcclusionTexture(shared_ptr<Texture> texture)
  {
    m_occlusionTexture = texture;
    m_noTexture = false;
  }

  shared_ptr<Texture> Material::getOcclusionTexture()
  {
    return m_occlusionTexture;
  }

  bool Material::hasNoTexture()
  {
    return m_noTexture;
  }

  void Material::setAlbedoColor(vec4& albedoColor)
  {
    m_albedoColor = albedoColor;
  }

  void Material::getAlbedoColor(vec4& albedoColor)
  {
    albedoColor = m_albedoColor;
  }

  void Material::setMetallic(float metallic)
  {
    m_metallic = metallic;
  }

  float Material::getMetallic()
  {
    return m_metallic;
  }

  void Material::setRoughness(float roughness)
  {
    m_roughness = roughness;
  }

  float Material::getRoughness()
  {
    return m_roughness;
  }


  void Material::setEmissiveColor(vec3& emissiveColor)
  {
    m_emissiveColor = emissiveColor;
  }

  void Material::getEmissiveColor(vec3& emissiveColor)
  {
    emissiveColor = m_emissiveColor;
  }

  void Material::setTwoSided(bool twoSided)
  {
    m_twoSided = twoSided;
  }

  bool Material::getTwoSided()
  {
    return m_twoSided;
  }

  void Material::setLightingEnable(bool enable)
  {
    m_lightingEnable = enable;
  }

  bool Material::getLightingEnable()
  {
    return m_lightingEnable;
  }

  void Material::setBlendEnable(bool enable)
  {
    m_blendEnable = enable;
  }

  bool Material::getBlendEnable()
  {
    return m_lightingEnable;
  }

  void Material::setGraphicsData(void * graphicsData)
  {
    m_graphicsData = graphicsData;
  }

  void* Material::getGraphicsData()
  {
    return m_graphicsData;
  }

  void Material::setDirty(bool dirty)
  {
    m_dirty = dirty;
  }

  bool Material::isDirty()
  {
    return m_dirty;
  }
}
