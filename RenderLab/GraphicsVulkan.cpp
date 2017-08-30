#include "stdafx.h"
#include "GraphicsVulkan.h"

namespace RenderLab
{
  GraphicsVulkan::GraphicsVulkan(string name, HINSTANCE hinstance, HWND window): Graphics(name, hinstance, window),
    m_shadowMaterial(nullptr),
    m_depthPrepassMaterial(nullptr),
    m_constantDepthBias(3.0f),
    m_slopeDepthBias(0.0f),
    m_lightFences(nullptr),
    m_lightFenceIndex(0),
    m_allocatedImageMemory(0),
    m_deferred(true),
    m_depthPrepass(true)
  {
  }


  GraphicsVulkan::~GraphicsVulkan()
  {
  }

  void GraphicsVulkan::initialize(uint32_t numFrames)
  {
    initializeInstance();
    initializeVirtualDevice();
    initializeProperties();
    initializeQueues(numFrames);
  }


  uint32_t GraphicsVulkan::acquireBackBuffer(shared_ptr<View> view)
  {
    vkViewData* viewData = (vkViewData*)view->getGraphicsData();
    vkBackBuffer &backBuffer = viewData->m_backBuffers.front();

    if (view->getType() == View::SCREEN)
    {
      // wait until acquire and render semaphores are waited/unsignaled
      vkWaitForFences(m_device, 1, &backBuffer.m_presentFence, true, UINT64_MAX);
      // reset the fence
      vkResetFences(m_device, 1, &backBuffer.m_presentFence);
      vkAcquireNextImageKHR(m_device, viewData->m_swapchain, UINT64_MAX, backBuffer.m_acquireSemaphore, VK_NULL_HANDLE, &backBuffer.m_imageIndex);
    }

    viewData->m_acquiredBackBuffer = backBuffer;
    viewData->m_backBuffers.pop();
    return backBuffer.m_imageIndex;
  }

  void GraphicsVulkan::setDepthBias(float constant, float slope)
  {
    m_constantDepthBias = constant;
    m_slopeDepthBias = slope;
  }

  float GraphicsVulkan::getGPUFrameTime()
  {
    return (m_currentTimestamp[1] - m_currentTimestamp[0]) * m_physicalDeviceProperties.limits.timestampPeriod;
  }

  float GraphicsVulkan::getGPUFrameTime2()
  {
    return (m_currentTimestamp[2] - m_currentTimestamp[1]) * m_physicalDeviceProperties.limits.timestampPeriod;
  }

  void GraphicsVulkan::renderBegin(shared_ptr<View> view, shared_ptr<View> lastView, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer, uint32_t frameIndex)
  {
    vkViewData* viewData = (vkViewData*)view->getGraphicsData();
    vkBackBuffer& backBuffer = viewData->m_acquiredBackBuffer;
    vkViewData* lastViewData = (vkViewData*)lastView->getGraphicsData();
    vkBackBuffer& lastBackBuffer = lastViewData->m_acquiredBackBuffer;
    vkUniformBufferData* frameDataUniformBufferData = (vkUniformBufferData*)frameDataUniformBuffer->getGraphicsData();
    vkUniformBufferData* objectDataUniformBufferData = (vkUniformBufferData*)objectDataUniformBuffer->getGraphicsData();

    if (m_lightFences == nullptr)
    {
      m_lightFences = new VkFence[m_shadowViewData.size()];
    }

    //vkWaitForFences(m_device, 1, &lastBackBuffer.m_renderFence, true, UINT64_MAX);
    vkResetFences(m_device, 1, &backBuffer.m_renderFence);

    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = 0; // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(viewData->m_commandBuffer[frameIndex], &commandBufferBeginInfo);

	  vkCmdResetQueryPool(viewData->m_commandBuffer[frameIndex], m_queryPool, 0, 3);
	  
    vkCmdWriteTimestamp(viewData->m_commandBuffer[frameIndex], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPool, 0);
    vkCmdSetViewport(viewData->m_commandBuffer[frameIndex], 0, 1, &viewData->m_viewport);
    vkCmdSetScissor(viewData->m_commandBuffer[frameIndex], 0, 1, &viewData->m_scissor);

    if (objectDataUniformBufferData->m_buffer != 0)
    {
      VkBufferMemoryBarrier bufferMemoryBarrier[2] = {};
      bufferMemoryBarrier[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      bufferMemoryBarrier[0].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
      bufferMemoryBarrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      bufferMemoryBarrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      bufferMemoryBarrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      bufferMemoryBarrier[0].buffer = frameDataUniformBufferData->m_buffer;
      bufferMemoryBarrier[0].offset = 0;
      bufferMemoryBarrier[0].size = VK_WHOLE_SIZE;

      bufferMemoryBarrier[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      bufferMemoryBarrier[1].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
      bufferMemoryBarrier[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      bufferMemoryBarrier[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      bufferMemoryBarrier[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      bufferMemoryBarrier[1].buffer = objectDataUniformBufferData->m_buffer;
      bufferMemoryBarrier[1].offset = 0;
      bufferMemoryBarrier[1].size = VK_WHOLE_SIZE;
      vkCmdPipelineBarrier(viewData->m_commandBuffer[frameIndex],
        VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 2, &bufferMemoryBarrier[0], 0, nullptr);
    }

    if (view->getType() == View::SHADOW)
    {
      vkCmdSetDepthBias(viewData->m_commandBuffer[frameIndex], m_constantDepthBias, 0.0f, m_slopeDepthBias);
    }

    array<VkClearValue, 8> clearValues = {};
    array<VkClearValue, 1> shadowClearValues = {};
    if (view->getType() == View::SCREEN)
    {
      if (m_deferred)
      {
        clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
        clearValues[1].depthStencil = { 1.0f, 0 };
        clearValues[2].color = { 0.0f, 0.2f, 0.8f, 1.0f };
        clearValues[3].color = { 0.0f, 0.2f, 0.8f, 1.0f };
        clearValues[4].color = { 0.0f, 0.2f, 0.8f, 1.0f };
        clearValues[5].color = { 0.0f, 0.2f, 0.8f, 1.0f };
        clearValues[6].color = { 0.0f, 0.2f, 0.8f, 1.0f };
        clearValues[7].depthStencil = { 1.0f, 0 };
      }
      else
      {
        clearValues[0].color = { 0.0f, 0.2f, 0.8f, 1.0f };
        clearValues[1].depthStencil = { 1.0f, 0 };
      }

    }
    else
    {
      shadowClearValues[0].depthStencil = { 1.0f, 0 };
    }

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = viewData->m_renderPass;

    if (view->getType() == View::SCREEN)
    {
      renderPassBeginInfo.clearValueCount = 2;
      renderPassBeginInfo.pClearValues = clearValues.data();
      if (m_deferred)
      {
        renderPassBeginInfo.clearValueCount = 8;
      }
    }
    else
    {
      renderPassBeginInfo.clearValueCount = 1;
      renderPassBeginInfo.pClearValues = shadowClearValues.data();
    }

    renderPassBeginInfo.framebuffer = viewData->m_framebuffers[frameIndex];
    renderPassBeginInfo.renderArea.extent = viewData->m_extent;
    //vkCmdBeginRenderPass(m_primaryCommandBuffer[frameIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    vkCmdBeginRenderPass(viewData->m_commandBuffer[frameIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
  }

  void GraphicsVulkan::bindPipeline(shared_ptr<Mesh> mesh, shared_ptr<View> view, uint32_t frameIndex, bool depthPrepass)
  {
    vkMeshData* meshData = (vkMeshData*)mesh->getGraphicsData();
    vkViewData* viewData = (vkViewData*)view->getGraphicsData();

    if (depthPrepass)
    {
      vkCmdBindPipeline(viewData->m_commandBuffer[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, meshData->m_depthPrepassPipelines[frameIndex]);
    }
    else
    {
      vkCmdBindPipeline(viewData->m_commandBuffer[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, meshData->m_pipelines[view][frameIndex]);
    }
  }

  void GraphicsVulkan::endDepthPrepass(shared_ptr<View> view, uint32_t frameIndex)
  {
    vkViewData* viewData = (vkViewData*)view->getGraphicsData();
    vkCmdNextSubpass(viewData->m_commandBuffer[frameIndex], VK_SUBPASS_CONTENTS_INLINE);
  }


  void GraphicsVulkan::render(shared_ptr<Mesh> mesh, uint32_t meshOffset, shared_ptr<View> view, uint32_t frameIndex, bool depthPrepass)
  {
    vkMeshData* meshData = (vkMeshData*)mesh->getGraphicsData();
    vkMaterialData* materialData = (vkMaterialData*)mesh->getMaterial()->getGraphicsData();
    vkViewData* viewData = (vkViewData*)view->getGraphicsData();

    if (view->getType() == View::SHADOW || view->getType() == View::SHADOW_CUBE)
    {
      materialData = (vkMaterialData*)m_shadowMaterial->getGraphicsData();
    }

    if (depthPrepass)
    {
      materialData = (vkMaterialData*)m_depthPrepassMaterial->getGraphicsData();
    }

    uint32_t dynamicOffsets[1];
    //dynamicOffsets[0] = 0;
    dynamicOffsets[0] = meshOffset;
    vkCmdBindDescriptorSets(viewData->m_commandBuffer[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
      materialData->m_pipelineLayout[frameIndex], 0, 1, &materialData->m_descriptorSet[frameIndex], 1, dynamicOffsets);

    const VkDeviceSize vb_offset = 0;
    vkCmdBindVertexBuffers(viewData->m_commandBuffer[frameIndex], 0, 1, &meshData->m_vertexBuffer, &vb_offset);
    vkCmdBindIndexBuffer(viewData->m_commandBuffer[frameIndex], meshData->m_indexBuffer, 0, meshData->m_indexType);
    vkCmdDrawIndexed(viewData->m_commandBuffer[frameIndex], (uint32_t)mesh->getIndexBufferSize(), 1, 0, 0, 0);
  }


  void GraphicsVulkan::renderEnd(shared_ptr<View> view, shared_ptr<View> lastView, bool lastLight, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer, uint32_t frameIndex)
  {
    vkViewData* viewData = (vkViewData*)view->getGraphicsData();
    vkBackBuffer& backBuffer = viewData->m_acquiredBackBuffer;

    vkViewData* lastViewData = (vkViewData*)lastView->getGraphicsData();
    vkBackBuffer& lastBackBuffer = lastViewData->m_acquiredBackBuffer;

    // Render the composite meshes
    if (view->getType() == View::SCREEN)
    {
      vkCmdWriteTimestamp(viewData->m_commandBuffer[frameIndex], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, 1);
      vkCmdNextSubpass(viewData->m_commandBuffer[frameIndex], VK_SUBPASS_CONTENTS_INLINE);
      vkCmdSetViewport(viewData->m_commandBuffer[frameIndex], 0, 1, &viewData->m_viewport);
      vkCmdSetScissor(viewData->m_commandBuffer[frameIndex], 0, 1, &viewData->m_scissor);

      shared_ptr<Mesh> mesh = view->getCompositeMesh(0);
      shared_ptr<Material> material = mesh->getMaterial();
      vkMeshData* meshData = (vkMeshData*)mesh->getGraphicsData();
      vkMaterialData* materialData = (vkMaterialData*)material->getGraphicsData();

      for (size_t i = 0; i < m_renderTechnique->getNumLightComponents(); i++)
      {
        shared_ptr<LightComponent> lightComponent = m_renderTechnique->getLightComponent((uint32_t)i);
        vec3 color;
        lightComponent->getDiffuse(color);
        m_lightPushConstants.m_lightColor = vec4(color, viewData->m_extent.width);

        mat4 transform;
        vec3 position;
        lightComponent->getPosition(position);
        lightComponent->getEntity(0)->getCompositeTransform(transform);
        m_lightPushConstants.m_lightPosition = vec4(vec3(transform * vec4(position, 1.0f)), viewData->m_extent.height);

        vkCmdPushConstants(
          viewData->m_commandBuffer[frameIndex], 
          materialData->m_pipelineLayout[frameIndex],
          VK_SHADER_STAGE_VERTEX_BIT| VK_SHADER_STAGE_FRAGMENT_BIT,
          0,
          sizeof(vkLightPushContants),
          &m_lightPushConstants);

        vkCmdBindPipeline(viewData->m_commandBuffer[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, meshData->m_pipelines[view][frameIndex]);
        uint32_t dynamicOffsets[1];
        dynamicOffsets[0] = 0;
        vkCmdBindDescriptorSets(viewData->m_commandBuffer[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
          materialData->m_pipelineLayout[frameIndex], 0, 1, &materialData->m_descriptorSet[frameIndex], 1, dynamicOffsets);

        const VkDeviceSize vb_offset = 0;
        vkCmdBindVertexBuffers(viewData->m_commandBuffer[frameIndex], 0, 1, &meshData->m_vertexBuffer, &vb_offset);
        vkCmdBindIndexBuffer(viewData->m_commandBuffer[frameIndex], meshData->m_indexBuffer, 0, meshData->m_indexType);
        vkCmdDrawIndexed(viewData->m_commandBuffer[frameIndex], (uint32_t)mesh->getIndexBufferSize(), 1, 0, 0, 0);
      }

      vkCmdEndRenderPass(viewData->m_commandBuffer[frameIndex]);
	  
	    vkCmdWriteTimestamp(viewData->m_commandBuffer[frameIndex], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, 2);
      vkEndCommandBuffer(viewData->m_commandBuffer[frameIndex]);

      // we will render to the swapchain images
      VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

      //wait for the image to be owned and signal for render completion
      VkSubmitInfo submitInfo = {};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitDstStageMask = &pipelineStageFlags;
      submitInfo.commandBufferCount = 1;
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = &lastBackBuffer.m_acquireSemaphore;
      submitInfo.pCommandBuffers = &viewData->m_commandBuffer[frameIndex];
      submitInfo.pSignalSemaphores = &backBuffer.m_renderSemaphore;

      vkQueueSubmit(m_commandQueue, 1, &submitInfo, backBuffer.m_renderFence);
      vkWaitForFences(m_device, 1, &backBuffer.m_renderFence, true, UINT64_MAX);

	    VkResult r = vkGetQueryPoolResults(m_device, m_queryPool, 0, 3, sizeof(m_currentTimestamp), (void*)m_currentTimestamp, sizeof(uint64_t), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
    }
  }

  void GraphicsVulkan::swapBackBuffer(shared_ptr<View> view, uint32_t frameIndex)
  {
    vkViewData* viewData = (vkViewData*)view->getGraphicsData();
    vkBackBuffer &backBuffer = viewData->m_acquiredBackBuffer;

    if (view->getType() == View::SCREEN)
    {
      VkPresentInfoKHR presentInfo = {};
      presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pWaitSemaphores = &backBuffer.m_renderSemaphore;
      presentInfo.swapchainCount = 1;
      presentInfo.pSwapchains = &viewData->m_swapchain;
      presentInfo.pImageIndices = &backBuffer.m_imageIndex;

      vkQueuePresentKHR(m_presentQueue, &presentInfo);

      vkQueueSubmit(m_presentQueue, 0, nullptr, backBuffer.m_presentFence);
    }

    viewData->m_backBuffers.push(backBuffer);
  }

  size_t GraphicsVulkan::getBufferAlignment()
  {
    return m_physicalDeviceProperties.limits.minStorageBufferOffsetAlignment;
  }

  void GraphicsVulkan::build(shared_ptr<Mesh> mesh, vector<shared_ptr<UniformBuffer>>& frameDataUniformBuffers, vector<shared_ptr<UniformBuffer>>& objectDataUniformBuffers, size_t numFrames, vector<shared_ptr<LightComponent>>& lightComponents)
  {
    vkMeshData* meshData = (vkMeshData*)mesh->getGraphicsData();
    if (meshData == nullptr || mesh->isDirty())
    {
      if (meshData != nullptr)
      {
        // TODO: Free old resources
      }

      meshData = new vkMeshData();
      meshData->m_stride = 0;
      meshData->m_vertexBufferSize = 0;

      size_t numBuffers = mesh->getNumBuffers();
      vector<VkVertexInputAttributeDescription> vertexInputAttributes(numBuffers);
      for (size_t i = 0; i<numBuffers; i++)
      {
        vertexInputAttributes[i].location = (uint32_t)i;
        vertexInputAttributes[i].binding = 0;
        if (mesh->getVertexBufferSize(i) == 2)
        {
          vertexInputAttributes[i].format = VK_FORMAT_R32G32_SFLOAT;
        }
        else
        {
          vertexInputAttributes[i].format = VK_FORMAT_R32G32B32_SFLOAT;
        }

        vertexInputAttributes[i].offset = meshData->m_stride;
        meshData->m_stride += (unsigned int)(mesh->getVertexBufferSize(i) * sizeof(float));
        meshData->m_vertexBufferSize += mesh->getVertexBufferNumBytes(i);
      }
      meshData->m_vertexInputAttributes = vertexInputAttributes;

      meshData->m_vertexInputBinding = {};
      meshData->m_vertexInputBinding.binding = 0;
      meshData->m_vertexInputBinding.stride = meshData->m_stride;
      meshData->m_vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      meshData->m_inputAssemblyState = {};
      meshData->m_inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
      meshData->m_inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      meshData->m_inputAssemblyState.primitiveRestartEnable = false;

      meshData->m_indexType = VK_INDEX_TYPE_UINT32;

      meshData->m_vertexInputState = {};
      meshData->m_vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      meshData->m_vertexInputState.vertexBindingDescriptionCount = 1;
      meshData->m_vertexInputState.pVertexBindingDescriptions = &meshData->m_vertexInputBinding;
      meshData->m_vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(meshData->m_vertexInputAttributes.size());
      meshData->m_vertexInputState.pVertexAttributeDescriptions = meshData->m_vertexInputAttributes.data();

      allocate_resources(mesh, meshData, meshData->m_vertexBufferSize, mesh->getIndexBufferSize() * sizeof(unsigned int));


      uint8_t *vertexBufferData = nullptr;
      uint8_t *indexBufferData = nullptr;
      vkMapMemory(m_device, meshData->m_memory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&vertexBufferData));
      indexBufferData = vertexBufferData + meshData->m_indexBufferMemoryOffset;


      bool hasNormals = false;
      bool hasTc0 = false;
      bool hasTangents = false;
      float* positions = mesh->getVertexBufferData(0);
      float* normals = nullptr;
      float* texCoords = nullptr;
      float* tangents = nullptr;
      float* bitangents = nullptr;

      if (mesh->getNumBuffers() > 1) {
        hasNormals = true;
        normals = mesh->getVertexBufferData(1);
      }
      if (mesh->getNumBuffers() > 2) {
        hasTc0 = true;
        texCoords = mesh->getVertexBufferData(2);
      }
      if (mesh->getNumBuffers() > 3) {
        hasTangents = true;
        tangents = mesh->getVertexBufferData(3);
        bitangents = mesh->getVertexBufferData(4);
      }

      float *vdst = reinterpret_cast<float *>(vertexBufferData);
      for (size_t i = 0, vindex = 0, tindex = 0; i < mesh->getNumVerts(); i++, vindex += 3, tindex += 2) {
        vdst[0] = positions[vindex];
        vdst[1] = positions[vindex + 1];
        vdst[2] = positions[vindex + 2];
        vdst += 3;
        if (hasNormals)
        {
          vdst[0] = normals[vindex];
          vdst[1] = normals[vindex + 1];
          vdst[2] = normals[vindex + 2];
          vdst += 3;
        }
        if (texCoords)
        {
          vdst[0] = texCoords[tindex];
          vdst[1] = texCoords[tindex + 1];
          vdst += 2;
        }
        if (hasTangents)
        {
          vdst[0] = tangents[vindex];
          vdst[1] = tangents[vindex + 1];
          vdst[2] = tangents[vindex + 2];
          vdst[3] = bitangents[vindex];
          vdst[4] = bitangents[vindex + 1];
          vdst[5] = bitangents[vindex + 2];
          vdst += 6;
        }
      }

      uint32_t *dst = reinterpret_cast<uint32_t *>(indexBufferData);
      unsigned int* indexBuffer = mesh->getIndexBuffer();
      for (unsigned int i = 0; i < mesh->getIndexBufferSize(); i++) {
        dst[i] = indexBuffer[i];
      }

      vkUnmapMemory(m_device, meshData->m_memory);

      mesh->setGraphicsData(meshData);
      mesh->setDirty(false);
    }
    build(mesh->getMaterial(), frameDataUniformBuffers, objectDataUniformBuffers, numFrames);

    if (m_depthPrepassMaterial == nullptr && m_depthPrepass)
    {
      m_depthPrepassMaterial = make_shared<Material>("Depth Prepass Material", Material::DEPTH_PREPASS);
      build(m_depthPrepassMaterial, frameDataUniformBuffers, objectDataUniformBuffers, numFrames);
    }


    VkPipeline* pipelines = new VkPipeline[numFrames];
    meshData->m_depthPrepassPipelines = new VkPipeline[numFrames];
    for (size_t i = 0; i < numFrames; i++)
    {
      pipelines[i] = loadPipeline(mesh, meshData, mesh->getMaterial(), i, m_onscreenView);
      if (m_depthPrepass)
      {
        meshData->m_depthPrepassPipelines[i] = loadPipeline(mesh, meshData, m_depthPrepassMaterial, i, m_onscreenView);
      }
    }
    meshData->m_pipelines[m_onscreenView] = pipelines;

    //if (m_shadowMaterial == nullptr)
    //{
    //  m_shadowMaterial = make_shared<Material>("Shadow Material", Material::SHADOW_CUBE);
    //  build(m_shadowMaterial, frameDataUniformBuffers, objectDataUniformBuffers, numFrames);
    //}

    for (size_t i = 0; i < lightComponents.size(); ++i)
    {
      if (lightComponents[i]->getCastShadow())
      {
        pipelines = new VkPipeline[numFrames];
        for (size_t j = 0; j < numFrames; ++j)
        {
          pipelines[j] = loadPipeline(mesh, meshData, m_shadowMaterial, j, lightComponents[i]->getShadowView());
        }
        meshData->m_pipelines[lightComponents[i]->getShadowView()] = pipelines;
      }
    }
  }

  void GraphicsVulkan::allocate_resources(shared_ptr<Mesh> mesh, vkMeshData* meshData, VkDeviceSize vertexBufferSize, VkDeviceSize indexBufferSize)
  {
      VkBufferCreateInfo bufferInfo = {};
      bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      bufferInfo.size = vertexBufferSize;
      bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      vkCreateBuffer(m_device, &bufferInfo, nullptr, &meshData->m_vertexBuffer);

      bufferInfo.size = indexBufferSize;
      bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      vkCreateBuffer(m_device, &bufferInfo, nullptr, &meshData->m_indexBuffer);

      VkMemoryRequirements vertexBufferMemoryRequirements, indexBufferMemoryRequitements;
      vkGetBufferMemoryRequirements(m_device, meshData->m_vertexBuffer, &vertexBufferMemoryRequirements);
      vkGetBufferMemoryRequirements(m_device, meshData->m_indexBuffer, &indexBufferMemoryRequitements);

      // indices follow vertices
      meshData->m_indexBufferMemoryOffset = vertexBufferMemoryRequirements.size +
        (indexBufferMemoryRequitements.alignment - (vertexBufferMemoryRequirements.size % indexBufferMemoryRequitements.alignment));

      VkMemoryAllocateInfo memoryAllocateInfo = {};
      memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      memoryAllocateInfo.allocationSize = meshData->m_indexBufferMemoryOffset + indexBufferMemoryRequitements.size;

      // find any supported and mappable memory type
      uint32_t memoryTypes = (vertexBufferMemoryRequirements.memoryTypeBits & indexBufferMemoryRequitements.memoryTypeBits);
      for (uint32_t idx = 0; idx < m_memoryFlags.size(); idx++) {
        if ((memoryTypes & (1 << idx)) &&
          (m_memoryFlags[idx] & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
          (m_memoryFlags[idx] & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
          // TODO this may not be reachable
          memoryAllocateInfo.memoryTypeIndex = idx;
          break;
        }
      }

      if (vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &meshData->m_memory) != VK_SUCCESS) {
        throw std::runtime_error("failed to create memory!");
      }

      vkBindBufferMemory(m_device, meshData->m_vertexBuffer, meshData->m_memory, 0);
      vkBindBufferMemory(m_device, meshData->m_indexBuffer, meshData->m_memory, meshData->m_indexBufferMemoryOffset);
  }

  void GraphicsVulkan::build(shared_ptr<Material> material, vector<shared_ptr<UniformBuffer>>& frameDataUniformBuffers, vector<shared_ptr<UniformBuffer>>& objectDataUniformBuffers, size_t numFrames)
  {
    vkMaterialData* materialData = (vkMaterialData*)material->getGraphicsData();
    if (materialData == nullptr || material->isDirty())
    {
      if (materialData != nullptr)
      {
        // TODO: Free old resources
      }

      materialData = new vkMaterialData();
      string vertexShaderFile = "shaders/";
      string fragmentShaderFile = "shaders/";
      if (material->getMaterialType() == Material::DEFERRED_LIT)
      {
        fragmentShaderFile += (material->getEmissiveTexture() == nullptr ? "GBuffer_CE_" : "GBuffer_TE_");
        fragmentShaderFile += (material->getAlbedoTexture() == nullptr ? "CA_" : "TA_");
        fragmentShaderFile += (material->getMetallicRoughnessTexture() == nullptr ? "CM_" : "TM_");
        fragmentShaderFile += (material->getOcclusionTexture() == nullptr ? "NO_" : "TO_");
        fragmentShaderFile += (material->getNormalTexture() == nullptr ? "VN.frag.spv" : "TN.frag.spv");
        if (material->hasNoTexture())
        {
          vertexShaderFile += "GBuffer_VN_NT.vert.spv";
        }
        else
        { 
          vertexShaderFile += (material->getNormalTexture() == nullptr ? "GBuffer_VN.vert.spv" : "GBuffer_TN.vert.spv");
        }
      }
      else if (material->getMaterialType() == Material::DEFERRED_COMPOSITE)
      {
        vertexShaderFile += "DeferredComposite.vert.spv";
        fragmentShaderFile += "DeferredComposite.frag.spv";
      }
	  else if (material->getMaterialType() == Material::DEPTH_PREPASS)
	  {
		  vertexShaderFile += "DepthPrepass.vert.spv";
		  fragmentShaderFile += "DepthPrepass.frag.spv";
	  }

      materialData->m_vertexShaderCode = readFile(vertexShaderFile);
      materialData->m_fragmentShaderCode = readFile(fragmentShaderFile);

      VkShaderModuleCreateInfo shaderInfo = {};
      shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

      shaderInfo.codeSize = materialData->m_vertexShaderCode.size();
      shaderInfo.pCode = (const uint32_t*)materialData->m_vertexShaderCode.data();
      vkCreateShaderModule(m_device, &shaderInfo, nullptr, &materialData->m_vertexShader);

      shaderInfo.codeSize = materialData->m_fragmentShaderCode.size();
      shaderInfo.pCode = (const uint32_t*)materialData->m_fragmentShaderCode.data();
      vkCreateShaderModule(m_device, &shaderInfo, nullptr, &materialData->m_fragmentShader);

      if (material->getMaterialType() == Material::SHADOW_CUBE)
      {
        shaderInfo.codeSize = materialData->m_geometryShaderCode.size();
        shaderInfo.pCode = (const uint32_t*)materialData->m_geometryShaderCode.data();
        vkCreateShaderModule(m_device, &shaderInfo, nullptr, &materialData->m_geometryShader);
      }

      if (material->getAlbedoTexture() != nullptr)
      {
        loadTexture(material->getAlbedoTexture());
      }

      if (material->getNormalTexture() != nullptr)
      {
        loadTexture(material->getNormalTexture());
      }

      if (material->getMetallicRoughnessTexture() != nullptr)
      {
        loadTexture(material->getMetallicRoughnessTexture());
      }

      if (material->getOcclusionTexture() != nullptr)
      {
        loadTexture(material->getOcclusionTexture());
      }

      if (material->getEmissiveTexture() != nullptr)
      {
        loadTexture(material->getEmissiveTexture());
      }

      materialData->m_descriptorSet = new VkDescriptorSet[numFrames];
      materialData->m_descriptorSetLayout = new VkDescriptorSetLayout[numFrames];
      materialData->m_pipelineLayout = new VkPipelineLayout[numFrames];
      materialData->m_descriptorPool = new VkDescriptorPool[numFrames];
      for (size_t i = 0; i < numFrames; i++)
      {
        createDescriptorSet(material, materialData, i);
        updateDescriptorSets(material, materialData, i, frameDataUniformBuffers[i], objectDataUniformBuffers[i]);
      }

      material->setGraphicsData(materialData);
      material->setDirty(false);
    }
  }

  void GraphicsVulkan::loadTexture(shared_ptr<Texture> texture)
  {
    vkTextureData* textureData = nullptr;

    map<string, vkTextureData*>::iterator it = m_textureMap.find(texture->getName());
    if (it != m_textureMap.end())
    {
      //element found;
      textureData = it->second;
      texture->setGraphicsData(textureData);
      return;
    }

    textureData = new vkTextureData();
    texture->setGraphicsData(textureData);
    m_textureMap[texture->getName()] = textureData;

    createTextureImage(texture);
    createImage(texture, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &textureData->m_image, &textureData->m_imageMemory);
    transitionImageLayout(textureData->m_stagingImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transitionImageLayout(textureData->m_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyImage(textureData->m_stagingImage, textureData->m_image, texture->getWidth(), texture->getHeight());
    transitionImageLayout(textureData->m_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    createImageView(textureData->m_image, VK_FORMAT_R8G8B8A8_UNORM, &textureData->m_imageView);
    createTextureSampler(&textureData->m_textureSampler);
  }

  void GraphicsVulkan::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginOneTimeCommands();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
      barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
      srcStages = VK_PIPELINE_STAGE_HOST_BIT;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      destStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
      srcStages = VK_PIPELINE_STAGE_HOST_BIT;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      destStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      srcStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      destStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
      throw std::invalid_argument("unsupported layout transition!");
    }

    //vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkCmdPipelineBarrier(commandBuffer, srcStages, destStages, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endOneTimeCommands(commandBuffer);
  }

  void GraphicsVulkan::copyImage(VkImage srcImage, VkImage dstImage, size_t width, size_t height) {
    VkCommandBuffer commandBuffer = beginOneTimeCommands();
    VkImageSubresourceLayers subResource = {};
    subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subResource.baseArrayLayer = 0;
    subResource.mipLevel = 0;
    subResource.layerCount = 1;

    VkImageCopy region = {};
    region.srcSubresource = subResource;
    region.dstSubresource = subResource;
    region.srcOffset = { 0, 0, 0 };
    region.dstOffset = { 0, 0, 0 };
    region.extent.width = (uint32_t)width;
    region.extent.height = (uint32_t)height;
    region.extent.depth = 1;

    vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endOneTimeCommands(commandBuffer);
  }

  void GraphicsVulkan::createImage(shared_ptr<Texture> texture, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory) {
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = (uint32_t)texture->getWidth();
    imageInfo.extent.height = (uint32_t)texture->getHeight();
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, image) != VK_SUCCESS) {
      throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, *image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    memory_type_from_properties(memRequirements.memoryTypeBits, properties, &allocInfo.memoryTypeIndex);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, imageMemory) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate image memory!");
    }
    m_allocatedImageMemory += allocInfo.allocationSize;
    

    vkBindImageMemory(m_device, *image, *imageMemory, 0);
  }

  void GraphicsVulkan::createTextureImage(shared_ptr<Texture> texture) {
    size_t width = texture->getWidth();
    size_t height = texture->getHeight();
    VkDeviceSize imageSize = width * height * 4;
    vkTextureData* textureData = (vkTextureData*)texture->getGraphicsData();

    createImage(texture, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &(textureData->m_stagingImage), &textureData->m_stagingImageMemory);

    VkImageSubresource subresource = {};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel = 0;
    subresource.arrayLayer = 0;

    VkSubresourceLayout stagingImageLayout;
    vkGetImageSubresourceLayout(m_device, textureData->m_stagingImage, &subresource, &stagingImageLayout);

    void* data;
    vkMapMemory(m_device, textureData->m_stagingImageMemory, 0, imageSize, 0, &data);

    unsigned char* srcData = texture->getData();
    if (stagingImageLayout.rowPitch == width * 4) 
    {
      memcpy(data, srcData, (size_t)imageSize);
    }
    else 
    {
      uint8_t* dataBytes = reinterpret_cast<uint8_t*>(data);

      for (int y = 0; y < height; y++) {
        memcpy(&dataBytes[y * stagingImageLayout.rowPitch], &srcData[y * width * 4], width * 4);
      }
    }

    vkUnmapMemory(m_device, textureData->m_stagingImageMemory);
  }

  void GraphicsVulkan::createImageView(VkImage image, VkFormat format, VkImageView* imageView) {
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device, &viewInfo, nullptr, imageView) != VK_SUCCESS) {
      throw std::runtime_error("failed to create texture image view!");
    }
  }

  void GraphicsVulkan::createTextureSampler(VkSampler* sampler)
  {
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, sampler) != VK_SUCCESS) {
      throw std::runtime_error("failed to create texture sampler!");
    }
  }

  void GraphicsVulkan::createDescriptorSet(shared_ptr<Material> material, vkMaterialData* materialData, size_t frameNumber)
  {
    vector<VkDescriptorSetLayoutBinding> bindings;
    VkDescriptorSetLayoutBinding frameDataLayoutBinding = {};
    frameDataLayoutBinding.binding = 0;
    frameDataLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    frameDataLayoutBinding.descriptorCount = 1;
    frameDataLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    bindings.push_back(frameDataLayoutBinding);

    VkDescriptorSetLayoutBinding objectDataLayoutBinding = {};
    objectDataLayoutBinding.binding = 1;
    objectDataLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    objectDataLayoutBinding.descriptorCount = 1;
    objectDataLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |  VK_SHADER_STAGE_VERTEX_BIT;
    bindings.push_back(objectDataLayoutBinding);

    VkDescriptorSetLayoutBinding albedoSamplerLayoutBinding = {};
    VkDescriptorSetLayoutBinding normalSamplerLayoutBinding = {};
    VkDescriptorSetLayoutBinding metallicRoughnessSamplerLayoutBinding = {};
    VkDescriptorSetLayoutBinding occlusionSamplerLayoutBinding = {};
    VkDescriptorSetLayoutBinding emissiveSamplerLayoutBinding = {};

    if (material->getMaterialType() == Material::DEFERRED_LIT)
    {
      if (material->getAlbedoTexture() != nullptr)
      {
        albedoSamplerLayoutBinding.binding = 2;
        albedoSamplerLayoutBinding.descriptorCount = 1;
        albedoSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        albedoSamplerLayoutBinding.pImmutableSamplers = nullptr;
        albedoSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(albedoSamplerLayoutBinding);
      }

      if (material->getNormalTexture() != nullptr)
      {
        normalSamplerLayoutBinding.binding = 3;
        normalSamplerLayoutBinding.descriptorCount = 1;
        normalSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        normalSamplerLayoutBinding.pImmutableSamplers = nullptr;
        normalSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(normalSamplerLayoutBinding);
      }

      if (material->getMetallicRoughnessTexture() != nullptr)
      {
        metallicRoughnessSamplerLayoutBinding.binding = 4;
        metallicRoughnessSamplerLayoutBinding.descriptorCount = 1;
        metallicRoughnessSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        metallicRoughnessSamplerLayoutBinding.pImmutableSamplers = nullptr;
        metallicRoughnessSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(metallicRoughnessSamplerLayoutBinding);
      }

      if (material->getOcclusionTexture() != nullptr)
      {
        occlusionSamplerLayoutBinding.binding = 5;
        occlusionSamplerLayoutBinding.descriptorCount = 1;
        occlusionSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        occlusionSamplerLayoutBinding.pImmutableSamplers = nullptr;
        occlusionSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(occlusionSamplerLayoutBinding);
      }

      if (material->getEmissiveTexture() != nullptr)
      {
        emissiveSamplerLayoutBinding.binding = 6;
        emissiveSamplerLayoutBinding.descriptorCount = 1;
        emissiveSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        emissiveSamplerLayoutBinding.pImmutableSamplers = nullptr;
        emissiveSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(emissiveSamplerLayoutBinding);
      }
    }

    if (material->getMaterialType() == Material::DEFERRED_COMPOSITE)
    {
      VkDescriptorSetLayoutBinding positionSamplerLayoutBinding = {};
      positionSamplerLayoutBinding.binding = 2;
      positionSamplerLayoutBinding.descriptorCount = 1;
      positionSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      positionSamplerLayoutBinding.pImmutableSamplers = nullptr;
      positionSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      bindings.push_back(positionSamplerLayoutBinding);

      normalSamplerLayoutBinding.binding = 3;
      normalSamplerLayoutBinding.descriptorCount = 1;
      normalSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      normalSamplerLayoutBinding.pImmutableSamplers = nullptr;
      normalSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      bindings.push_back(normalSamplerLayoutBinding);

      VkDescriptorSetLayoutBinding albedoSamplerLayoutBinding = {};
      albedoSamplerLayoutBinding.binding = 4;
      albedoSamplerLayoutBinding.descriptorCount = 1;
      albedoSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      albedoSamplerLayoutBinding.pImmutableSamplers = nullptr;
      albedoSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      bindings.push_back(albedoSamplerLayoutBinding);

      VkDescriptorSetLayoutBinding metallicRoughnessSamplerLayoutBinding = {};
      metallicRoughnessSamplerLayoutBinding.binding = 5;
      metallicRoughnessSamplerLayoutBinding.descriptorCount = 1;
      metallicRoughnessSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      metallicRoughnessSamplerLayoutBinding.pImmutableSamplers = nullptr;
      metallicRoughnessSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      bindings.push_back(metallicRoughnessSamplerLayoutBinding);

      VkDescriptorSetLayoutBinding emissiveSamplerLayoutBinding = {};
      emissiveSamplerLayoutBinding.binding = 6;
      emissiveSamplerLayoutBinding.descriptorCount = 1;
      emissiveSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      emissiveSamplerLayoutBinding.pImmutableSamplers = nullptr;
      emissiveSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      bindings.push_back(emissiveSamplerLayoutBinding);


      //if (material->getMaterialType() == Material::LIT || material->getMaterialType() == Material::LIT_NORMALMAP || 
      //    material->getMaterialType() == Material::DEFERRED_LIT || material->getMaterialType() == Material::LIT_NOTEXTURE ||
      //    material->getMaterialType() == Material::DEFERRED_COMPOSITE)
      //{
      //  VkDescriptorSetLayoutBinding shadowLayoutBinding = {};
      //  shadowLayoutBinding.binding = 4;
      //  shadowLayoutBinding.descriptorCount = (uint32_t)m_shadowViewData.size();
      //  shadowLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      //  shadowLayoutBinding.pImmutableSamplers = nullptr;
      //  shadowLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      //  bindings.push_back(shadowLayoutBinding);
      //}
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = (uint32_t)bindings.size();
    descriptorSetLayoutInfo.pBindings = bindings.data();
    vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutInfo, nullptr, &materialData->m_descriptorSetLayout[frameNumber]);


    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &materialData->m_descriptorSetLayout[frameNumber];

    VkPushConstantRange pushConstantRange = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,  sizeof(vkLightPushContants) };
    if (material->getMaterialType() == Material::DEFERRED_COMPOSITE)
    {
      pipelineLayoutInfo.pushConstantRangeCount = 1;
      pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    }

    vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &materialData->m_pipelineLayout[frameNumber]);

    vector<VkDescriptorPoolSize> descriptorPoolSize;
    VkDescriptorPoolSize frameDataPoolSize;

    frameDataPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    frameDataPoolSize.descriptorCount = 1;
    descriptorPoolSize.push_back(frameDataPoolSize);

    VkDescriptorPoolSize objectDataPoolSize;
    objectDataPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    objectDataPoolSize.descriptorCount = 1;
    descriptorPoolSize.push_back(objectDataPoolSize);

    if (material->getMaterialType() == Material::DEFERRED_LIT)
    {
      if (material->getAlbedoTexture() != nullptr)
      {
        VkDescriptorPoolSize albedoSamplerPoolSize;
        albedoSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        albedoSamplerPoolSize.descriptorCount = 1;
        descriptorPoolSize.push_back(albedoSamplerPoolSize);
      }

      if (material->getNormalTexture() != nullptr)
      {
        VkDescriptorPoolSize normalSamplerPoolSize;
        normalSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        normalSamplerPoolSize.descriptorCount = 1;
        descriptorPoolSize.push_back(normalSamplerPoolSize);
      }

      if (material->getMetallicRoughnessTexture() != nullptr)
      {
        VkDescriptorPoolSize metallicRoughnessSamplerPoolSize;
        metallicRoughnessSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        metallicRoughnessSamplerPoolSize.descriptorCount = 1;
        descriptorPoolSize.push_back(metallicRoughnessSamplerPoolSize);
      }

      if (material->getOcclusionTexture() != nullptr)
      {
        VkDescriptorPoolSize occlusionSamplerPoolSize;
        occlusionSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        occlusionSamplerPoolSize.descriptorCount = 1;
        descriptorPoolSize.push_back(occlusionSamplerPoolSize);
      }

      if (material->getEmissiveTexture() != nullptr)
      {
        VkDescriptorPoolSize emissiveSamplerPoolSize;
        emissiveSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        emissiveSamplerPoolSize.descriptorCount = 1;
        descriptorPoolSize.push_back(emissiveSamplerPoolSize);
      }
    }

    if (material->getMaterialType() == Material::DEFERRED_COMPOSITE)
    {
      VkDescriptorPoolSize positionSamplerPoolSize;
      positionSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      positionSamplerPoolSize.descriptorCount = 1;
      descriptorPoolSize.push_back(positionSamplerPoolSize);

      VkDescriptorPoolSize normalSamplerPoolSize;
      normalSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      normalSamplerPoolSize.descriptorCount = 1;
      descriptorPoolSize.push_back(normalSamplerPoolSize);

      VkDescriptorPoolSize albedoSamplerPoolSize;
      albedoSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      albedoSamplerPoolSize.descriptorCount = 1;
      descriptorPoolSize.push_back(albedoSamplerPoolSize);

      VkDescriptorPoolSize metallicRoughnessSamplerPoolSize;
      metallicRoughnessSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      metallicRoughnessSamplerPoolSize.descriptorCount = 1;
      descriptorPoolSize.push_back(metallicRoughnessSamplerPoolSize);

      VkDescriptorPoolSize emissiveSamplerPoolSize;
      emissiveSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      emissiveSamplerPoolSize.descriptorCount = 1;
      descriptorPoolSize.push_back(emissiveSamplerPoolSize);

      //if (material->getMaterialType() == Material::LIT || material->getMaterialType() == Material::LIT_NORMALMAP ||
      //  material->getMaterialType() == Material::DEFERRED_LIT || material->getMaterialType() == Material::LIT_NOTEXTURE ||
      //  material->getMaterialType() == Material::DEFERRED_COMPOSITE)
      //{
      //  VkDescriptorPoolSize shadowPoolSize;
      //  shadowPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      //  shadowPoolSize.descriptorCount = (uint32_t)m_shadowViewData.size();
      //  descriptorPoolSize.push_back(shadowPoolSize);
      //}
    }

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.maxSets = 1;
    descriptorPoolInfo.poolSizeCount = (uint32_t)descriptorPoolSize.size();
    descriptorPoolInfo.pPoolSizes = descriptorPoolSize.data();

    // create descriptor pool
    vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &materialData->m_descriptorPool[frameNumber]);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(1, materialData->m_descriptorSetLayout[frameNumber]);
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = materialData->m_descriptorPool[frameNumber];
    descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

    // create descriptor sets
    std::vector<VkDescriptorSet> descriptorSets(1, VK_NULL_HANDLE);
    vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, descriptorSets.data());

    materialData->m_descriptorSet[frameNumber] = descriptorSets[0];
  }

  void GraphicsVulkan::updateDescriptorSets(shared_ptr<Material> material, vkMaterialData* materialData, size_t frameNumber, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer)
  {
    vkUniformBufferData* frameDataUniformBufferData = (vkUniformBufferData*)frameDataUniformBuffer->getGraphicsData();
    vkUniformBufferData* objectDataUniformBufferData = (vkUniformBufferData*)objectDataUniformBuffer->getGraphicsData();
    std::vector<VkDescriptorBufferInfo> descriptorBufferInfo(2);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet;

    VkDescriptorBufferInfo frameDataDescBuffer = {};
    frameDataDescBuffer.buffer = frameDataUniformBufferData->m_buffer;
    frameDataDescBuffer.offset = 0;
    frameDataDescBuffer.range = VK_WHOLE_SIZE;
    descriptorBufferInfo[0] = frameDataDescBuffer;

    VkWriteDescriptorSet frameDataDescWrite = {};
    frameDataDescWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    frameDataDescWrite.dstSet = materialData->m_descriptorSet[frameNumber];
    frameDataDescWrite.dstBinding = 0;
    frameDataDescWrite.dstArrayElement = 0;
    frameDataDescWrite.descriptorCount = 1;
    frameDataDescWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    frameDataDescWrite.pBufferInfo = &descriptorBufferInfo[0];
    writeDescriptorSet.push_back(frameDataDescWrite);

    VkDescriptorBufferInfo objectDataDescBuffer = {};
    objectDataDescBuffer.buffer = objectDataUniformBufferData->m_buffer;
    objectDataDescBuffer.offset = 0;
    objectDataDescBuffer.range = VK_WHOLE_SIZE;
    descriptorBufferInfo[1] = objectDataDescBuffer;

    VkWriteDescriptorSet objectDataDescWrite = {};
    objectDataDescWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    objectDataDescWrite.dstSet = materialData->m_descriptorSet[frameNumber];
    objectDataDescWrite.dstBinding = 1;
    objectDataDescWrite.dstArrayElement = 0;
    objectDataDescWrite.descriptorCount = 1;
    objectDataDescWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    objectDataDescWrite.pBufferInfo = &descriptorBufferInfo[1];
    writeDescriptorSet.push_back(objectDataDescWrite);

    if (material->getMaterialType() == Material::DEFERRED_LIT)
    {
      if (material->getAlbedoTexture() != nullptr)
      {
        shared_ptr<Texture> texture = material->getAlbedoTexture();
        vkTextureData* textureData = (vkTextureData*)texture->getGraphicsData();
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureData->m_imageView;
        imageInfo.sampler = textureData->m_textureSampler;

        VkWriteDescriptorSet desc_sampler_write = {};
        desc_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_sampler_write.dstSet = materialData->m_descriptorSet[frameNumber];
        desc_sampler_write.dstBinding = 2;
        desc_sampler_write.dstArrayElement = 0;
        desc_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_sampler_write.descriptorCount = 1;
        desc_sampler_write.pImageInfo = &imageInfo;
        writeDescriptorSet.push_back(desc_sampler_write);
      }

      if (material->getNormalTexture() != nullptr)
      {
        shared_ptr<Texture> normalTexture = material->getNormalTexture();
        vkTextureData* normalTextureData = (vkTextureData*)normalTexture->getGraphicsData();
        VkDescriptorImageInfo normalImageInfo = {};
        normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normalImageInfo.imageView = normalTextureData->m_imageView;
        normalImageInfo.sampler = normalTextureData->m_textureSampler;

        VkWriteDescriptorSet desc_normal_sampler_write = {};
        desc_normal_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_normal_sampler_write.dstSet = materialData->m_descriptorSet[frameNumber];
        desc_normal_sampler_write.dstBinding = 3;
        desc_normal_sampler_write.dstArrayElement = 0;
        desc_normal_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_normal_sampler_write.descriptorCount = 1;
        desc_normal_sampler_write.pImageInfo = &normalImageInfo;
        writeDescriptorSet.push_back(desc_normal_sampler_write);
      }

      if (material->getMetallicRoughnessTexture() != nullptr)
      {
        shared_ptr<Texture> roughnessTexture = material->getMetallicRoughnessTexture();
        vkTextureData* roughnessTextureData = (vkTextureData*)roughnessTexture->getGraphicsData();
        VkDescriptorImageInfo roughnessImageInfo = {};
        roughnessImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        roughnessImageInfo.imageView = roughnessTextureData->m_imageView;
        roughnessImageInfo.sampler = roughnessTextureData->m_textureSampler;

        VkWriteDescriptorSet desc_roughness_sampler_write = {};
        desc_roughness_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_roughness_sampler_write.dstSet = materialData->m_descriptorSet[frameNumber];
        desc_roughness_sampler_write.dstBinding = 4;
        desc_roughness_sampler_write.dstArrayElement = 0;
        desc_roughness_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_roughness_sampler_write.descriptorCount = 1;
        desc_roughness_sampler_write.pImageInfo = &roughnessImageInfo;
        writeDescriptorSet.push_back(desc_roughness_sampler_write);
      }

      if (material->getOcclusionTexture() != nullptr)
      {
        shared_ptr<Texture> occlusionTexture = material->getOcclusionTexture();
        vkTextureData* occlusionTextureData = (vkTextureData*)occlusionTexture->getGraphicsData();
        VkDescriptorImageInfo occlusionImageInfo = {};
        occlusionImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        occlusionImageInfo.imageView = occlusionTextureData->m_imageView;
        occlusionImageInfo.sampler = occlusionTextureData->m_textureSampler;

        VkWriteDescriptorSet desc_occlusion_sampler_write = {};
        desc_occlusion_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_occlusion_sampler_write.dstSet = materialData->m_descriptorSet[frameNumber];
        desc_occlusion_sampler_write.dstBinding = 5;
        desc_occlusion_sampler_write.dstArrayElement = 0;
        desc_occlusion_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_occlusion_sampler_write.descriptorCount = 1;
        desc_occlusion_sampler_write.pImageInfo = &occlusionImageInfo;
        writeDescriptorSet.push_back(desc_occlusion_sampler_write);
      }

      if (material->getEmissiveTexture() != nullptr)
      {
        shared_ptr<Texture> emissiveTexture = material->getMetallicRoughnessTexture();
        vkTextureData* emissiveTextureData = (vkTextureData*)emissiveTexture->getGraphicsData();
        VkDescriptorImageInfo emissiveImageInfo = {};
        emissiveImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        emissiveImageInfo.imageView = emissiveTextureData->m_imageView;
        emissiveImageInfo.sampler = emissiveTextureData->m_textureSampler;

        VkWriteDescriptorSet desc_emissive_sampler_write = {};
        desc_emissive_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        desc_emissive_sampler_write.dstSet = materialData->m_descriptorSet[frameNumber];
        desc_emissive_sampler_write.dstBinding = 6;
        desc_emissive_sampler_write.dstArrayElement = 0;
        desc_emissive_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc_emissive_sampler_write.descriptorCount = 1;
        desc_emissive_sampler_write.pImageInfo = &emissiveImageInfo;
        writeDescriptorSet.push_back(desc_emissive_sampler_write);
      }
    }

    if (material->getMaterialType() == Material::DEFERRED_COMPOSITE)
    {
      vkViewData* viewData = (vkViewData*)m_onscreenView->getGraphicsData();

      VkDescriptorImageInfo positionImageInfo = {};
      positionImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      positionImageInfo.imageView = viewData->m_gBuffer.m_position.m_view;
      positionImageInfo.sampler = viewData->m_gBuffer.m_position.m_textureSampler;

      VkWriteDescriptorSet desc_position_sampler_write = {};
      desc_position_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      desc_position_sampler_write.dstSet = materialData->m_descriptorSet[frameNumber];
      desc_position_sampler_write.dstBinding = 2;
      desc_position_sampler_write.dstArrayElement = 0;
      desc_position_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      desc_position_sampler_write.descriptorCount = 1;
      desc_position_sampler_write.pImageInfo = &positionImageInfo;
      writeDescriptorSet.push_back(desc_position_sampler_write);

      VkDescriptorImageInfo normalImageInfo = {};
      normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      normalImageInfo.imageView = viewData->m_gBuffer.m_normal.m_view;
      normalImageInfo.sampler = viewData->m_gBuffer.m_normal.m_textureSampler;

      VkWriteDescriptorSet desc_normal_sampler_write = {};
      desc_normal_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      desc_normal_sampler_write.dstSet = materialData->m_descriptorSet[frameNumber];
      desc_normal_sampler_write.dstBinding = 3;
      desc_normal_sampler_write.dstArrayElement = 0;
      desc_normal_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      desc_normal_sampler_write.descriptorCount = 1;
      desc_normal_sampler_write.pImageInfo = &normalImageInfo;
      writeDescriptorSet.push_back(desc_normal_sampler_write);

      VkDescriptorImageInfo albedoImageInfo = {};
      albedoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      albedoImageInfo.imageView = viewData->m_gBuffer.m_albedo.m_view;
      albedoImageInfo.sampler = viewData->m_gBuffer.m_albedo.m_textureSampler;

      VkWriteDescriptorSet desc_albedo_sampler_write = {};
      desc_albedo_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      desc_albedo_sampler_write.dstSet = materialData->m_descriptorSet[frameNumber];
      desc_albedo_sampler_write.dstBinding = 4;
      desc_albedo_sampler_write.dstArrayElement = 0;
      desc_albedo_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      desc_albedo_sampler_write.descriptorCount = 1;
      desc_albedo_sampler_write.pImageInfo = &albedoImageInfo;
      writeDescriptorSet.push_back(desc_albedo_sampler_write);

      VkDescriptorImageInfo metallicRoughnessImageInfo = {};
      metallicRoughnessImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      metallicRoughnessImageInfo.imageView = viewData->m_gBuffer.m_metallicRoughnessFlags.m_view;
      metallicRoughnessImageInfo.sampler = viewData->m_gBuffer.m_metallicRoughnessFlags.m_textureSampler;

      VkWriteDescriptorSet desc_metallic_roughness_sampler_write = {};
      desc_metallic_roughness_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      desc_metallic_roughness_sampler_write.dstSet = materialData->m_descriptorSet[frameNumber];
      desc_metallic_roughness_sampler_write.dstBinding = 5;
      desc_metallic_roughness_sampler_write.dstArrayElement = 0;
      desc_metallic_roughness_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      desc_metallic_roughness_sampler_write.descriptorCount = 1;
      desc_metallic_roughness_sampler_write.pImageInfo = &metallicRoughnessImageInfo;
      writeDescriptorSet.push_back(desc_metallic_roughness_sampler_write);

      VkDescriptorImageInfo emissiveImageInfo = {};
      emissiveImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      emissiveImageInfo.imageView = viewData->m_gBuffer.m_emissive.m_view;
      emissiveImageInfo.sampler = viewData->m_gBuffer.m_emissive.m_textureSampler;

      VkWriteDescriptorSet desc_emissive_sampler_write = {};
      desc_emissive_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      desc_emissive_sampler_write.dstSet = materialData->m_descriptorSet[frameNumber];
      desc_emissive_sampler_write.dstBinding = 6;
      desc_emissive_sampler_write.dstArrayElement = 0;
      desc_emissive_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      desc_emissive_sampler_write.descriptorCount = 1;
      desc_emissive_sampler_write.pImageInfo = &emissiveImageInfo;
      writeDescriptorSet.push_back(desc_emissive_sampler_write);

      //vector<VkDescriptorImageInfo> shadowImageInfo(m_shadowViewData.size());
      //if (material->getMaterialType() == Material::LIT || material->getMaterialType() == Material::LIT_NORMALMAP ||
      //  material->getMaterialType() == Material::DEFERRED_LIT || material->getMaterialType() == Material::LIT_NOTEXTURE ||
      //  material->getMaterialType() == Material::DEFERRED_COMPOSITE)
      //{
      //  for (size_t i = 0; i < m_shadowViewData.size(); ++i)
      //  {
      //    shadowImageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      //    shadowImageInfo[i].imageView = m_shadowViewData[i]->m_depthView;
      //    shadowImageInfo[i].sampler = m_shadowViewData[i]->m_shadowSampler;

      //    VkWriteDescriptorSet desc_shadow_sampler_write = {};
      //    desc_shadow_sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      //    desc_shadow_sampler_write.dstSet = materialData->m_descriptorSet[frameNumber];
      //    desc_shadow_sampler_write.dstBinding = 4;
      //    desc_shadow_sampler_write.dstArrayElement = (uint32_t)i;
      //    desc_shadow_sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      //    desc_shadow_sampler_write.descriptorCount = 1;
      //    desc_shadow_sampler_write.pImageInfo = &shadowImageInfo[i];
      //    writeDescriptorSet.push_back(desc_shadow_sampler_write);
      //  }
      //}
    }

    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, nullptr);
  }

  VkPipeline GraphicsVulkan::loadPipeline(shared_ptr<Mesh> mesh, vkMeshData* meshData, shared_ptr<Material> material, size_t frameIndex, shared_ptr<View> view)
  {
    int pipelineIndex = -1;

    for (size_t i = 0; i < m_pipelineCache.size(); i++)
    {
      if (mesh->getNumBuffers() == m_pipelineCache[i]->m_numMeshBuffers && 
          material->getMaterialType() == m_pipelineCache[i]->m_materialType &&
          frameIndex == m_pipelineCache[i]->m_frameIndex)
      {
        pipelineIndex = (int)i;
        break;
      }
    }

    if (pipelineIndex != -1)
    {
      return m_pipelineCache[pipelineIndex]->m_pipeline;
    }

    vkPipelineCacheInfo* pipelineCacheInfo = new vkPipelineCacheInfo();
    pipelineCacheInfo->m_materialType = material->getMaterialType();
    pipelineCacheInfo->m_numMeshBuffers = mesh->getNumBuffers();
    pipelineCacheInfo->m_frameIndex = frameIndex;
    m_pipelineCache.push_back(pipelineCacheInfo);

    vkMaterialData* materialData = (vkMaterialData*)material->getGraphicsData();
    vkViewData* viewData = (vkViewData*)view->getGraphicsData();

    VkPipelineShaderStageCreateInfo shaderStageInfo[3] = {};
    shaderStageInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStageInfo[0].module = materialData->m_vertexShader;
    shaderStageInfo[0].pName = "main";
    shaderStageInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStageInfo[1].module = materialData->m_fragmentShader;
    shaderStageInfo[1].pName = "main";
    if (material->getMaterialType() == Material::SHADOW_CUBE)
    {
      shaderStageInfo[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      shaderStageInfo[2].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
      shaderStageInfo[2].module = materialData->m_geometryShader;
      shaderStageInfo[2].pName = "main";
    }

    VkPipelineViewportStateCreateInfo viewportInfo = {};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationInfo = {};
    rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationInfo.depthClampEnable = false;
    rasterizationInfo.rasterizerDiscardEnable = false;
    rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    
    if (view->getType() == View::SCREEN)
    {
      rasterizationInfo.depthBiasEnable = false;
    }
    else
    {
      rasterizationInfo.depthBiasEnable = true;
    }
    rasterizationInfo.lineWidth = 1.0f; 

    VkPipelineMultisampleStateCreateInfo multisampleInfo = {};
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleInfo.sampleShadingEnable = false;
    multisampleInfo.pSampleMask = nullptr;
    multisampleInfo.alphaToCoverageEnable = false;
    multisampleInfo.alphaToOneEnable = false;

    vector<VkPipelineColorBlendAttachmentState> blendAttachments;
    VkPipelineColorBlendAttachmentState blendAttachment = {};   
    if (view->getType() == View::SCREEN && m_deferred && material->getMaterialType() == Material::DEFERRED_COMPOSITE)
    {
      blendAttachment.blendEnable = true;
    }
    else
    {
      blendAttachment.blendEnable = false;
    }
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
      VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT |
      VK_COLOR_COMPONENT_A_BIT;
    blendAttachments.push_back(blendAttachment);

    if (m_deferred && material->getMaterialType() == Material::DEFERRED_LIT)
    {
      blendAttachments.push_back(blendAttachment);
      blendAttachments.push_back(blendAttachment);
      blendAttachments.push_back(blendAttachment);
      blendAttachments.push_back(blendAttachment);
    }

    VkPipelineColorBlendStateCreateInfo blendInfo = {};
    blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendInfo.logicOpEnable = false;
    blendInfo.attachmentCount = (uint32_t)blendAttachments.size();
    blendInfo.pAttachments = blendAttachments.data();

    std::array<VkDynamicState, 2> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR
    };
    struct VkPipelineDynamicStateCreateInfo dynamicInfo = {};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = (uint32_t)dynamicStates.size();
    dynamicInfo.pDynamicStates = dynamicStates.data();

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    if (view->getType() == View::SCREEN && m_deferred && material->getMaterialType() == Material::DEFERRED_COMPOSITE)
    {
      depthStencil.depthTestEnable = VK_FALSE;
      depthStencil.depthWriteEnable = VK_FALSE;
    }
    else if (view->getType() == View::SCREEN && m_deferred && material->getMaterialType() == Material::DEPTH_PREPASS)
    {
      depthStencil.depthTestEnable = VK_TRUE;
      depthStencil.depthWriteEnable = VK_TRUE;
    }
    else
    {
      if (m_depthPrepass)
      {
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_FALSE;
      }
      else
      {
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
      }
    }

    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    if (material->getMaterialType() == Material::SHADOW_CUBE)
    {
      pipelineInfo.stageCount = 3;
    }
    else
    {
      pipelineInfo.stageCount = 2;
    }
    
    pipelineInfo.pStages = shaderStageInfo;
    pipelineInfo.pVertexInputState = &meshData->m_vertexInputState;
    pipelineInfo.pInputAssemblyState = &meshData->m_inputAssemblyState;
    pipelineInfo.pTessellationState = nullptr;
    pipelineInfo.pViewportState = &viewportInfo;
    pipelineInfo.pRasterizationState = &rasterizationInfo;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pDepthStencilState = &depthStencil;

    if (material->getMaterialType() == Material::SHADOW || material->getMaterialType() == Material::SHADOW_CUBE || material->getMaterialType() == Material::DEPTH_PREPASS)
    {
      pipelineInfo.pColorBlendState = nullptr;
    }
    else
    {
      pipelineInfo.pColorBlendState = &blendInfo;
    }
    
    pipelineInfo.pDynamicState = &dynamicInfo;
    pipelineInfo.layout = materialData->m_pipelineLayout[frameIndex];
    pipelineInfo.renderPass = viewData->m_renderPass;
    
    if (material->getMaterialType() == Material::DEPTH_PREPASS)
    {
      pipelineInfo.subpass = 0;
    }
    else if(material->getMaterialType() == Material::DEFERRED_COMPOSITE)
    {
      pipelineInfo.subpass = m_depthPrepass? 2: 1;
    }
    else
    {
      pipelineInfo.subpass = m_depthPrepass ? 1: 0;
    }
    
    VkResult res = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineCacheInfo->m_pipeline);

    return pipelineCacheInfo->m_pipeline;
  }

  void GraphicsVulkan::build(shared_ptr<UniformBuffer> buffer)
  {
    vkUniformBufferData* uniformBufferData = new vkUniformBufferData();
    buffer->setGraphicsData(uniformBufferData);

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = buffer->getSize();
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(m_device, &bufferInfo, nullptr, &uniformBufferData->m_buffer);

    // Create The Buffer Memory
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(m_device, uniformBufferData->m_buffer, &memoryRequirements);

    VkDeviceSize alignedSize = memoryRequirements.size;
    if (alignedSize % memoryRequirements.alignment)
    {
      alignedSize += memoryRequirements.alignment - (alignedSize % memoryRequirements.alignment);
    }

    // allocate memory
    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;

    for (uint32_t idx = 0; idx < m_memoryFlags.size(); idx++) {
      if ((memoryRequirements.memoryTypeBits & (1 << idx)) &&
        (m_memoryFlags[idx] & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
        (m_memoryFlags[idx] & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        memoryAllocateInfo.memoryTypeIndex = idx;
        break;
      }
    }

    vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &uniformBufferData->m_deviceMemory);

    void *ptr;
    vkMapMemory(m_device, uniformBufferData->m_deviceMemory, 0, VK_WHOLE_SIZE, 0, &ptr);

    vkBindBufferMemory(m_device, uniformBufferData->m_buffer, uniformBufferData->m_deviceMemory, 0);
    uniformBufferData->m_data = reinterpret_cast<uint8_t *>(ptr);
  }

  void GraphicsVulkan::updateUniformData(shared_ptr<UniformBuffer> buffer, size_t offset, float* data, size_t size)
  {
    vkUniformBufferData* bufferData = (vkUniformBufferData*)buffer->getGraphicsData();
    uint8_t* ptr = bufferData->m_data + offset;
    memcpy(ptr, data, size);
  }

  void GraphicsVulkan::updateUniformData(shared_ptr<UniformBuffer> buffer, size_t offset, uint8_t* data, size_t size)
  {
    vkUniformBufferData* bufferData = (vkUniformBufferData*)buffer->getGraphicsData();
    uint8_t* ptr = bufferData->m_data + offset;
    memcpy(ptr, data, size);
  }

  void GraphicsVulkan::build(shared_ptr<View> view, size_t numFrames)
  {
    vkViewData* viewData = new vkViewData();
    viewData->m_swapchain = VK_NULL_HANDLE;
    viewData->m_depthImage = VK_NULL_HANDLE;

    view->setGraphicsData(viewData);

    vec2 size;
    view->getViewportSize(size);
    viewData->m_currentWidth = (uint32_t)size.x;
    viewData->m_currentHeight = (uint32_t)size.y;

    if (view->getType() == View::SCREEN)
    {
      createOnscreenView(view, viewData, numFrames);
    } 
    else if (view->getType() == View::SHADOW || view->getType() == View::SHADOW_CUBE)
    {
      createShadowView(view, viewData, numFrames);
      m_shadowViewData.push_back(viewData);
    }

    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = NULL;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = m_commandQueueFamily;
    vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &viewData->m_commandPool);

    //m_primaryCommandBuffer = new VkCommandBuffer[numFrames];
    VkCommandBufferAllocateInfo commandBufferInfo = {};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.pNext = NULL;
    commandBufferInfo.commandBufferCount = (uint32_t)numFrames;
    commandBufferInfo.commandPool = m_primaryCommandPool;
    commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkAllocateCommandBuffers(m_device, &commandBufferInfo, &viewData->m_commandBuffer[0]);
  }

  void GraphicsVulkan::createOnscreenView(shared_ptr<View> view, vkViewData* viewData, size_t numFrames)
  {
    // Create the surface
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = m_hinstance;
    surfaceInfo.hwnd = m_window;
    vkCreateWin32SurfaceKHR(m_instance, &surfaceInfo, nullptr, &(viewData->m_surface));

    vector<VkSurfaceFormatKHR> formats;
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, viewData->m_surface, &count, nullptr);
    formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, viewData->m_surface, &count, formats.data());
    viewData->m_surfaceFormat = formats[0];

    createOnscreenDepthBuffer(view, viewData);
    createOnscreenRenderPass(view, viewData);
    resize(m_onscreenView, viewData->m_currentWidth, viewData->m_currentHeight);

    // Create Back Buffers
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < numFrames; i++) {
      vkBackBuffer backBuffer = {};
      vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &backBuffer.m_acquireSemaphore);
      vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &backBuffer.m_renderSemaphore);
      vkCreateFence(m_device, &fenceInfo, nullptr, &backBuffer.m_presentFence);
      vkCreateFence(m_device, &fenceInfo, nullptr, &backBuffer.m_renderFence);

      viewData->m_backBuffers.push(backBuffer);
    }

    //createGBufferPass(view, viewData);
  }

  void GraphicsVulkan::createGBufferAttachments(shared_ptr<View> view, vkViewData* viewData)
  {
    // (World space) Positions
    createGBufferAttachment(view, viewData, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &viewData->m_gBuffer.m_position);

    // (World space) Normals
    createGBufferAttachment(view, viewData, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,  &viewData->m_gBuffer.m_normal);

    // Albedo (color)
    createGBufferAttachment(view, viewData, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &viewData->m_gBuffer.m_albedo);

    // Metallic/Roughness/Flags
    createGBufferAttachment(view, viewData, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &viewData->m_gBuffer.m_metallicRoughnessFlags);

    // Emissive
    createGBufferAttachment(view, viewData, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &viewData->m_gBuffer.m_emissive);

    // Depth attachment
    createGBufferAttachment(view, viewData, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &viewData->m_gBuffer.m_depth);
  }

  void GraphicsVulkan::createGBufferAttachment(shared_ptr<View> view, vkViewData* viewData, VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment *attachment)
  {
    VkImageAspectFlags aspectMask = 0;
    VkImageLayout imageLayout;

    attachment->m_format = format;

    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    {
      aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = nullptr;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = format;
    image.extent.width = viewData->m_extent.width;
    image.extent.height = viewData->m_extent.height;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = nullptr;
    VkMemoryRequirements memReqs;

    vkCreateImage(m_device, &image, nullptr, &attachment->m_image);
    vkGetImageMemoryRequirements(m_device, attachment->m_image, &memReqs);
    memoryAllocateInfo.allocationSize = memReqs.size;
    memory_type_from_properties(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryAllocateInfo.memoryTypeIndex);
    vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &attachment->m_memory);
    vkBindImageMemory(m_device, attachment->m_image, attachment->m_memory, 0);

    VkImageViewCreateInfo imageView = {};
    imageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageView.pNext = nullptr;
    imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageView.format = format;
    imageView.subresourceRange = {};
    imageView.subresourceRange.aspectMask = aspectMask;
    imageView.subresourceRange.baseMipLevel = 0;
    imageView.subresourceRange.levelCount = 1;
    imageView.subresourceRange.baseArrayLayer = 0;
    imageView.subresourceRange.layerCount = 1;
    imageView.image = attachment->m_image;
    vkCreateImageView(m_device, &imageView, nullptr, &attachment->m_view);

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    vkCreateSampler(m_device, &samplerInfo, nullptr, &attachment->m_textureSampler);
  }

  void GraphicsVulkan::createShadowView(shared_ptr<View> view, vkViewData* viewData, size_t numFrames)
  {
    VkFormat fbColorFormat = VK_FORMAT_R8G8B8A8_UNORM;

    viewData->m_extent.width = viewData->m_currentWidth;
    viewData->m_extent.height = viewData->m_currentHeight;

    viewData->m_viewport.x = 0.0f;
    viewData->m_viewport.y = 0.0f;
    viewData->m_viewport.width = static_cast<float>(viewData->m_extent.width);
    viewData->m_viewport.height = static_cast<float>(viewData->m_extent.height);
    viewData->m_viewport.minDepth = 0.0f;
    viewData->m_viewport.maxDepth = 1.0f;

    viewData->m_scissor.offset = { 0, 0 };
    viewData->m_scissor.extent = viewData->m_extent;

    // For shadow mapping we only need a depth attachment
    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = nullptr;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.extent.width = viewData->m_currentWidth;
    image.extent.height = viewData->m_currentHeight;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 6;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.format = VK_FORMAT_D32_SFLOAT;
    image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    vkCreateImage(m_device, &image, nullptr, &viewData->m_depthImage);

    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = nullptr;
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, viewData->m_depthImage, &memReqs);
    memoryAllocateInfo.allocationSize = memReqs.size;
    memory_type_from_properties(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryAllocateInfo.memoryTypeIndex);
    vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &viewData->m_depthMemory);
    vkBindImageMemory(m_device, viewData->m_depthImage, viewData->m_depthMemory, 0);

    VkImageViewCreateInfo depthStencilView = {};
    depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthStencilView.pNext = nullptr;
    depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    depthStencilView.format = VK_FORMAT_D32_SFLOAT;
    depthStencilView.subresourceRange = {};
    depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthStencilView.subresourceRange.baseMipLevel = 0;
    depthStencilView.subresourceRange.levelCount = 1;
    depthStencilView.subresourceRange.baseArrayLayer = 0;
    depthStencilView.subresourceRange.layerCount = 6;
    depthStencilView.image = viewData->m_depthImage;
    vkCreateImageView(m_device, &depthStencilView, nullptr, &viewData->m_depthView);

    VkSamplerCreateInfo sampler = {};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.pNext = nullptr;
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV = sampler.addressModeU;
    sampler.addressModeW = sampler.addressModeU;
    sampler.mipLodBias = 0.0f;
    sampler.maxAnisotropy = 1.0f;
    sampler.minLod = 0.0f;
    sampler.maxLod = 1.0f;
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    vkCreateSampler(m_device, &sampler, nullptr, &viewData->m_shadowSampler);

    createShadowRenderPass(view, viewData);

    for (int i = 0; i < numFrames; i++) {

      // Create frame buffer
      VkFramebufferCreateInfo fbufCreateInfo = {};
      fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      fbufCreateInfo.pNext = nullptr;
      fbufCreateInfo.renderPass = viewData->m_renderPass;
      fbufCreateInfo.attachmentCount = 1;
      fbufCreateInfo.pAttachments = &viewData->m_depthView;
      fbufCreateInfo.width = viewData->m_currentWidth;
      fbufCreateInfo.height = viewData->m_currentHeight;
      fbufCreateInfo.layers = 6;

      VkFramebuffer fb;
      VkResult result = vkCreateFramebuffer(m_device, &fbufCreateInfo, nullptr, &fb);
      viewData->m_framebuffers.push_back(fb);

      // Create Back Buffers
      VkSemaphoreCreateInfo semaphoreInfo = {};
      semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

      VkFenceCreateInfo fenceInfo = {};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

      vkBackBuffer backBuffer = {};
      vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &backBuffer.m_acquireSemaphore);
      vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &backBuffer.m_renderSemaphore);
      vkCreateFence(m_device, &fenceInfo, nullptr, &backBuffer.m_renderFence);
      viewData->m_backBuffers.push(backBuffer);
    }

  }

  void GraphicsVulkan::createOnscreenDepthBuffer(shared_ptr<View> view, vkViewData* viewData)
  {
    if (viewData->m_depthImage != VK_NULL_HANDLE)
    {
      vkDestroyImageView(m_device, viewData->m_depthView, nullptr);
      vkFreeMemory(m_device, viewData->m_depthMemory, nullptr);
      vkDestroyImage(m_device, viewData->m_depthImage, nullptr);
    }

    VkImageCreateInfo imageCreateInfo = {};

    VkCommandBuffer commandBuffer = beginOneTimeCommands();

    viewData->m_depthFormat = VK_FORMAT_D32_SFLOAT;
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, viewData->m_depthFormat, &formatProperties);
    if (formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) 
    {
      imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    }
    else if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) 
    {
      imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    }
    else 
    {
      /* Try other depth formats? */
      std::cout << "depth_format " << viewData->m_depthFormat << " Unsupported.\n";
      exit(-1);
    }

    
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = NULL;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = viewData->m_depthFormat;
    imageCreateInfo.extent.width = viewData->m_currentWidth;
    imageCreateInfo.extent.height = viewData->m_currentHeight;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = NULL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageCreateInfo.flags = 0;

    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = NULL;
    memoryAllocateInfo.allocationSize = 0;
    memoryAllocateInfo.memoryTypeIndex = 0;

    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext = NULL;
    viewCreateInfo.image = VK_NULL_HANDLE;
    viewCreateInfo.format = viewData->m_depthFormat;
    viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; // | VK_IMAGE_ASPECT_STENCIL_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.flags = 0;

    VkMemoryRequirements memoryRequirements;

    /* Create image */
    vkCreateImage(m_device, &imageCreateInfo, NULL, &viewData->m_depthImage);
    vkGetImageMemoryRequirements(m_device, viewData->m_depthImage, &memoryRequirements);

    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    /* Use the memory properties to determine the type of memory required */
    memory_type_from_properties(memoryRequirements.memoryTypeBits, 0, &memoryAllocateInfo.memoryTypeIndex);

    /* Allocate memory */
    vkAllocateMemory(m_device, &memoryAllocateInfo, NULL, &viewData->m_depthMemory);

    /* Bind memory */
    vkBindImageMemory(m_device, viewData->m_depthImage, viewData->m_depthMemory, 0);

    /* Set the image layout to depth stencil optimal */
    setImageLayout(commandBuffer, viewData->m_depthImage, viewCreateInfo.subresourceRange.aspectMask,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    /* Create image view */
    viewCreateInfo.image = viewData->m_depthImage;
    vkCreateImageView(m_device, &viewCreateInfo, NULL, &viewData->m_depthView);

    endOneTimeCommands(commandBuffer);
  }

  void GraphicsVulkan::setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout) {

    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext = NULL;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = 0;
    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange.aspectMask = aspectMask;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (oldImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) 
    {
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      srcStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
    {
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      destStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) 
    {
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      destStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
    {
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      srcStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if (oldImageLayout == VK_IMAGE_LAYOUT_PREINITIALIZED) 
    {
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
      srcStages = VK_PIPELINE_STAGE_HOST_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
    {
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      destStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) 
    {
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      destStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
    {
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      destStages = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }



    vkCmdPipelineBarrier(commandBuffer, srcStages, destStages, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
  }

  void GraphicsVulkan::createOnscreenRenderPass(shared_ptr<View> view, vkViewData* viewData)
  {
    VkAttachmentDescription attachmentDescs[8];
    attachmentDescs[0].format = viewData->m_surfaceFormat.format;
    attachmentDescs[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachmentDescs[0].flags = 0;

    attachmentDescs[1].format = viewData->m_depthFormat;
    attachmentDescs[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescs[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescs[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescs[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentDescs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentDescs[1].flags = 0;

    // Init attachment properties
    for (uint32_t i = 2; i < 8; ++i)
    {
      attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
      attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachmentDescs[i].flags = 0;
      if (i == 7)
      {
        attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      }
      else
      {
        attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }
    }

    // Formats
    attachmentDescs[2].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachmentDescs[3].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachmentDescs[4].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachmentDescs[5].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachmentDescs[6].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachmentDescs[7].format = VK_FORMAT_D32_SFLOAT;

    std::vector<VkAttachmentReference> gBufferColorReferences;
    gBufferColorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    gBufferColorReferences.push_back({ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    gBufferColorReferences.push_back({ 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    gBufferColorReferences.push_back({ 5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    gBufferColorReferences.push_back({ 6, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    VkAttachmentReference gBufferDepthReference = {};
    gBufferDepthReference.attachment = 7;
    gBufferDepthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpasses[3];
    int subpassIndex = 0;
    if (m_depthPrepass) {
      subpasses[subpassIndex].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpasses[subpassIndex].flags = 0;
      subpasses[subpassIndex].inputAttachmentCount = 0;
      subpasses[subpassIndex].pInputAttachments = NULL;
      subpasses[subpassIndex].colorAttachmentCount = 0;
      subpasses[subpassIndex].pColorAttachments = NULL;
      subpasses[subpassIndex].pResolveAttachments = NULL;
      subpasses[subpassIndex].pDepthStencilAttachment = &gBufferDepthReference;
      subpasses[subpassIndex].preserveAttachmentCount = 0;
      subpasses[subpassIndex].pPreserveAttachments = NULL;
      subpassIndex++;
    }

    subpasses[subpassIndex].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[subpassIndex].flags = 0;
    subpasses[subpassIndex].inputAttachmentCount = 0;
    subpasses[subpassIndex].pInputAttachments = NULL;
    subpasses[subpassIndex].colorAttachmentCount = 5;
    subpasses[subpassIndex].pColorAttachments = gBufferColorReferences.data();
    subpasses[subpassIndex].pResolveAttachments = NULL;
    subpasses[subpassIndex].pDepthStencilAttachment = &gBufferDepthReference;
    subpasses[subpassIndex].preserveAttachmentCount = 0;
    subpasses[subpassIndex].pPreserveAttachments = NULL;
    subpassIndex++;

    std::vector<VkAttachmentReference> gBufferInputReferences;
    gBufferInputReferences.push_back({ 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    gBufferInputReferences.push_back({ 3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    gBufferInputReferences.push_back({ 4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    gBufferInputReferences.push_back({ 5, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    gBufferInputReferences.push_back({ 6, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    subpasses[subpassIndex].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[subpassIndex].flags = 0;
    subpasses[subpassIndex].inputAttachmentCount = 5;
    subpasses[subpassIndex].pInputAttachments = gBufferInputReferences.data();
    subpasses[subpassIndex].colorAttachmentCount = 1;
    subpasses[subpassIndex].pColorAttachments = &colorReference;
    subpasses[subpassIndex].pResolveAttachments = NULL;
    subpasses[subpassIndex].pDepthStencilAttachment = &depthReference;
    subpasses[subpassIndex].preserveAttachmentCount = 0;
    subpasses[subpassIndex].pPreserveAttachments = NULL;
    subpassIndex++;

    array<VkSubpassDependency, 2> subpassDependency;
    subpassDependency[0].srcSubpass = 0;
    subpassDependency[0].dstSubpass = 1;
    subpassDependency[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subpassDependency[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependency[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    subpassDependency[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    if (m_depthPrepass)
    {
      subpassDependency[1].srcSubpass = 1;
      subpassDependency[1].dstSubpass = 2;
      subpassDependency[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      subpassDependency[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      subpassDependency[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      subpassDependency[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      subpassDependency[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 8;
    render_pass_info.pAttachments = attachmentDescs;
    render_pass_info.subpassCount = subpassIndex;
    render_pass_info.pSubpasses = subpasses;
    render_pass_info.dependencyCount = (uint32_t)(subpassIndex-1);
    render_pass_info.pDependencies = subpassDependency.data();

    vkCreateRenderPass(m_device, &render_pass_info, nullptr, &viewData->m_renderPass);
  }

  void GraphicsVulkan::createShadowRenderPass(shared_ptr<View> view, vkViewData* viewData)
  {
    VkAttachmentDescription attachmentDescription = {};
    attachmentDescription.format = VK_FORMAT_D32_SFLOAT;
    attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 0;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthReference;

    array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = nullptr;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachmentDescription;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassCreateInfo.pDependencies = dependencies.data();

    vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &viewData->m_renderPass);
  }

  void GraphicsVulkan::resize(shared_ptr<View> view, uint32_t width, uint32_t height)
  {
    vkViewData* viewData = (vkViewData*)view->getGraphicsData();
    if (viewData->m_swapchain == nullptr || width != viewData->m_currentWidth || height != viewData->m_currentHeight)
    {
      VkSurfaceCapabilitiesKHR surfaceCapabilities;
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, viewData->m_surface, &surfaceCapabilities);

      VkExtent2D extent = surfaceCapabilities.currentExtent;
      // use the hints
      if (extent.width == (uint32_t)-1) {
        extent.width = width;
        extent.height = height;
      }
      // clamp width; to protect us from broken hints?
      if (extent.width < surfaceCapabilities.minImageExtent.width)
        extent.width = surfaceCapabilities.minImageExtent.width;
      else if (extent.width > surfaceCapabilities.maxImageExtent.width)
        extent.width = surfaceCapabilities.maxImageExtent.width;
      // clamp height
      if (extent.height < surfaceCapabilities.minImageExtent.height)
        extent.height = surfaceCapabilities.minImageExtent.height;
      else if (extent.height > surfaceCapabilities.maxImageExtent.height)
        extent.height = surfaceCapabilities.maxImageExtent.height;

      if (viewData->m_extent.width == extent.width && viewData->m_extent.height == extent.height)
        return;

      uint32_t image_count = 2;
      if (image_count < surfaceCapabilities.minImageCount)
        image_count = surfaceCapabilities.minImageCount;
      else if (image_count > surfaceCapabilities.maxImageCount)
        image_count = surfaceCapabilities.maxImageCount;

      assert(surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
      assert(surfaceCapabilities.supportedTransforms & surfaceCapabilities.currentTransform);
      assert(surfaceCapabilities.supportedCompositeAlpha & (VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR));
      VkCompositeAlphaFlagBitsKHR composite_alpha =
        (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) ?
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

      vector<VkPresentModeKHR> presentModes;
      uint32_t count = 0;
      vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, viewData->m_surface, &count, nullptr);

      presentModes.resize(count);
      vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, viewData->m_surface, &count, presentModes.data());

      // FIFO is the only mode universally supported
      VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
      bool vsync = false;
      for (auto m : presentModes) {
        if ((vsync && m == VK_PRESENT_MODE_MAILBOX_KHR) ||
          (!vsync && m == VK_PRESENT_MODE_IMMEDIATE_KHR)) {
          mode = m;
          break;
        }
      }

      VkBool32 supported;
      vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_presentQueueFamily, viewData->m_surface, &supported);
      if (supported == VK_FALSE)
      {
        std::cout << "Present Queue " << m_presentQueueFamily << " Unsupported.\n";
        exit(-1);
      }

      VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
      swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
      swapchainCreateInfo.surface = viewData->m_surface;
      swapchainCreateInfo.minImageCount = image_count;
      swapchainCreateInfo.imageFormat = viewData->m_surfaceFormat.format;
      swapchainCreateInfo.imageColorSpace = viewData->m_surfaceFormat.colorSpace;
      swapchainCreateInfo.imageExtent = extent;
      swapchainCreateInfo.imageArrayLayers = 1;
      swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

      std::vector<uint32_t> queueFamilies(1, m_commandQueueFamily);
      if (m_commandQueueFamily != m_presentQueueFamily) 
      {
        queueFamilies.push_back(m_presentQueueFamily);

        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = (uint32_t)queueFamilies.size();
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilies.data();
      }
      else 
      {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      }

      swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;;
      swapchainCreateInfo.compositeAlpha = composite_alpha;
      swapchainCreateInfo.presentMode = mode;
      swapchainCreateInfo.clipped = true;
      swapchainCreateInfo.oldSwapchain = viewData->m_swapchain;

      vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &viewData->m_swapchain);
      viewData->m_extent = extent;
      viewData->m_currentWidth = extent.width;
      viewData->m_currentHeight = extent.height;

      // destroy the old swapchain
      if (swapchainCreateInfo.oldSwapchain != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
        detachSwapchain(view, viewData);
        vkDestroySwapchainKHR(m_device, swapchainCreateInfo.oldSwapchain, nullptr);
      }
      createOnscreenDepthBuffer(view, viewData);
      createGBufferAttachments(view, viewData);
      attachSwapchain(view, viewData);

      //createGBufferPass(view, viewData);
    }
  }

  void GraphicsVulkan::attachSwapchain(shared_ptr<View> view, vkViewData* viewData)
  {
    // Update Viewport data
    viewData->m_viewport.x = 0.0f;
    viewData->m_viewport.y = 0.0f;
    viewData->m_viewport.width = static_cast<float>(viewData->m_extent.width);
    viewData->m_viewport.height = static_cast<float>(viewData->m_extent.height);
    viewData->m_viewport.minDepth = 0.0f;
    viewData->m_viewport.maxDepth = 1.0f;

    viewData->m_scissor.offset = { 0, 0 };
    viewData->m_scissor.extent = viewData->m_extent;

    // get swapchain images
    uint32_t count = 0;
    vkGetSwapchainImagesKHR(m_device, viewData->m_swapchain, &count, nullptr);
    viewData->m_images.resize(count);
    vkGetSwapchainImagesKHR(m_device, viewData->m_swapchain, &count, viewData->m_images.data());

    viewData->m_imageViews.reserve(viewData->m_images.size());
    viewData->m_framebuffers.reserve(viewData->m_images.size());
    for (auto img : viewData->m_images) {
      VkImageViewCreateInfo imageViewCreateInfo = {};
      imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      imageViewCreateInfo.image = img;
      imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      imageViewCreateInfo.format = viewData->m_surfaceFormat.format;
      imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      imageViewCreateInfo.subresourceRange.levelCount = 1;
      imageViewCreateInfo.subresourceRange.layerCount = 1;

      VkImageView view;
      vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &view);
      viewData->m_imageViews.push_back(view);

      std::array<VkImageView, 8> attachments = {
        view,
        viewData->m_depthView,
        viewData->m_gBuffer.m_position.m_view,
        viewData->m_gBuffer.m_normal.m_view,
        viewData->m_gBuffer.m_albedo.m_view,
        viewData->m_gBuffer.m_metallicRoughnessFlags.m_view,
        viewData->m_gBuffer.m_emissive.m_view,
        viewData->m_gBuffer.m_depth.m_view
      };

      VkFramebufferCreateInfo framebufferCreateInfo = {};
      framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferCreateInfo.renderPass = viewData->m_renderPass;
      framebufferCreateInfo.attachmentCount = (uint32_t)attachments.size();
      framebufferCreateInfo.pAttachments = attachments.data();
      framebufferCreateInfo.width = viewData->m_extent.width;
      framebufferCreateInfo.height = viewData->m_extent.height;
      framebufferCreateInfo.layers = 1;

      VkFramebuffer fb;
      vkCreateFramebuffer(m_device, &framebufferCreateInfo, nullptr, &fb);
      viewData->m_framebuffers.push_back(fb);
    }
  }

  void GraphicsVulkan::detachSwapchain(shared_ptr<View> view, vkViewData* viewData)
  {
    for (auto fb : viewData->m_framebuffers)
    {
      vkDestroyFramebuffer(m_device, fb, nullptr);
    }

    for (auto view : viewData->m_imageViews)
    {
      vkDestroyImageView(m_device, view, nullptr);
    }

    viewData->m_framebuffers.clear();
    viewData->m_imageViews.clear();
    viewData->m_images.clear();
  }

  vector<char> GraphicsVulkan::readFile(const string& filename) {
    ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
  }

  void GraphicsVulkan::initializeInstance()
  {
    const char filename[] = "vulkan-1.dll";
    HMODULE mod;
    PFN_vkGetInstanceProcAddr get_proc = nullptr;
    bool verbose = true;

    mod = LoadLibrary(L"vulkan-1.dll");
    if (mod) {
      get_proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(
        mod, "vkGetInstanceProcAddr"));
    }

    if (!mod || !get_proc) {
      std::stringstream ss;
      ss << "failed to load " << filename;

      if (mod)
        FreeLibrary(mod);

      throw std::runtime_error(ss.str());
    }

    //hmodule_ = mod;

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "RenderLab";
    app_info.applicationVersion = 0;
    app_info.apiVersion = VK_API_VERSION_1_0;

#ifdef _DEBUG
    bool validate = true;
#else
    bool validate = false;
#endif
    // require generic WSI extensions
    m_instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    m_deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    //m_deviceExtensions.push_back("VK_NV_viewport_array2");
    //m_deviceExtensions.push_back("VK_NV_glsl_shader");

    // require "standard" validation layers
    if (validate)
    {
      m_instanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
      m_instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    m_instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledLayerCount = static_cast<uint32_t>(m_instanceLayers.size());
    if (validate)
    {
      instance_info.ppEnabledLayerNames = &m_instanceLayers[0];
    }
    else
    {
      instance_info.ppEnabledLayerNames = nullptr;
    }
    instance_info.enabledExtensionCount = static_cast<uint32_t>(m_instanceExtensions.size());
    instance_info.ppEnabledExtensionNames = &m_instanceExtensions[0];

    vkCreateInstance(&instance_info, nullptr, &m_instance);

    // get physical devices
    std::vector<VkPhysicalDevice> physical_devices;
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    physical_devices.resize(count);
    vkEnumeratePhysicalDevices(m_instance, &count, physical_devices.data());

    m_physicalDevice = VK_NULL_HANDLE;
    for (auto physical_device : physical_devices) {
      if (!has_all_device_extensions(physical_device))
        continue;

      // get queue properties
      vector<VkQueueFamilyProperties> queues;
      vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);
      queues.resize(count);
      vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queues.data());

      int commandQueueFamily = -1;
      int presentQueueFamily = -1;

      for (uint32_t i = 0; i < queues.size(); i++) {
        const VkQueueFamilyProperties &q = queues[i];

        // requires only GRAPHICS for now
        const VkFlags gameQueueFlags = VK_QUEUE_GRAPHICS_BIT;
        if (commandQueueFamily < 0 && (q.queueFlags & gameQueueFlags) == gameQueueFlags)
        {
          commandQueueFamily = i;
        }

        // present queue must support the surface
        if (presentQueueFamily < 0 && (vkGetPhysicalDeviceWin32PresentationSupportKHR(physical_device, i) == VK_TRUE))
        { 
          presentQueueFamily = i;
        }
          

        if (commandQueueFamily >= 0 && presentQueueFamily >= 0)
        {
          break;
        }
      }

      if (commandQueueFamily >= 0 && presentQueueFamily >= 0) {
        m_physicalDevice = physical_device;
        m_commandQueueFamily = commandQueueFamily;
        m_presentQueueFamily = presentQueueFamily;
        break;
      }
    }

    if (m_physicalDevice == VK_NULL_HANDLE)
      throw std::runtime_error("failed to find any capable Vulkan physical device");
  }

  void GraphicsVulkan::initializeVirtualDevice()
  {
    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    const vector<float> queuePriorities(1, 0.0f);
    array<VkDeviceQueueCreateInfo, 2> queueInfo = {};
    queueInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo[0].queueFamilyIndex = m_commandQueueFamily;
    queueInfo[0].queueCount = 1;
    queueInfo[0].pQueuePriorities = queuePriorities.data();

    if (m_commandQueueFamily != m_presentQueueFamily) 
    {
      queueInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueInfo[1].queueFamilyIndex = m_presentQueueFamily;
      queueInfo[1].queueCount = 1;
      queueInfo[1].pQueuePriorities = queuePriorities.data();

      deviceInfo.queueCreateInfoCount = 2;
    }
    else 
    {
      deviceInfo.queueCreateInfoCount = 1;
    }

    deviceInfo.pQueueCreateInfos = queueInfo.data();
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

    // disable all features
    VkPhysicalDeviceFeatures features = {};
    features.geometryShader = 1;
    features.imageCubeArray = 1;

    deviceInfo.pEnabledFeatures = &features;

    vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device);
  }

  void GraphicsVulkan::initializeProperties()
  {
    // Get the device properties
    vkGetPhysicalDeviceProperties(m_physicalDevice, &m_physicalDeviceProperties);

    // Get the memory properties
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);
    m_memoryFlags.reserve(m_memoryProperties.memoryTypeCount);
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; i++)
    {
      m_memoryFlags.push_back(m_memoryProperties.memoryTypes[i].propertyFlags);
    }
  }

  void GraphicsVulkan::initializeQueues(uint32_t numFrames)
  {
    vkGetDeviceQueue(m_device, m_commandQueueFamily, 0, &m_commandQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);

    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = NULL;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = m_commandQueueFamily;
    vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_primaryCommandPool);

    //m_primaryCommandBuffer = new VkCommandBuffer[numFrames];
    VkCommandBufferAllocateInfo commandBufferInfo = {};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.pNext = NULL;
    commandBufferInfo.commandBufferCount = numFrames;
    commandBufferInfo.commandPool = m_primaryCommandPool;
    commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkAllocateCommandBuffers(m_device, &commandBufferInfo, &m_primaryCommandBuffer[0]);

    VkQueryPoolCreateInfo queryPoolCreateInfo;
    queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolCreateInfo.pNext = nullptr;
    queryPoolCreateInfo.flags = 0;
    queryPoolCreateInfo.queryCount = 3;
    queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolCreateInfo.pipelineStatistics = 0;
    vkCreateQueryPool(m_device, &queryPoolCreateInfo, nullptr, &m_queryPool);
  }

  bool GraphicsVulkan::has_all_device_extensions(VkPhysicalDevice physicalDevice)
  {
    // get device extensions
    vector<VkExtensionProperties> extensionProperties;
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);
    extensionProperties.resize(count);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, extensionProperties.data());

    set<string> extensionNames;
    for (const auto &extension : extensionProperties)
    {
      extensionNames.insert(extension.extensionName);
    }

    // all listed device extensions are required
    for (const auto &name : m_deviceExtensions) {
      if (extensionNames.find(name) == extensionNames.end())
      {
        return false;
      }
    }

    return true;
  }

  bool  GraphicsVulkan::memory_type_from_properties(uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex) {
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; i++) {
      if ((typeBits & 1) == 1) {
        // Type is available, does it match user properties?
        if ((m_memoryProperties.memoryTypes[i].propertyFlags &
          requirements_mask) == requirements_mask) {
          *typeIndex = i;
          return true;
        }
      }
      typeBits >>= 1;
    }
    // No memory types matched, return failure
    return false;
  }

  VkCommandBuffer GraphicsVulkan::beginOneTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_primaryCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
  }

  void GraphicsVulkan::endOneTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_commandQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_commandQueue);

    vkFreeCommandBuffers(m_device, m_primaryCommandPool, 1, &commandBuffer);
  }
}