#pragma once
#include "ProcessorComponent.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>

#include <atlstr.h>

using std::string;
using glm::mat4;
using glm::vec3;
using glm::vec4;

namespace RenderLab
{
  class TranslationProcessor :
    public ProcessorComponent
  {
  public:
    TranslationProcessor(string name, float start, float end, float increment, vec3 axis, shared_ptr<Entity> entity);
    ~TranslationProcessor();

    void  execute(double absoluteTime, double deltaTime);

  private:
    float m_start;
    float m_end;
    float m_increment;
    float m_current;
    vec3  m_axis;
    shared_ptr<Entity> m_entity;
  };
}

