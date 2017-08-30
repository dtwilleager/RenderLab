#pragma once

#include "Component.h"

#include <string>
#include <vector>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

using glm::mat4;

using std::string;
using std::vector;
using std::shared_ptr;
using std::enable_shared_from_this;

class Component;

namespace RenderLab
{
  class Entity: public enable_shared_from_this<Entity>
  {
  public:
    Entity(string name);
    ~Entity();

    void                    setCastShadow(bool castShadow);
    bool                    getCastShadow();
    void                    addComponent(shared_ptr<Component> component);
    void                    removeComponent(shared_ptr<Component> component);
    size_t                  numComponents();
    shared_ptr<Component>   getComponent(unsigned int index);

    void                    addChild(shared_ptr<Entity> entity);
    void                    removeChild(shared_ptr<Entity> entity);
    size_t                  numChildren();
    shared_ptr<Entity>      getChild(unsigned int index);

    void                    setTransform(const mat4& transform);
    void                    getTransform(mat4& transform);
    void                    getCompositeTransform(mat4& transform);

    void                    updateCompositeTransform(mat4& parent);

  private:
    string                          m_name;
    bool                            m_castShadow;

    vector<shared_ptr<Component>>   m_components;
    vector<shared_ptr<Entity>>      m_children;

    mat4                            m_transform;
    mat4                            m_compositeTransform; 
  };
}

