#pragma once
#include "ProcessorComponent.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>

using std::string;
using glm::mat4;
using glm::vec3;
using glm::vec4;

namespace RenderLab
{
  class RotationProcessor :
    public ProcessorComponent
  {
  public:
    RotationProcessor(string name, shared_ptr<Entity> entity, vec3 axis, float revolutionsPerSecond);
    ~RotationProcessor();

    void execute(double absoluteTime, double deltaTime);

  private:
    shared_ptr<Entity>  m_target;
    vec3                m_axis;
    double              m_rotationSpeed;
    mat4                m_transform;
  };
}
