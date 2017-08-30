#pragma once
#include "Component.h"
#include "View.h"

#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using glm::vec3;
using std::string;

namespace RenderLab
{
  class LightComponent :
    public Component
  {
  public:
    enum Type
    {
      DIRECTIONAL,
      POINT,
      SPOT,
      AREA
    };

    LightComponent(string name, Type type, bool castShadow);
    ~LightComponent();

    void setDirty(bool dirty);
    bool isDirty();
    void setPosition(vec3& position);
    void getPosition(vec3& position);
    void setDirection(vec3& direction);
    void getDirection(vec3& direction);
    void setAmbient(vec3& ambient);
    void getAmbient(vec3& ambient);
    void setDiffuse(vec3& diffuse);
    void getDiffuse(vec3& diffuse);
    void setSpecular(vec3& specular);
    void getSpecular(vec3& specular);
    void setInnerConeAngle(float angle);
    float getInnerConeAngle();
    void setOuterConeAngle(float angle);
    float getOuterConeAngle();
    void setAttenuation(vec3& attenuation);
    void getAttenuation(vec3& attenuation);
    void setCastShadow(bool castShadow);
    bool getCastShadow();
    Type getLightType();
    shared_ptr<View> getShadowView();

  private:
    Type              m_type;
    bool              m_dirty;
    vec3              m_position;
    vec3              m_direction;
    vec3              m_ambient;
    vec3              m_diffuse;
    vec3              m_specular;
    float             m_innerConeAngle;
    float             m_outerConeAngle;
    vec3              m_attenuation;
    bool              m_castShadow;
    shared_ptr<View>  m_shadowView;
  };
}
