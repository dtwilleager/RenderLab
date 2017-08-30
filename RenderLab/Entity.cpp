#include "stdafx.h"
#include "Entity.h"
#include "LightComponent.h"

using std::static_pointer_cast;

namespace RenderLab
{
  Entity::Entity(string name) : 
    m_name(name),
    m_castShadow(true)
  {
  }

  Entity::~Entity()
  {
  }

  void Entity::setCastShadow(bool castShadow)
  {
    m_castShadow = castShadow;
    for (size_t i = 0; i < m_children.size(); ++i)
    {
      m_children[i]->setCastShadow(castShadow);
    }
  }

  bool Entity::getCastShadow()
  {
    return m_castShadow;
  }

  void Entity::addComponent(shared_ptr<Component> component)
  {
    m_components.push_back(component);
    component->addEntity(shared_from_this());
  }

  void Entity::removeComponent(shared_ptr<Component> component)
  {
    for (vector<shared_ptr<Component>>::iterator it = m_components.begin(); it != m_components.end(); ++it)
    {
      if (*it == component)
      {
        m_components.erase(it);
        return;
      }
    }
    component->removeEntity(shared_from_this());
  }

  size_t Entity::numComponents()
  {
    return m_components.size();
  }

  shared_ptr<Component> Entity::getComponent(unsigned int index)
  {
    return m_components[index];
  }


  void Entity::addChild(shared_ptr<Entity> entity)
  {
    m_children.push_back(entity);
  }

  void Entity::removeChild(shared_ptr<Entity> entity)
  {
    for (vector<shared_ptr<Entity>>::iterator it = m_children.begin(); it != m_children.end(); ++it)
    {
      if (*it == entity)
      {
        m_children.erase(it);
        return;
      }
    }
  }

  size_t Entity::numChildren()
  {
    return m_children.size();
  }

  shared_ptr<Entity> Entity::getChild(unsigned int index)
  {
    return m_children[index];
  }

  void Entity::setTransform(const mat4& transform)
  {
    m_transform = transform;
    for (size_t i = 0; i < m_components.size(); ++i)
    {
      if (m_components[i]->getType() == Component::LIGHT)
      {
        static_pointer_cast<LightComponent>(m_components[i])->setDirty(true);
      }
    }
  }

  void Entity::getTransform(mat4& transform)
  {
    transform = m_transform;
  }

  void Entity::getCompositeTransform(mat4& transform)
  {
    transform = m_compositeTransform;
  }

  void Entity::updateCompositeTransform(mat4& parent)
  {
    m_compositeTransform = parent * m_transform;
  }
}
