#pragma once

#include "Texture.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <memory>

using std::string;
using std::shared_ptr;

using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;

namespace RenderLab
{
  class Material
  {
  public:
    enum Type
    {
      UNLIT = 0,
      LIT,
      LIT_NORMALMAP,
      LIT_NOTEXTURE,
      SHADOW,
      SHADOW_CUBE,
      DEFERRED_COMPOSITE,
      DEFERRED_LIT,
	    DEPTH_PREPASS
    };

    Material(string name, Type type);
    ~Material();

    Type          getMaterialType();

    // Texture Data (Takes precedence over scalar values
    void                setAlbedoTexture(shared_ptr<Texture> texture);
    shared_ptr<Texture> getAlbedoTexture();
    void                setNormalTexture(shared_ptr<Texture> texture);
    shared_ptr<Texture> getNormalTexture();
    void                setMetallicRoughnessTexture(shared_ptr<Texture> texture);
    shared_ptr<Texture> getMetallicRoughnessTexture();
    void                setOcclusionTexture(shared_ptr<Texture> texture);
    shared_ptr<Texture> getOcclusionTexture();
    void                setEmissiveTexture(shared_ptr<Texture> texture);
    shared_ptr<Texture> getEmissiveTexture();
    bool                hasNoTexture();

    void          setAlbedoColor(vec4& albedoColor);
    void          getAlbedoColor(vec4& albedoColor);
    void          setMetallic(float metallic);
    float         getMetallic();
    void          setRoughness(float roughness);
    float         getRoughness();
    void          setEmissiveColor(vec3& emissiveColor);
    void          getEmissiveColor(vec3& emissiveColor);

    void          setTwoSided(bool twoSided);
    bool          getTwoSided();
    void          setLightingEnable(bool enable);
    bool          getLightingEnable();
    void          setBlendEnable(bool enable);
    bool          getBlendEnable();

    void          setDirty(bool dirty);
    bool          isDirty();
    void          setGraphicsData(void * graphicsData);
    void*         getGraphicsData();

  private:
    string        m_name;
    Type          m_materialType;

    vec4          m_albedoColor;
    vec3          m_emissiveColor;
    bool          m_twoSided;

    shared_ptr<Texture> m_albedoTexture;
    shared_ptr<Texture> m_normalTexture;
    shared_ptr<Texture> m_metallicRoughnessTexture;
    shared_ptr<Texture> m_occlusionTexture;
    shared_ptr<Texture> m_emissiveTexture;
    bool                m_noTexture;

    float         m_metallic;
    float         m_roughness;
    bool          m_lightingEnable;
    bool          m_blendEnable;
    bool          m_dirty;
    void*         m_graphicsData;
  };
}

