#include "stdafx.h"
#include "TranslationProcessor.h"

namespace RenderLab
{
  TranslationProcessor::TranslationProcessor(string name, float start, float end, float increment, vec3 axis, shared_ptr<Entity> entity) : ProcessorComponent(name, false, false),
    m_start(start),
    m_end(end),
    m_increment(increment),
    m_current(start),
    m_axis(axis),
    m_entity(entity)
  {
  }


  TranslationProcessor::~TranslationProcessor()
  {
  }

  void TranslationProcessor::execute(double absoluteTime, double deltaTime)
  {
    mat4 currentTransform;
    m_entity->getTransform(currentTransform);

    m_current += m_increment;
    if (m_current > m_end || m_current < m_start)
    {
      m_increment = -m_increment;
    }

    vec3 translation = m_increment*m_axis;
    mat4 transform = glm::translate(mat4(), translation);

    m_entity->setTransform(currentTransform*transform);
  }
}
