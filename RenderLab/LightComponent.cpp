#include "stdafx.h"
#include "LightComponent.h"

using std::make_shared;

namespace RenderLab
{
  LightComponent::LightComponent(string name, Type type, bool castShadow) : Component(name, Component::LIGHT),
    m_type(type),
    m_dirty(true),
    m_position(vec3()),
    m_direction(vec3(0.0f, 0.0f, -1.0f)),
    m_ambient(vec3(0.2f, 0.2f, 0.2f)),
    m_diffuse(vec3(0.8f, 0.8f, 0.8f)),
    m_specular(vec3(0.2f, 0.2f, 0.2f)),
    m_innerConeAngle(0.0f),
    m_outerConeAngle(90.0f),
    m_attenuation(vec3(1.0f, 0.0f, 0.0f)),
    m_castShadow(castShadow)
  {
    if (castShadow)
    {
      if (m_type == DIRECTIONAL)
      {
        m_shadowView = make_shared<RenderLab::View>(name + " shadow view", View::SHADOW);
        m_shadowView->setViewportSize(vec2(2048, 2048));
      }
      else if (m_type == POINT)
      {
        m_shadowView = make_shared<RenderLab::View>(name + " shadow cube view", View::SHADOW_CUBE);
        m_shadowView->setViewportSize(vec2(1024, 1024));
      }
    }
  }


  LightComponent::~LightComponent()
  {
  }

  LightComponent::Type LightComponent::getLightType()
  {
    return m_type;
  }

  void LightComponent::setDirty(bool dirty)
  {
    m_dirty = dirty;
  }

  bool LightComponent::isDirty()
  {
    return m_dirty;
  }

  void LightComponent::setPosition(vec3& position)
  {
    m_position = position;
  }

  void LightComponent::getPosition(vec3& position)
  {
    position = m_position;
  }

  void LightComponent::setDirection(vec3& direction)
  {
    m_direction = direction;
  }

  void LightComponent::getDirection(vec3& direction)
  {
    direction = m_direction;
  }

  void LightComponent::setAmbient(vec3& ambient)
  {
    m_ambient = ambient;
  }

  void LightComponent::getAmbient(vec3& ambient)
  {
    ambient = m_ambient;
  }

  void LightComponent::setDiffuse(vec3& diffuse)
  {
    m_diffuse = diffuse;
  }

  void LightComponent::getDiffuse(vec3& diffuse)
  {
    diffuse = m_diffuse;
  }

  void LightComponent::setSpecular(vec3& specular)
  {
    m_specular = specular;
  }

  void LightComponent::getSpecular(vec3& specular)
  {
    specular = m_specular;
  }

  void LightComponent::setInnerConeAngle(float angle)
  {
    m_innerConeAngle = angle;
  }

  float LightComponent::getInnerConeAngle()
  {
    return m_innerConeAngle;
  }

  void LightComponent::setOuterConeAngle(float angle)
  {
    m_outerConeAngle = angle;
  }

  float LightComponent::getOuterConeAngle()
  {
    return m_outerConeAngle;
  }

  void LightComponent::setAttenuation(vec3& attenuation)
  {
    m_attenuation = attenuation;
  }

  void LightComponent::getAttenuation(vec3& attenuation)
  {
    attenuation = m_attenuation;
  }

  void LightComponent::setCastShadow(bool castShadow)
  {
    m_castShadow = castShadow;
  }

  bool LightComponent::getCastShadow()
  {
    return m_castShadow;
  }

  shared_ptr<View> LightComponent::getShadowView()
  {
    return m_shadowView;
  }
}