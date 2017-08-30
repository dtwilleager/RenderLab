#include "stdafx.h"
#include "RotationProcessor.h"

namespace RenderLab
{
  RotationProcessor::RotationProcessor(string name, shared_ptr<Entity> entity, vec3 axis, float revolutionsPerSecond): ProcessorComponent(name, false, false),
    m_target(entity),
    m_axis(axis),
    m_rotationSpeed(1.0f/revolutionsPerSecond)
  {

    m_rotationSpeed = 360.0/((1.0 / revolutionsPerSecond) * 1000000.0);
  }


  RotationProcessor::~RotationProcessor()
  {
  }

  void RotationProcessor::execute(double absoluteTime, double deltaTime)
  {
    mat4 currentTransform;
    m_target->getTransform(currentTransform);

    float angle = (float)(deltaTime * m_rotationSpeed);

    m_transform = glm::rotate(mat4(), angle, m_axis);

    m_target->setTransform(currentTransform*m_transform);
  }
}