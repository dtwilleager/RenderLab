#pragma once
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

#include <GL/glew.h>
#include <gl/GL.h>

using std::string;
using std::shared_ptr;
using std::vector;
using std::queue;
using std::set;
using std::array;
using std::ifstream;

namespace RenderLab
{
  class GraphicsOpenGL :
    public Graphics
  {
  public:
    GraphicsOpenGL(string name, HINSTANCE hinstance, HWND window);
    ~GraphicsOpenGL();

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
    float			          getGPUFrameTime();
    float			          getGPUFrameTime2();

  private:

    // Per mesh graphics data
    struct glMeshData
    {
      GLuint  m_vertexArrayID;
      GLuint* m_vertexArrayBufferIDs;
    };

    // Per texture graphics data
    struct glTextureData
    {
      GLuint m_diffuseTextureID;
      GLuint m_diffuseTextureLocation;
    };

    // Per material graphics data
    struct glMaterialData
    {
      vector<char>  m_vertexShaderCode;
      GLint         m_vertexShaderCodeSize;
      vector<char>  m_fragmentShaderCode;
      GLint         m_fragmentShaderCodeSize;

      GLuint m_vertexShader;
      GLuint m_fragmentShader;
      GLuint m_shaderProgram;

      GLuint m_modelViewUniform;
      GLuint m_projectionUniform;
      GLuint m_normalMatrixUniform;

      // Material Info
      GLuint m_diffuseColorUniform;
      GLuint m_specularColorUniform;
      GLuint m_ambientColorUniform;
      GLuint m_emissiveColorUniform;
      GLuint m_transparentColorUniform;
      GLuint m_shininessUniform;

      struct Light
      {
        GLuint position;
        GLuint direction;
        GLuint ambient;
        GLuint diffuse;
        GLuint specular;
        GLuint attenuation;
      };

      Light m_lightsUniform[8];
      GLuint m_numLightsUniform;
    };

    // Per buffer graphics data
    struct glUniformBufferData
    {
      uint8_t*                      m_data;
    };

    // Data needed for back buffers
    struct glBackBuffer {
    };

    // Per View graphics data
    struct glViewData
    {
      uint32_t m_currentWidth;
      uint32_t m_currentHeight;
      uint32_t m_frameIndex;
    };

    struct glPipelineCacheInfo
    {
    };

    void          loadTexture(shared_ptr<Texture> texture);
    vector<char>  readFile(const string& filename);
    void          render(shared_ptr<Material> material);

    uint32_t  m_frameIndex;
    HINSTANCE m_hinstance;
    HWND      m_window;

    HGLRC    m_renderContext;
    HDC      m_deviceContext;
    GLuint   m_frameBufferTextureId;
    GLuint   m_frameBufferObjectId;
    GLuint   m_frameBufferDepthTextureId;
  };
}


