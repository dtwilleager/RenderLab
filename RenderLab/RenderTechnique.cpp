#include "stdafx.h"
#include "RenderTechnique.h"

namespace RenderLab
{
  RenderTechnique::RenderTechnique(string name, HINSTANCE hinstance, HWND window, shared_ptr<Graphics> graphics):
    m_name(name),
    m_hinstance(hinstance),
    m_window(window),
    m_graphics(graphics),
    m_currentLight(0),
    m_depthPrepass(false),
    m_clusterData(nullptr),
    m_freezeClusterEntity(false)
  {
  }


  RenderTechnique::~RenderTechnique()
  {
  }

  void RenderTechnique::addRenderComponent(shared_ptr<RenderComponent> renderComponent, shared_ptr<Entity> entity)
  {
    m_renderComponents.push_back(renderComponent);
    m_renderEntities.push_back(entity);
  }

  void RenderTechnique::removeRenderComponent(shared_ptr<RenderComponent> renderComponent, shared_ptr<Entity> entity)
  {
    for (vector<shared_ptr<RenderComponent>>::iterator it = m_renderComponents.begin(); it != m_renderComponents.end(); ++it)
    {
      if (*it == renderComponent)
      {
        m_renderComponents.erase(it);
        return;
      }
    }

    for (vector<shared_ptr<Entity>>::iterator it = m_renderEntities.begin(); it != m_renderEntities.end(); ++it)
    {
      if (*it == entity)
      {
        m_renderEntities.erase(it);
        return;
      }
    }
  }

  void RenderTechnique::addLightComponent(shared_ptr<LightComponent> lightComponent, shared_ptr<Entity> entity)
  {
    m_lightComponents.push_back(lightComponent);
    m_lightEntities.push_back(entity);
  }

  void RenderTechnique::removeLightComponent(shared_ptr<LightComponent> lightComponent, shared_ptr<Entity> entity)
  {
    for (vector<shared_ptr<LightComponent>>::iterator it = m_lightComponents.begin(); it != m_lightComponents.end(); ++it)
    {
      if (*it == lightComponent)
      {
        m_lightComponents.erase(it);
        return;
      }
    }

    for (vector<shared_ptr<Entity>>::iterator it = m_lightEntities.begin(); it != m_lightEntities.end(); ++it)
    {
      if (*it == entity)
      {
        m_lightEntities.erase(it);
        return;
      }
    }
  }

  size_t RenderTechnique::getNumLightComponents()
  {
    return m_lightComponents.size();
  }

  shared_ptr<LightComponent> RenderTechnique::getLightComponent(uint32_t index)
  {
    return m_lightComponents[index];
  }

  void RenderTechnique::addView(shared_ptr<View> view)
  {
    m_views.push_back(view);
    if (view->getType() == View::SCREEN)
    {
      m_onscreenView = view;
      m_graphics->setOnscreenView(view);
    }
  }

  void RenderTechnique::removeView(shared_ptr<View> view)
  {
    for (vector<shared_ptr<View>>::iterator it = m_views.begin(); it != m_views.end(); ++it)
    {
      if (*it == view)
      {
        m_views.erase(it);
        return;
      }
    }
  }

  void RenderTechnique::updateWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
  {
    vec2 position(x, y);
    vec2 size(width, height);
    m_onscreenView->setViewportPosition(position);
    m_onscreenView->setViewportSize(size);
    m_graphics->resize(m_onscreenView, width, height);
  }

  void RenderTechnique::buildFrustumLines(shared_ptr<View> view)
  {
    m_clusterEntity = make_shared<Entity>("Frustum Lines");
    shared_ptr<RenderComponent> renderComponent = make_shared<RenderComponent>("Frustum Lines");
    vec2 viewportSize;
    view->getViewportSize(viewportSize);

    // Determine the number of X and Y tiles
    uint32_t segmentWidth = 64;
    uint32_t segmentHeight = 64;
    uint32_t numXSegments = (uint32_t)viewportSize.x / segmentWidth;
    uint32_t numYSegments = (uint32_t)viewportSize.y / segmentHeight;
    uint32_t numZSegments = 10;

    if ((uint32_t)viewportSize.x % segmentWidth != 0)
    {
      numXSegments++;
    }
    if ((uint32_t)viewportSize.y % segmentHeight != 0)
    {
      numYSegments++;
    }
    uint32_t numVerts = (numXSegments + 1) * (numYSegments + 1) * numZSegments;
    uint32_t numClusters = numXSegments * numYSegments * (numZSegments - 1);

    shared_ptr<Mesh> mesh = make_shared<Mesh>("Frustum Lines", Mesh::LINES, numVerts, 2);

    float* vertexBuffer = (float*)malloc(numVerts * 3 * sizeof(float));
    float* normalBuffer = (float*)malloc(numVerts * 3 * sizeof(float));

    uint32_t* indexBuffer = (uint32_t*)malloc(numClusters * 12 * 2 * sizeof(uint32_t));

    m_clusterData = (ClusterData*)malloc(sizeof(ClusterData));
    m_clusterData->m_clusters = (Cluster*)malloc(numClusters*sizeof(Cluster));
    m_clusterData->m_numClusterVerts = numVerts;
    m_clusterData->m_localVerts = vertexBuffer;
    m_clusterData->m_clusterVerts = (vec3*)malloc(numVerts*sizeof(vec3));

    // Get the eye and direction in world space
    mat4 viewTransform;
    view->getViewTransform(viewTransform);
    mat4 invViewTransform = glm::inverse(viewTransform);

    vec3 worldDirectionX = vec3(1.0f, 0.0f, 0.0f);
    vec3 worldDirectionY = vec3(0.0f, 1.0f, 0.0f);
    vec3 worldDirectionZ = vec3(0.0f, 0.0f, 1.0f);
    vec3 worldEye = vec3();

    //vec3 worldDirectionX = vec3(invViewTransform[0]);
    //vec3 worldDirectionY = vec3(invViewTransform[1]);
    //vec3 worldDirectionZ = vec3(invViewTransform[2]);
    //vec3 worldEye = vec3(invViewTransform[3]);

    float hFov = view->getFieldOfView();
    float nearClip = view->getNearClip();
    float farClip = view->getFarClip();

    vec3 nearZ = worldEye + -worldDirectionZ * nearClip;
    vec3 farZ = worldEye + -worldDirectionZ * farClip;

    float zInc = (farClip - nearClip) / numZSegments;
    
    uint32_t vindex = 0;
    uint32_t nindex = 0;

    vec3 currentZ = nearZ;
    float currentZD = nearClip;

    for (uint32_t k = 0; k < numZSegments; k++)
    {
      float farD = tan(hFov*0.5f * (float)M_PI / 180.0f) * currentZD;
      float currentYD = farD;
      float xInc = farD * 2.0f / numXSegments;
      float yInc = -farD * 2.0f / numYSegments;

      for (uint32_t j = 0; j < numYSegments + 1; j++)
      {
        float currentXD = -farD;
        for (uint32_t i = 0; i < numXSegments + 1; i++)
        {
          vec3 currentPoint = currentZ + worldDirectionX * currentXD + worldDirectionY * currentYD;
          vertexBuffer[vindex++] = currentPoint.x;
          vertexBuffer[vindex++] = currentPoint.y;
          vertexBuffer[vindex++] = currentPoint.z;
          normalBuffer[nindex++] = 0.0f;
          normalBuffer[nindex++] = 0.0f;
          normalBuffer[nindex++] = 1.0f;
          currentXD += xInc;
        }
        currentYD += yInc;
      }
      currentZD += zInc;
      currentZ = worldEye + -worldDirectionZ * currentZD;
    }

    uint32_t iindex = 0;
    uint32_t bvindex = 0;
    for (uint32_t k = 0; k < numZSegments-1; k++)
    {
      vindex = k * (numYSegments + 1) * (numXSegments + 1);
      bvindex = (k+1) * (numYSegments + 1) * (numXSegments + 1);
      for (uint32_t j = 0; j < numYSegments; j++)
      {    
        for (uint32_t i = 0; i < numXSegments; i++)
        {
          indexBuffer[iindex++] = vindex;
          indexBuffer[iindex++] = vindex+1;
          indexBuffer[iindex++] = vindex + 1;
          indexBuffer[iindex++] = vindex + 1 + numXSegments + 1;
          indexBuffer[iindex++] = vindex + 1 + numXSegments + 1;
          indexBuffer[iindex++] = vindex + numXSegments + 1;
          indexBuffer[iindex++] = vindex + numXSegments + 1;
          indexBuffer[iindex++] = vindex;

          indexBuffer[iindex++] = vindex;
          indexBuffer[iindex++] = bvindex;
          indexBuffer[iindex++] = vindex + 1;
          indexBuffer[iindex++] = bvindex + 1;
          indexBuffer[iindex++] = vindex + 1 + numXSegments + 1;
          indexBuffer[iindex++] = bvindex + 1 + numXSegments + 1;
          indexBuffer[iindex++] = vindex + numXSegments + 1;
          indexBuffer[iindex++] = bvindex + numXSegments + 1;

          indexBuffer[iindex++] = bvindex;
          indexBuffer[iindex++] = bvindex + 1;
          indexBuffer[iindex++] = bvindex + 1;
          indexBuffer[iindex++] = bvindex + 1 + numXSegments + 1;
          indexBuffer[iindex++] = bvindex + 1 + numXSegments + 1;
          indexBuffer[iindex++] = bvindex + numXSegments + 1;
          indexBuffer[iindex++] = bvindex + numXSegments + 1;
          indexBuffer[iindex++] = bvindex;

          m_clusterData->m_clusters->m_verts[0] = vindex;
          m_clusterData->m_clusters->m_verts[1] = vindex + 1;
          m_clusterData->m_clusters->m_verts[2] = vindex + 1 + numXSegments + 1;
          m_clusterData->m_clusters->m_verts[3] = vindex + numXSegments + 1;
          m_clusterData->m_clusters->m_verts[4] = bvindex;
          m_clusterData->m_clusters->m_verts[5] = bvindex + 1;
          m_clusterData->m_clusters->m_verts[6] = bvindex + 1 + numXSegments + 1;
          m_clusterData->m_clusters->m_verts[7] = bvindex + numXSegments + 1;

          vindex++;
          bvindex++;
        }
        vindex++;
        bvindex++;
      }
    }

    mesh->addVertexBuffer(0, 3, numVerts * 3 * sizeof(float), vertexBuffer);
    mesh->addVertexBuffer(1, 3, numVerts * 3 * sizeof(float), normalBuffer);
    mesh->addIndexBuffer(numClusters * 12 * 2, indexBuffer);

    shared_ptr<Material>material = make_shared<Material>("Frustum Lines", Material::DEFERRED_LIT);
    material->setAlbedoColor(vec4(1.0f, 1.0f, 1.0f, 1.0f));
    mesh->setMaterial(material);

    free(normalBuffer);
    free(indexBuffer);
    renderComponent->addMesh(mesh);
    m_clusterEntity->addComponent(renderComponent);
    m_clusterEntity->setTransform(invViewTransform);
    m_clusterEntity->updateCompositeTransform(mat4());
    addRenderComponent(renderComponent, m_clusterEntity);
  }

  void RenderTechnique::setClusterEntityFreeze(bool freeze)
  {
    m_freezeClusterEntity = freeze;
  }

  void RenderTechnique::build()
  {
    size_t numFrames = 2;
    size_t currentOffset = 0;
    size_t currentMeshIndex = 0;
    size_t alignment = m_graphics->getBufferAlignment();
    shared_ptr<UniformBuffer> uniformBuffer = nullptr;

    for (size_t i = 0; i < m_views.size(); ++i)
    {
      m_graphics->build(m_views[i], numFrames);
      if (m_views[i]->getType() == View::SCREEN)
      {
        buildFrustumLines(m_views[i]);
      }
    }

    //for (size_t i = 0; i < m_lightComponents.size(); ++i)
    //{
    //  m_graphics->build(m_lightComponents[i]->getShadowView(), numFrames);
    //}

    m_frameDataAlignedSize = sizeof(FrameShaderParamBlock);
    if (m_frameDataAlignedSize % alignment)
    {
      m_frameDataAlignedSize += alignment - (m_frameDataAlignedSize % alignment);
    }
    for (size_t i = 0; i < numFrames; ++i)
    {
      uniformBuffer = make_shared<UniformBuffer>("Frame Data UniformBuffer " + std::to_string(i), m_frameDataAlignedSize);
      m_graphics->build(uniformBuffer);
      m_frameDataUniformBuffers.push_back(uniformBuffer);
    }
   
    m_numMeshes = 0;
    for (size_t i = 0; i < m_renderComponents.size(); ++i)
    {
      m_numMeshes += m_renderComponents[i]->numMeshes();
    }

    m_objectDataAlignedSize = sizeof(ObjectShaderParamBlock);
    if (m_objectDataAlignedSize % alignment)
    {
      m_objectDataAlignedSize += alignment - (m_objectDataAlignedSize % alignment);
    }

    for (size_t i = 0; i < numFrames; i++)
    {
      uniformBuffer = make_shared<UniformBuffer>("Object Data UniformBuffer " + std::to_string(i), m_objectDataAlignedSize * m_numMeshes);
      m_graphics->build(uniformBuffer);
      m_objectDataUniformBuffers.push_back(uniformBuffer);
    }   

    m_meshOffsets = new uint32_t[m_numMeshes];
    for (size_t i = 0; i < m_renderComponents.size(); i++)
    {
      m_renderComponents[i]->build(m_graphics, m_frameDataUniformBuffers, m_objectDataUniformBuffers, numFrames, m_lightComponents);
      for (size_t j = 0; j < m_renderComponents[i]->numMeshes(); j++, currentMeshIndex++)
      {
        m_meshOffsets[currentMeshIndex] = (uint32_t)currentOffset;
        currentOffset += m_objectDataAlignedSize;
      }
    }

    createCompositeMeshes();
  }

  void RenderTechnique::createCompositeMeshes()
  {
    shared_ptr<Mesh> mesh = make_shared<Mesh>("Composite Mesh", Mesh::TRIANGLES, 24, 1);
    shared_ptr<Material> material = make_shared<Material>("Composite Material", Material::DEFERRED_COMPOSITE);
    mesh->setMaterial(material);

    //float verts[12] = { 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    //mesh->addVertexBuffer(0, 3, sizeof(float)*4 * 3, verts);

    //float texCoords[8] = { 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    //mesh->addVertexBuffer(1, 2, sizeof(float)*4 * 2, texCoords);

    //unsigned int indexBuffer[6] = { 0,2,1, 2,0,3 };
    //mesh->addIndexBuffer(2 * 3, indexBuffer);
    float verts[24];
    verts[0] =  -0.5f; verts[1] =   0.5f; verts[2] =  -0.5f;
    verts[3] =   0.5f; verts[4] =   0.5f; verts[5] =  -0.5f;
    verts[6] =   0.5f; verts[7] =  -0.5f; verts[8] =  -0.5f;
    verts[9] =  -0.5f; verts[10] = -0.5f; verts[11] = -0.5f;

    verts[12] = -0.5f; verts[13] =  0.5f; verts[14] =  0.5f;
    verts[15] =  0.5f; verts[16] =  0.5f; verts[17] =  0.5f;
    verts[18] =  0.5f; verts[19] = -0.5f; verts[20] =  0.5f;
    verts[21] = -0.5f; verts[22] = -0.5f; verts[23] =  0.5f;

    mesh->addVertexBuffer(0, 3, sizeof(float) * 24 * 3, verts);

    unsigned int ib[36];
    ib[0] = 0; ib[1] = 1; ib[2] = 2;
    ib[3] = 0; ib[4] = 2; ib[5] = 3;

    ib[6] = 1; ib[7] = 5; ib[8] = 6;
    ib[9] = 1; ib[10] = 6; ib[11] = 2;

    ib[12] = 5; ib[13] = 4; ib[14] = 6;
    ib[15] = 4; ib[16] = 7; ib[17] = 6;

    ib[18] = 4; ib[19] = 0; ib[20] = 3;
    ib[21] = 4; ib[22] = 3; ib[23] = 7;

    ib[24] = 4; ib[25] = 5; ib[26] = 1;
    ib[27] = 4; ib[28] = 1; ib[29] = 0;

    ib[30] = 3; ib[31] = 2; ib[32] = 6;
    ib[33] = 3; ib[34] = 6; ib[35] = 7;
    mesh->addIndexBuffer(12 * 3, ib);

    m_graphics->build(mesh, m_frameDataUniformBuffers, m_objectDataUniformBuffers, 2, m_lightComponents);

    m_onscreenView->addCompositeMesh(mesh);
  }

  void RenderTechnique::render()
  {
    uint32_t frameIndex = m_graphics->acquireBackBuffer(m_onscreenView);
    shared_ptr<View> lastView = m_onscreenView;

    //updateFrameData(frameIndex);
    updateClusterData(m_onscreenView, frameIndex);
    updateMeshData(m_onscreenView, frameIndex);
    bool lastLight = false;

    // Render the shadow maps
    for (size_t i = 0; i < m_lightComponents.size(); ++i)
    {
      updateCurrentLight(frameIndex, (int)i);
      if (i == m_lightComponents.size() - 1)
      {
        lastLight = true;
      }

      if (m_lightComponents[i]->getCastShadow() && m_lightComponents[i]->isDirty())
      {
        shared_ptr<View> view = m_lightComponents[i]->getShadowView();
        //updateCurrentLight(frameIndex, (int)i);
        //updateMeshData(view, frameIndex);
        m_graphics->acquireBackBuffer(view);
        m_graphics->renderBegin(view, lastView, m_frameDataUniformBuffers[frameIndex], m_objectDataUniformBuffers[frameIndex], frameIndex);
        renderMeshes(view, frameIndex, true, false);
        m_graphics->renderEnd(view, lastView, lastLight, m_frameDataUniformBuffers[frameIndex], m_objectDataUniformBuffers[frameIndex], frameIndex);
        m_graphics->swapBackBuffer(view, frameIndex);
        lastView = view;
        m_lightComponents[i]->setDirty(false);
      }
    }

    //m_graphics->acquireBackBuffer(lastView, lastView);
    // Render the meshes
    //updateMeshData(m_onscreenView, frameIndex);
    m_graphics->renderBegin(m_onscreenView, lastView, m_frameDataUniformBuffers[frameIndex], m_objectDataUniformBuffers[frameIndex], frameIndex);
    if (m_depthPrepass)
    { 
      renderMeshes(m_onscreenView, frameIndex, false, true);
      m_graphics->endDepthPrepass(lastView, frameIndex);
    }
    renderMeshes(m_onscreenView, frameIndex, false, false);
    m_graphics->renderEnd(m_onscreenView, lastView, false, m_frameDataUniformBuffers[frameIndex], m_objectDataUniformBuffers[frameIndex], frameIndex);

    // Swap the buffers
    m_graphics->swapBackBuffer(m_onscreenView, frameIndex);
  }

  void RenderTechnique::updateCurrentLight(uint32_t frameIndex, int lightIndex)
  {
    uint32_t offset = 0;
    uint32_t size = 0;
    float* data = nullptr;

    ivec4 lightInfo;
    lightInfo.x = lightIndex;
    lightInfo.y = (int)m_lightComponents.size();
    data = (float*)glm::value_ptr(lightInfo);
    size = sizeof(lightInfo);
    m_graphics->updateUniformData(m_frameDataUniformBuffers[frameIndex], offset, data, size);
  }

  void RenderTechnique::updateClusterData(shared_ptr<View> view, uint32_t frameIndex)
  {
    mat4 viewTransform;
    mat4 invViewTransform;

    view->getViewTransform(viewTransform);
    invViewTransform = glm::inverse(viewTransform);

    uint32_t vindex = 0;
    for (uint32_t i = 0; i < m_clusterData->m_numClusterVerts; i++)
    {
      vec4 localPoint(0.0f, 0.0f, 0.0f, 1.0f);
      localPoint.x = m_clusterData->m_localVerts[vindex++];
      localPoint.y = m_clusterData->m_localVerts[vindex++];
      localPoint.z = m_clusterData->m_localVerts[vindex++];
      m_clusterData->m_clusterVerts[i] = vec3(invViewTransform * localPoint);
    }

    if (!m_freezeClusterEntity)
    {
      m_clusterEntity->setTransform(invViewTransform);
      m_clusterEntity->updateCompositeTransform(mat4());
    }
  }

  void RenderTechnique::updateFrameData(uint32_t frameIndex)
  {
    mat4 transform;
    mat4 projectionTransform;
    mat4 viewProjection;
    uint32_t offset = 0;
    uint32_t size = 0;
    float* data = nullptr;

    mat4 viewMatrix;
    mat4 lightProjection = glm::perspective(90.0f, 1.0f, 0.1f, 1000.0f);

    vec3 yAxis(0.0f, -1.0f, 0.0f);
    vec3 zAxis(0.0f, 0.0f, -1.0f);
    vec3 center;

    Light lightData;

    vec4 viewPosition(0.0f, 0.0f, 0.0f, 1.0f);
    m_onscreenView->getViewTransform(viewMatrix);
    viewPosition = glm::inverse(viewMatrix)*viewPosition;

    data = glm::value_ptr(viewPosition);
    size = sizeof(viewPosition);
    offset = sizeof(ivec4);
    m_graphics->updateUniformData(m_frameDataUniformBuffers[frameIndex], offset, data, size);

    offset = 2*sizeof(ivec4);
    for (size_t i = 0; i < m_lightComponents.size(); ++i)
    {
      vec3 position;
      m_lightComponents[i]->getPosition(position);
      m_lightComponents[i]->getEntity(0)->getCompositeTransform(transform);
      vec3 lightWorldPosition = vec3(transform * vec4(position, 1.0f));

      center = lightWorldPosition + vec3(1.0f, 0.0f, 0.0f);
      lightData.light_view_projections[0] = lightProjection * glm::lookAt(lightWorldPosition, center, vec3(0.0, -1.0, 0.0));
      center = lightWorldPosition + vec3(-1.0f, 0.0f, 0.0f);
      lightData.light_view_projections[1] = lightProjection * glm::lookAt(lightWorldPosition, center, vec3(0.0, -1.0, 0.0));
      center = lightWorldPosition + vec3(0.0f, 1.0f, 0.0f);
      lightData.light_view_projections[2] = lightProjection * glm::lookAt(lightWorldPosition, center, vec3(0.0, 0.0, 1.0));
      center = lightWorldPosition + vec3(0.0f, -1.0f, 0.0f);
      lightData.light_view_projections[3] = lightProjection * glm::lookAt(lightWorldPosition, center, glm::vec3(0.0, 0.0, -1.0));
      center = lightWorldPosition + vec3(0.0f, 0.0f, 1.0f);
      lightData.light_view_projections[4] = lightProjection * glm::lookAt(lightWorldPosition, center, vec3(0.0, -1.0, 0.0));
      center = lightWorldPosition + vec3(0.0f, 0.0f, -1.0f);
      lightData.light_view_projections[5] = lightProjection * glm::lookAt(lightWorldPosition, center, vec3(0.0, -1.0, 0.0));

      lightData.light_position = transform * vec4(position, 1.0f);

      vec3 color;
      m_lightComponents[i]->getDiffuse(color);
      lightData.light_color = vec4(color.r, color.g, color.b, 1.0f);

      size = sizeof(lightData);
      m_graphics->updateUniformData(m_frameDataUniformBuffers[frameIndex], offset, (uint8_t*)&lightData, size);
      offset += size;
    }
  }

  void RenderTechnique::renderMeshes(shared_ptr<View> view, uint32_t frameIndex, bool shadowPass, bool depthPrepass)
  {
    uint32_t currentMeshIndex = 0;
    for (size_t i = 0; i < m_renderComponents.size(); i++)
    {
      if (shadowPass && m_renderComponents[i]->getEntity(0)->getCastShadow() == false)
      {
        continue;
      }

      for (size_t j = 0; j < m_renderComponents[i]->numMeshes(); j++, currentMeshIndex++)
      {
        shared_ptr<Mesh> mesh = m_renderComponents[i]->getMesh(j);
        m_graphics->bindPipeline(mesh, view, frameIndex, depthPrepass);
        m_graphics->render(mesh, m_meshOffsets[currentMeshIndex], view, frameIndex, depthPrepass);
      }
    }
  }

  void RenderTechnique::updateMeshData(shared_ptr<View> view, uint32_t frameIndex)
  {
    uint32_t currentMeshIndex = 0;
    for (size_t i = 0; i < m_renderComponents.size(); i++)
    {
      for (size_t j = 0; j < m_renderComponents[i]->numMeshes(); j++, currentMeshIndex++)
      {
        shared_ptr<Mesh> mesh = m_renderComponents[i]->getMesh(j);
        updateMeshData(view, mesh, m_renderComponents[i]->getEntity(0), frameIndex, currentMeshIndex);
      }
    }
  }

  void RenderTechnique::updateMeshData(shared_ptr<View> view, shared_ptr<Mesh> mesh, shared_ptr<Entity> entity, uint32_t frameIndex, uint32_t meshIndex)
  {
    uint32_t offset = m_meshOffsets[meshIndex];
    ObjectShaderParamBlock objectData;
    mat4 viewTransform;
    mat4 projectionTransform;
    size_t size = 0;
    vec3 color;

    view->getViewTransform(viewTransform);
    view->getProjectionTransform(projectionTransform);
    entity->getCompositeTransform(objectData.model);
    objectData.view_projection = projectionTransform * viewTransform;

    shared_ptr<Material> material = mesh->getMaterial();
    material->getAlbedoColor(objectData.albedoColor);
    material->getEmissiveColor(color);
    objectData.emmisiveColor = vec4(color, 1.0f);
    objectData.metallicRoughness.r = material->getMetallic();
    objectData.metallicRoughness.g = material->getRoughness();
    objectData.flags.r = material->getLightingEnable() ? 1.0f: 0.0f;

    size = sizeof(objectData);
    m_graphics->updateUniformData(m_objectDataUniformBuffers[frameIndex], offset, (uint8_t*)&objectData, size);
  }
}