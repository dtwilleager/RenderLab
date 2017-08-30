#pragma once

#include "Entity.h"

#include <string>
#include <vector>
#include <memory>

using std::string;
using std::vector;
using std::shared_ptr;


namespace RenderLab
{
  class Entity;

  class Component
  {
  public:
    friend class RenderLab::Entity;

    enum Type
    {
      RENDER = 0,
      LIGHT,
      PROCESS
    };

    Component(string name, Type type);
    ~Component();

    Type getType();
    shared_ptr<Entity> getEntity(uint32_t index);

  private:
    string  m_name;
    Type    m_type;

    vector<shared_ptr<Entity>>   m_entities;

    void addEntity(shared_ptr<Entity> entity);
    void removeEntity(shared_ptr<Entity> entity);
  };
}

