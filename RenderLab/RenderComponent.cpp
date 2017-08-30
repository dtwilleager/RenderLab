#include "stdafx.h"
#include "RenderComponent.h"

namespace RenderLab
{
  RenderComponent::RenderComponent(string name) : Component(name, Component::RENDER)
  {
  }


  RenderComponent::~RenderComponent()
  {
  }

  void RenderComponent::addMesh(shared_ptr<Mesh> mesh)
  {
    m_meshes.push_back(mesh);
    mesh->setRenderComponent(shared_from_this());
  }

  shared_ptr<Mesh> RenderComponent::getMesh(size_t index)
  {
    return m_meshes[index];
  }

  size_t RenderComponent::numMeshes()
  {
    return m_meshes.size();
  }

  void RenderComponent::setVisible(bool visible)
  {
    m_visible = visible;
  }

  bool RenderComponent::IsVisible()
  {
    return m_visible;
  }

  void RenderComponent::build(shared_ptr<Graphics> graphics, vector<shared_ptr<UniformBuffer>>& frameDataUniformBuffers, vector<shared_ptr<UniformBuffer>>& objectDataUniformBuffers, size_t numFrames, vector<shared_ptr<LightComponent>>& lightComponents)
  {
    for (size_t i = 0; i < m_meshes.size(); i++)
    {
      graphics->build(m_meshes[i], frameDataUniformBuffers, objectDataUniformBuffers, numFrames, lightComponents);
    }   
  }
}
