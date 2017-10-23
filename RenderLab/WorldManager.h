#pragma once

#include "Entity.h"
#include "Mesh.h"
#include "Material.h"
#include "Texture.h"
#include "View.h"
#include "ProcessorComponent.h"
#include "RenderComponent.h"
#include "RenderTechnique.h"
#include "LightComponent.h"
#include "Graphics.h"
#include "CpuTimer.h"
#include "ModelLoader.h"

#include <string>
#include <vector>
#include <memory>
#include <map>

#include <assimp/Importer.hpp>
#include <assimp/scene.h> 
#include <assimp/postprocess.h>
#include <assimp/mesh.h>

using std::string;
using std::vector;
using std::shared_ptr;
using std::map;

namespace RenderLab
{
  class WorldManager
  {
  public:
    WorldManager(string name, HINSTANCE hinstance, HWND window);
    ~WorldManager();

    void                addEntity(shared_ptr<Entity> entity);
    void                removeEntity(shared_ptr<Entity> entity);
    shared_ptr<Entity>  getEntity(unsigned int index);
    size_t              numEntities();

    void                addView(shared_ptr<View> view);
    void                removeView(shared_ptr<View> view);
    shared_ptr<View>    getView(unsigned int index);
    size_t              numViews();

    void                handleKeyboard(MSG* event);
    void                handleMouse(MSG* event);

    shared_ptr<Entity>  loadAssimpModel(string filename);
    shared_ptr<Entity>  loadGLTFModel(string filename);

    void                buildFrame();
    void                executeFrame();


    void                updateWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

  private:
    string      m_name;

    vector<shared_ptr<Entity>>                m_entities;
    vector<shared_ptr<View>>                  m_views;
    vector<shared_ptr<ProcessorComponent>>    m_processors;
    shared_ptr<Graphics>                      m_graphics;
    shared_ptr<RenderTechnique>               m_renderTechnique;
    shared_ptr<ModelLoader>                   m_modelLoader;
    CpuTimer                                  m_timer;
    unsigned long long                        m_frameStartTime;
    unsigned long long                        m_lastFrameStartTime;
    unsigned long long                        m_totalTime;
    float                                     m_constantDepthBias;
    float                                     m_slopeDepthBias;
    bool                                      m_clusterEntityFreeze;

    void processAddEntity(shared_ptr<Entity> entity);
    void processRemoveEntity(shared_ptr<Entity> entity);
    void addProcessorComponent(shared_ptr<ProcessorComponent> processorComponent);
    void addLightComponent(shared_ptr<LightComponent> lightComponent, shared_ptr<Entity> entity);
    void addRenderComponent(shared_ptr<RenderComponent> renderComponent, shared_ptr<Entity> entity);
    void removeProcessorComponent(shared_ptr<ProcessorComponent> processorComponent);
    void removeLightComponent(shared_ptr<LightComponent> lightComponent, shared_ptr<Entity> entity);
    void removeRenderComponent(shared_ptr<RenderComponent> renderComponent, shared_ptr<Entity> entity);

    void printLog(string s);

    void updateTransforms();
    void updateTransform(shared_ptr<Entity> entity, mat4& parent);
  };
}

