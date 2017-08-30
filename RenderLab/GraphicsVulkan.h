#pragma once
#include "Graphics.h"
#include "Mesh.h"
#include "Material.h"

#include <string>
#include <memory>
#include <vector>
#include <queue>
#include <iostream>
#include <sstream>
#include <set>
#include <array>
#include <stdio.h>
#include <fstream>
#include <cassert>
#include <map>

#include <vulkan/vulkan.h>

using std::string;
using std::shared_ptr;
using std::vector;
using std::queue;
using std::set;
using std::array;
using std::ifstream;
using std::map;

namespace RenderLab
{
  class GraphicsVulkan :
    public Graphics
  {
  public:
    GraphicsVulkan(string name, HINSTANCE hinstance, HWND window);
    ~GraphicsVulkan();

    void initialize(uint32_t numFrames);
    void build(shared_ptr<Mesh> mesh, vector<shared_ptr<UniformBuffer>>& frameDataUniformBuffers, vector<shared_ptr<UniformBuffer>>& objectDataUniformBuffers, size_t numFrames, vector<shared_ptr<LightComponent>>& lightComponents);
    void build(shared_ptr<Material> material, vector<shared_ptr<UniformBuffer>>& frameDataUniformBuffers, vector<shared_ptr<UniformBuffer>>& objectDataUniformBuffers, size_t numFrames);
    size_t getBufferAlignment();
    void build(shared_ptr<UniformBuffer> buffer);
    void build(shared_ptr<View> view, size_t numFrames);
    void resize(shared_ptr<View> view, uint32_t width, uint32_t height);
    void setDepthBias(float constant, float slope);

    void updateUniformData(shared_ptr<UniformBuffer> buffer, size_t offset, float* data, size_t size);
    void updateUniformData(shared_ptr<UniformBuffer> buffer, size_t offset, uint8_t* data, size_t size);

    uint32_t            acquireBackBuffer(shared_ptr<View> view);
    void                renderBegin(shared_ptr<View> view, shared_ptr<View> lastView, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer, uint32_t frameIndex);
    void                renderEnd(shared_ptr<View> view, shared_ptr<View> lastView, bool lastLight, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer, uint32_t frameIndex);
    void                swapBackBuffer(shared_ptr<View> view, uint32_t frameIndex);
    void                bindPipeline(shared_ptr<Mesh> mesh, shared_ptr<View> view, uint32_t frameIndex, bool depthPrepass);
    void                endDepthPrepass(shared_ptr<View> view, uint32_t frameIndex);
    void                render(shared_ptr<Mesh> mesh, uint32_t meshOffset, shared_ptr<View> view, uint32_t frameIndex, bool depthPrepass);
    float				        getGPUFrameTime();
    float				        getGPUFrameTime2();

  private:
    vector<const char *>  m_instanceLayers;
    vector<const char *>  m_instanceExtensions;
    vector<const char *>  m_deviceExtensions;

    VkInstance            m_instance;
    VkPhysicalDevice      m_physicalDevice;

    uint32_t              m_commandQueueFamily;
    uint32_t              m_presentQueueFamily;
    VkDevice              m_device;
    VkQueue               m_commandQueue;
    VkQueue               m_presentQueue;

    VkCommandPool         m_primaryCommandPool;
    VkCommandBuffer       m_primaryCommandBuffer[2];

    VkPhysicalDeviceProperties          m_physicalDeviceProperties;
    vector<VkMemoryPropertyFlags>       m_memoryFlags;
    VkPhysicalDeviceMemoryProperties    m_memoryProperties;

    VkQueryPool           m_queryPool;
	  uint64_t							m_currentTimestamp[3];

    // Per mesh graphics data
    struct vkMeshData
    {
      VkVertexInputBindingDescription           m_vertexInputBinding;
      vector<VkVertexInputAttributeDescription> m_vertexInputAttributes;
      VkPipelineVertexInputStateCreateInfo      m_vertexInputState;
      VkPipelineInputAssemblyStateCreateInfo    m_inputAssemblyState;
      VkDeviceSize                              m_vertexBufferSize;
      unsigned int                              m_stride;
      VkIndexType                               m_indexType;
      //vector<VkDrawIndexedIndirectCommand>      m_draw_commands;

      VkBuffer                                  m_vertexBuffer;
      VkBuffer                                  m_indexBuffer;
      VkDeviceMemory                            m_memory;
      VkDeviceSize                              m_indexBufferMemoryOffset;

      map<shared_ptr<View>, VkPipeline*>        m_pipelines;
      VkPipeline*                               m_depthPrepassPipelines;
    };

    // Per texture graphics data
    struct vkTextureData
    {
      VkImage               m_stagingImage;
      VkDeviceMemory        m_stagingImageMemory;
      VkImage               m_image;
      VkDeviceMemory        m_imageMemory;
      VkImageView           m_imageView;
      VkSampler             m_textureSampler;
    };

    // Per material graphics data
    struct vkMaterialData
    {
      vector<char>          m_vertexShaderCode;
      vector<char>          m_geometryShaderCode;
      vector<char>          m_fragmentShaderCode;

      VkShaderModule        m_vertexShader;
      VkShaderModule        m_geometryShader;
      VkShaderModule        m_fragmentShader;

      VkDescriptorSet*       m_descriptorSet;
      VkDescriptorSetLayout* m_descriptorSetLayout;
      VkPipelineLayout*      m_pipelineLayout;
      VkDescriptorPool*      m_descriptorPool;
    };

    struct vkLightPushContants
    {
      vec4 m_lightColor;
      vec4 m_lightPosition;
    };

    // Per buffer graphics data
    struct vkUniformBufferData
    {
      VkDeviceMemory                m_deviceMemory;
      VkBuffer                      m_buffer;
      uint8_t*                      m_data;
    };

    // Data needed for back buffers
    struct vkBackBuffer {
      uint32_t    m_imageIndex;
      VkSemaphore m_acquireSemaphore;
      VkSemaphore m_renderSemaphore;
      VkFence     m_presentFence;
      VkFence     m_renderFence;
    };

    struct FrameBufferAttachment {
      VkImage         m_image;
      VkDeviceMemory  m_memory;
      VkImageView     m_view;
      VkFormat        m_format;
      VkSampler       m_textureSampler;
    };

    struct FrameBuffer {
      VkFramebuffer         m_frameBuffer;
      FrameBufferAttachment m_position;
      FrameBufferAttachment m_normal;
      FrameBufferAttachment m_albedo;
      FrameBufferAttachment m_metallicRoughnessFlags;
      FrameBufferAttachment m_emissive;
      FrameBufferAttachment m_depth;
      VkRenderPass          m_renderPass;
    };

    // Per View graphics data
    struct vkViewData
    {
      uint32_t                            m_currentWidth;
      uint32_t                            m_currentHeight;
      VkSurfaceKHR                        m_surface;
      VkSurfaceFormatKHR                  m_surfaceFormat;
      VkSwapchainKHR                      m_swapchain;
      VkExtent2D                          m_extent;
      VkViewport                          m_viewport;
      VkRect2D                            m_scissor;

      vector<VkImage>                     m_images;
      vector<VkImageView>                 m_imageViews;
      vector<VkFramebuffer>               m_framebuffers;
      VkFormat                            m_depthFormat;
      VkImage                             m_depthImage;
      VkDeviceMemory                      m_depthMemory;
      VkImageView                         m_depthView;
      VkSampler                           m_shadowSampler;
      VkRenderPass                        m_renderPass;

      queue<vkBackBuffer>                 m_backBuffers;
      vkBackBuffer                        m_acquiredBackBuffer;

      VkCommandPool                       m_commandPool;
      VkCommandBuffer                     m_commandBuffer[2];

      FrameBuffer                         m_gBuffer;
    };

    struct vkPipelineCacheInfo
    {
      size_t          m_numMeshBuffers;
      Material::Type  m_materialType;
      size_t          m_frameIndex;
      VkPipeline      m_pipeline;
    };



    void initializeInstance();
    void initializeVirtualDevice();
    void initializeProperties();
    void initializeQueues(uint32_t numFrames);

    bool has_all_device_extensions(VkPhysicalDevice physicalDevice);
    bool memory_type_from_properties(uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex);
    void allocate_resources(shared_ptr<Mesh> mesh, vkMeshData* meshData, VkDeviceSize vertexBufferSize, VkDeviceSize indexBufferSize);
    void loadTexture(shared_ptr<Texture> texture);
    void createImage(shared_ptr<Texture> texture, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory);
    void createTextureImage(shared_ptr<Texture> texture);
    void copyImage(VkImage srcImage, VkImage dstImage, size_t width, size_t height);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void createImageView(VkImage image, VkFormat format, VkImageView* imageView);
    void createTextureSampler(VkSampler* sampler);
    void createDescriptorSet(shared_ptr<Material> material, vkMaterialData* materialData, size_t frameNumber);
    void updateDescriptorSets(shared_ptr<Material> material, vkMaterialData* materialData, size_t frameNumber, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer);

    void createOnscreenView(shared_ptr<View> view, vkViewData* viewData, size_t numFrames);
    void createShadowView(shared_ptr<View> view, vkViewData* viewData, size_t numFrames);
    void createShadowRenderPass(shared_ptr<View> view, vkViewData* viewData);
    void createOnscreenDepthBuffer(shared_ptr<View> view, vkViewData* viewData);
    void createGBufferAttachments(shared_ptr<View> view, vkViewData* viewData);
    void createGBufferAttachment(shared_ptr<View> view, vkViewData* viewData, VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment *attachment);
    void setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout);

    void createOnscreenRenderPass(shared_ptr<View> view, vkViewData* viewData);

    void attachSwapchain(shared_ptr<View> view, vkViewData* viewData);
    void detachSwapchain(shared_ptr<View> view, vkViewData* viewData);

    VkPipeline loadPipeline(shared_ptr<Mesh> mesh, vkMeshData* meshData, shared_ptr<Material> material, size_t frameIndex, shared_ptr<View> view);

    VkCommandBuffer beginOneTimeCommands();
    void            endOneTimeCommands(VkCommandBuffer commandBuffer);

    vector<char> readFile(const string& filename);

    vector<vkPipelineCacheInfo*>  m_pipelineCache;
    shared_ptr<Material>          m_shadowMaterial;
    vector<vkViewData*>           m_shadowViewData;
    float                         m_constantDepthBias;
    float                         m_slopeDepthBias;
    VkFence*                      m_lightFences;
    int                           m_lightFenceIndex;
    long long                     m_allocatedImageMemory;
    map<string, vkTextureData*>   m_textureMap;
    bool                          m_deferred;
    shared_ptr<Material>          m_depthPrepassMaterial;
    vkLightPushContants           m_lightPushConstants;
    bool                          m_depthPrepass;
  };
}
