#include "stdafx.h"
#include "Component.h"

namespace RenderLab
{
  Component::Component(string name, Type type) :
    m_name(name),
    m_type(type)
  {
  }


  Component::~Component()
  {
  }

  Component::Type Component::getType()
  {
    return m_type;
  }

  void Component::addEntity(shared_ptr<Entity> entity)
  {
    m_entities.push_back(entity);
  }

  void Component::removeEntity(shared_ptr<Entity> entity)
  {
    for (vector<shared_ptr<Entity>>::iterator it = m_entities.begin(); it != m_entities.end(); ++it)
    {
      if (*it == entity)
      {
        m_entities.erase(it);
        return;
      }
    }
  }

  shared_ptr<Entity> Component::getEntity(uint32_t index)
  {
    return m_entities[index];
  }
}