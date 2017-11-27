#include "stdafx.h"
#include "WorldManager.h"

#include "Mesh.h"
#include "Material.h"
#include "RenderComponent.h"
#include "GraphicsVulkan.h"
#include "GraphicsOpenGL.h"

using std::make_shared;
using std::static_pointer_cast;

namespace RenderLab {
  WorldManager::WorldManager(string name, HINSTANCE hinstance, HWND window) :
    m_name(name),
    m_constantDepthBias(3.0f),
    m_slopeDepthBias(0.0f),
    m_clusterEntityFreeze(false)
  {
    m_graphics = make_shared<GraphicsVulkan>("Vulkan Graphics", hinstance, window);
    //m_graphics = make_shared<GraphicsOpenGL>("OpenGL Graphics", hinstance, window);
    m_graphics->initialize(2);
    m_renderTechnique = make_shared<RenderTechnique>("Default Render Technique", this, hinstance, window, m_graphics);
    m_graphics->setRenderTechnique(m_renderTechnique);

    m_modelLoader = make_shared<ModelLoader>();

    m_timer.start();
  }

  WorldManager::~WorldManager()
  {
  }

  void WorldManager::buildFrame()
  {
    m_renderTechnique->build();
  }

  void WorldManager::executeFrame()
  {
    unsigned long long processTime = 0;
    unsigned long long renderTime = 0;
    unsigned long long currentTime = 0;
    m_lastFrameStartTime = m_frameStartTime;
    m_frameStartTime = m_timer.elapsedMicro();

    // Run all the processors
    for (size_t i = 0; i < m_processors.size(); i++)
    {
      m_processors[i]->execute((double)m_timer.elapsedMicro(), (double)(m_frameStartTime - m_lastFrameStartTime));
    }
    updateTransforms();
    currentTime = m_timer.elapsedMicro();
    processTime = currentTime - m_frameStartTime;

    m_graphics->setDepthBias(m_constantDepthBias, m_slopeDepthBias);

    m_renderTechnique->render();

    renderTime = m_timer.elapsedMicro() - currentTime;
    float gpuTime = m_graphics->getGPUFrameTime();
    float gpuTime2 = m_graphics->getGPUFrameTime2();
    printLog("ProcessTime: " + std::to_string((double)processTime/1000.0) + ", RenderTime: " + std::to_string((double)renderTime/1000.0) + ", GPUTime: " + std::to_string((double)gpuTime / 1000000.0) + ", " + std::to_string((double)gpuTime2 / 1000000.0));
  }

  void WorldManager::updateTransforms()
  {
    mat4 identity;
    for (size_t i = 0; i<m_entities.size(); i++)
    {
      updateTransform(m_entities[i], identity);
    }
  }

  void WorldManager::updateTransform(shared_ptr<Entity> entity, mat4& parent)
  {
    entity->updateCompositeTransform(parent);
    mat4 newParent;
    entity->getCompositeTransform(newParent);
    for (unsigned int i = 0; i<entity->numChildren(); i++)
    {
      updateTransform(entity->getChild(i), newParent);
    }
  }

  void WorldManager::updateWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
  {
    m_renderTechnique->updateWindow(x, y, width, height);
  }

  void WorldManager::addEntity(shared_ptr<Entity> entity)
  {
    m_entities.push_back(entity);
    processAddEntity(entity);
  }

  void WorldManager::removeEntity(shared_ptr<Entity> entity)
  {
    for (vector<shared_ptr<Entity>>::iterator it = m_entities.begin(); it != m_entities.end(); ++it)
    {
      if (*it == entity)
      {
        m_entities.erase(it);
        return;
      }
    }
    processRemoveEntity(entity);
  }

  shared_ptr<Entity> WorldManager::getEntity(unsigned int index)
  {
    return m_entities[index];
  }

  size_t WorldManager::numEntities()
  {
    return m_entities.size();
  }

  void WorldManager::processAddEntity(shared_ptr<Entity> entity)
  {
    for (unsigned int i = 0; i<entity->numComponents(); i++)
    {
      shared_ptr<Component> component = entity->getComponent(i);
      switch (component->getType())
      {
      case Component::PROCESS:
        addProcessorComponent(static_pointer_cast<ProcessorComponent>(component));
        break;
      case Component::LIGHT:
        addLightComponent(static_pointer_cast<LightComponent>(component), entity);
        break;
      case Component::RENDER:
        addRenderComponent(static_pointer_cast<RenderComponent>(component), entity);
        break;
      }
    }

    for (unsigned int i = 0; i<entity->numChildren(); i++)
    {
      processAddEntity(entity->getChild(i));
    }
  }

  void WorldManager::processRemoveEntity(shared_ptr<Entity> entity)
  {
    for (unsigned int i = 0; i<entity->numComponents(); i++)
    {
      shared_ptr<Component> component = entity->getComponent(i);
      switch (component->getType())
      {
      case Component::PROCESS:
        removeProcessorComponent(static_pointer_cast<ProcessorComponent>(component));
        break;
      case Component::LIGHT:
        removeLightComponent(static_pointer_cast<LightComponent>(component), entity);
        break;
      case Component::RENDER:
        removeRenderComponent(static_pointer_cast<RenderComponent>(component), entity);
        break;
      }
    }

    for (unsigned int i = 0; i<entity->numChildren(); i++)
    {
      processRemoveEntity(entity->getChild(i));
    }
  }

  void WorldManager::addProcessorComponent(shared_ptr<ProcessorComponent> processorComponent)
  {
    m_processors.push_back(processorComponent);
  }

  void WorldManager::addLightComponent(shared_ptr<LightComponent> lightComponent, shared_ptr<Entity> entity)
  {
    m_renderTechnique->addLightComponent(lightComponent, entity);
  }

  void WorldManager::addRenderComponent(shared_ptr<RenderComponent> renderComponent, shared_ptr<Entity> entity)
  {
    m_renderTechnique->addRenderComponent(renderComponent, entity);
  }

  void WorldManager::removeProcessorComponent(shared_ptr<ProcessorComponent> processorComponent)
  {
    for (vector<shared_ptr<ProcessorComponent>>::iterator it = m_processors.begin(); it != m_processors.end(); ++it)
    {
      if (*it == processorComponent)
      {
        m_processors.erase(it);
        return;
      }
    }
  }

  void WorldManager::removeLightComponent(shared_ptr<LightComponent> lightComponent, shared_ptr<Entity> entity)
  {
    m_renderTechnique->removeLightComponent(lightComponent, entity);
  }

  void WorldManager::removeRenderComponent(shared_ptr<RenderComponent> renderComponent, shared_ptr<Entity> entity)
  {
    m_renderTechnique->removeRenderComponent(renderComponent, entity);
  }

  void WorldManager::addView(shared_ptr<View> view)
  {
    m_views.push_back(view);
    m_renderTechnique->addView(view);
  }

  void WorldManager::removeView(shared_ptr<View> view)
  {
    m_renderTechnique->removeView(view);
    for (vector<shared_ptr<View>>::iterator it = m_views.begin(); it != m_views.end(); ++it)
    {
      if (*it == view)
      {
        m_views.erase(it);
        return;
      }
    }
  }

  shared_ptr<View> WorldManager::getView(unsigned int index)
  {
    return m_views[index];
  }

  size_t WorldManager::numViews()
  {
    return m_views.size();
  }

  void WorldManager::handleKeyboard(MSG* event)
  {
    if (event->message == WM_KEYDOWN)
    {
      WPARAM keyCode = event->wParam;
      switch (keyCode)
      {
      case VK_ADD:
        m_constantDepthBias += 1.0f;
        printLog("Bias " + std::to_string(m_constantDepthBias));
        break;
      case VK_SUBTRACT:
        m_constantDepthBias -= 1.0f;
        printLog("Bias " + std::to_string(m_constantDepthBias));
        break;
      case VK_F1:
        m_clusterEntityFreeze = !m_clusterEntityFreeze;
        m_renderTechnique->setClusterEntityFreeze(m_clusterEntityFreeze);
        break;
      }
    }

    for (size_t i = 0; i < m_processors.size(); i++)
    {
      if (m_processors[i]->sendKeyboardEvents())
      {
        m_processors[i]->handleKeyboard(event);
      }
    }
  }

  void WorldManager::handleMouse(MSG* event)
  {
    for (size_t i = 0; i < m_processors.size(); i++)
    {
      if (m_processors[i]->sendMouseEvents())
      {
        m_processors[i]->handleMouse(event);
      }
    }
  }

  shared_ptr<Entity> WorldManager::loadAssimpModel(string filename)
  {
    return m_modelLoader->loadAssimpModel(filename);
  }

  shared_ptr<Entity>  WorldManager::loadGLTFModel(string filename)
  {
    return m_modelLoader->loadGLTFModel(filename);
  }

  void WorldManager::printLog(string s)
  {
    string st = s + "\n";
    TCHAR name[256];
    _tcscpy_s(name, CA2T(st.c_str()));
    OutputDebugString(name);
  }
}