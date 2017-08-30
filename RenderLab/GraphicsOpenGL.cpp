#include "stdafx.h"
#include "GraphicsOpenGL.h"
#include "LightComponent.h"

namespace RenderLab
{
  GraphicsOpenGL::GraphicsOpenGL(string name, HINSTANCE hinstance, HWND window) : Graphics(name, hinstance, window),
    m_frameIndex(0),
    m_hinstance(hinstance),
    m_window(window)
  {
  }


  GraphicsOpenGL::~GraphicsOpenGL()
  {
  }

  void GraphicsOpenGL::initialize(uint32_t numFrames)
  {
    // Set the appropriate pixel format.
    PIXELFORMATDESCRIPTOR pfd =
    {
      sizeof(PIXELFORMATDESCRIPTOR),            // Size Of This Pixel Format Descriptor
      1,                                        // Version Number
      PFD_DRAW_TO_WINDOW |                      // Format Must Support Window
      PFD_SUPPORT_OPENGL |                      // Format Must Support OpenGL
      PFD_STEREO |                              // Format Must Support Quad-buffer Stereo
      PFD_DOUBLEBUFFER,                         // Must Support Double Buffering
      PFD_TYPE_RGBA,                            // Request An RGBA Format
      24,                                       // 24-bit color depth
      0, 0, 0, 0, 0, 0,                         // Color Bits Ignored
      0,                                        // No Alpha Buffer
      0,                                        // Shift Bit Ignored
      0,                                        // No Accumulation Buffer
      0, 0, 0, 0,                               // Accumulation Bits Ignored
      32,                                       // 32-bit Z-Buffer (Depth Buffer)
      0,                                        // No Stencil Buffer
      0,                                        // No Auxiliary Buffer
      PFD_MAIN_PLANE,                           // Main Drawing Layer
      0,                                        // Reserved
      0, 0, 0                                   // Layer Masks Ignored
    };

    m_deviceContext = GetDC(m_window);
    GLuint pixelFormat = ChoosePixelFormat(m_deviceContext, &pfd);
    SetPixelFormat(m_deviceContext, pixelFormat, &pfd);
    m_renderContext = wglCreateContext(m_deviceContext);
    wglMakeCurrent(m_deviceContext, m_renderContext);

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
      fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
    }

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
  }


  uint32_t GraphicsOpenGL::acquireBackBuffer(shared_ptr<View> view)
  {
    glViewData* viewData = (glViewData*)view->getGraphicsData();
    return viewData->m_frameIndex;
  }

  float GraphicsOpenGL::getGPUFrameTime()
  {
    return 0.0f;
  }

  float GraphicsOpenGL::getGPUFrameTime2()
  {
    return 0.0f;
  }

  void GraphicsOpenGL::renderBegin(shared_ptr<View> view, shared_ptr<View> lastView, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer, uint32_t frameIndex)
  {
    glViewData* viewData = (glViewData*)view->getGraphicsData();

    glViewport(0, 0, viewData->m_currentWidth, viewData->m_currentHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  void GraphicsOpenGL::bindPipeline(shared_ptr<Mesh> mesh, shared_ptr<View> view, uint32_t frameIndex, bool depthPrepass)
  {
  }

  void GraphicsOpenGL::setDepthBias(float constant, float slope)
  {
  }

  void GraphicsOpenGL::endDepthPrepass(shared_ptr<View> view, uint32_t frameIndex)
  {
  }

  void GraphicsOpenGL::render(shared_ptr<Mesh> mesh, uint32_t meshOffset, shared_ptr<View> view, uint32_t frameIndex, bool depthPrepass)
  {
    render(mesh->getMaterial());

    glMaterialData* materialData = (glMaterialData*)mesh->getMaterial()->getGraphicsData();
    glMeshData* meshData = (glMeshData*)mesh->getGraphicsData();

    mat4 viewTransform;
    mat4 compositeModelTransform;
    GLint i = 0;

    m_onscreenView->getViewTransform(viewTransform);
    mesh->getRenderComponent()->getEntity(0)->getCompositeTransform(compositeModelTransform);

    glUniformMatrix4fv(materialData->m_modelViewUniform, 1, GL_FALSE, glm::value_ptr(viewTransform*compositeModelTransform));

    glBindVertexArray(meshData->m_vertexArrayID);
    for (i = 0; i<mesh->getNumBuffers(); i++)
    {
      glEnableVertexAttribArray(i);
      glBindBuffer(GL_ARRAY_BUFFER, meshData->m_vertexArrayBufferIDs[i]);
      glVertexAttribPointer(i, (GLint)mesh->getVertexBufferSize(i), GL_FLOAT, GL_FALSE, 0, 0);
    }

    for (; i<8; i++)
    {
      glDisableVertexAttribArray(i);
    }

    if (mesh->getIndexBufferSize() != 0)
    {
      glDrawElements(GL_TRIANGLES, (GLsizei)mesh->getIndexBufferSize(), GL_UNSIGNED_INT, mesh->getIndexBuffer());
    }
    else
    {
      glDrawArrays(GL_TRIANGLES, 0, (GLsizei)mesh->getNumVerts());
    }
  }

  void GraphicsOpenGL::render(shared_ptr<Material> material)
  {
    glMaterialData* materialData = (glMaterialData*)material->getGraphicsData();
    glUseProgram(materialData->m_shaderProgram);

    mat4 viewTransform;
    mat4 projectionTransform;
    mat4 worldToCamera;
    mat4 compositeModelTransform;
    vec3 boundsCenter;

    m_onscreenView->getProjectionTransform(projectionTransform);

    glUniformMatrix4fv(materialData->m_projectionUniform, 1, GL_FALSE, glm::value_ptr(projectionTransform));

    m_onscreenView->getViewTransform(viewTransform);
    if (material->getBlendEnable())
    {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
      glDisable(GL_BLEND);
    }

    glDisable(GL_BLEND);

    vec4 diffuseColor;
    material->getAlbedoColor(diffuseColor);
    glUniform3fv(materialData->m_diffuseColorUniform, 1, glm::value_ptr(diffuseColor));

    if (material->getLightingEnable())
    {
      vec3 value;
      for (unsigned int i = 0; i<m_renderTechnique->getNumLightComponents(); i++)
      {
        shared_ptr<LightComponent> light = m_renderTechnique->getLightComponent(i);
        mat4 lightTransform;
        light->getEntity(0)->getCompositeTransform(lightTransform);

        light->getPosition(value);
        vec4 pos = vec4(value.x, value.y, value.z, 1.0f);
        pos = viewTransform*lightTransform*pos;
        value = vec3(pos.x, pos.y, pos.z);
        glUniform3fv(materialData->m_lightsUniform[i].position, 1, glm::value_ptr(value));

        light->getDirection(value);
        vec4 dir = vec4(value.x, value.y, value.z, 0.0f);
        dir = viewTransform*lightTransform*dir;
        value = glm::normalize(vec3(dir.x, dir.y, dir.z));
        glUniform3fv(materialData->m_lightsUniform[i].direction, 1, glm::value_ptr(value));

        light->getAmbient(value);
        glUniform3fv(materialData->m_lightsUniform[i].ambient, 1, glm::value_ptr(value));
        light->getDiffuse(value);
        glUniform3fv(materialData->m_lightsUniform[i].diffuse, 1, glm::value_ptr(value));
        light->getSpecular(value);
        glUniform3fv(materialData->m_lightsUniform[i].specular, 1, glm::value_ptr(value));
        light->getAttenuation(value);
        glUniform3fv(materialData->m_lightsUniform[i].attenuation, 1, glm::value_ptr(value));
      }
      glUniform1i(materialData->m_numLightsUniform, (GLint)m_renderTechnique->getNumLightComponents());

      vec3 color;
      material->getEmissiveColor(color);
      glUniform3fv(materialData->m_emissiveColorUniform, 1, glm::value_ptr(color));

      if (material->getMaterialType() != Material::LIT_NOTEXTURE)
      {
        shared_ptr<Texture> texture = material->getAlbedoTexture();
        glTextureData* textureData = (glTextureData*)texture->getGraphicsData();
        glBindTexture(GL_TEXTURE_2D, textureData->m_diffuseTextureID);
      }
      else
      {
        glBindTexture(GL_TEXTURE_2D, 0);
      }
    }
  }


  void GraphicsOpenGL::renderEnd(shared_ptr<View> view, shared_ptr<View> lastView, bool lastLight, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer, uint32_t frameIndex)
  {
  }

  void GraphicsOpenGL::swapBackBuffer(shared_ptr<View> view, uint32_t frameIndex)
  {
    SwapBuffers(m_deviceContext);
  }

  size_t GraphicsOpenGL::getBufferAlignment()
  {
    return 64;
  }

  void GraphicsOpenGL::build(shared_ptr<Mesh> mesh, vector<shared_ptr<UniformBuffer>>& frameDataUniformBuffers, vector<shared_ptr<UniformBuffer>>& objectDataUniformBuffers, size_t numFrames, vector<shared_ptr<LightComponent>>& lightComponents)
  {
    glMeshData* meshData = (glMeshData*)mesh->getGraphicsData();
    if (meshData == nullptr || mesh->isDirty())
    {
      if (meshData != nullptr)
      {
        // TODO: Free old resources
      }

      size_t numBuffers = mesh->getNumBuffers();

      meshData = new glMeshData();
      mesh->setGraphicsData(meshData);
      meshData->m_vertexArrayBufferIDs = new GLuint[numBuffers];

      if (meshData->m_vertexArrayID == 0)
      {
        glGenVertexArrays(1, &meshData->m_vertexArrayID);
      }

      for (unsigned int i = 0; i < numBuffers; i++)
      {
        glBindVertexArray(meshData->m_vertexArrayID);
        glEnableVertexAttribArray(i);
        glGenBuffers(1, &meshData->m_vertexArrayBufferIDs[i]);
        glBindBuffer(GL_ARRAY_BUFFER, meshData->m_vertexArrayBufferIDs[i]);
        glBufferData(GL_ARRAY_BUFFER, mesh->getVertexBufferNumBytes(i), mesh->getVertexBufferData(i), GL_STATIC_DRAW);
        glVertexAttribPointer(i, (GLint)mesh->getVertexBufferSize(i), GL_FLOAT, GL_FALSE, 0, 0);
      }
    }
    build(mesh->getMaterial(), frameDataUniformBuffers, objectDataUniformBuffers, numFrames);
  }

  void GraphicsOpenGL::build(shared_ptr<Material> material, vector<shared_ptr<UniformBuffer>>& frameDataUniformBuffers, vector<shared_ptr<UniformBuffer>>& objectDataUniformBuffers, size_t numFrames)
  {
    glMaterialData* materialData = (glMaterialData*)material->getGraphicsData();
    if (materialData == nullptr || material->isDirty())
    {
      if (materialData != nullptr)
      {
        // TODO: Free old resources
      }

      materialData = new glMaterialData();

      switch (material->getMaterialType())
      {
      case Material::LIT:
        materialData->m_vertexShaderCode = readFile("shaders/vShaderLit.glsl");
        materialData->m_fragmentShaderCode = readFile("shaders/fShaderLit.glsl");
        break;
      case Material::LIT_NOTEXTURE:
        materialData->m_vertexShaderCode = readFile("shaders/vShaderLitNoTex.glsl");
        materialData->m_fragmentShaderCode = readFile("shaders/fShaderLitNoTex.glsl");
        break;
      }

      materialData->m_vertexShaderCodeSize = (GLint)materialData->m_vertexShaderCode.size();
      materialData->m_fragmentShaderCodeSize = (GLint)materialData->m_fragmentShaderCode.size();

      materialData->m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
      const char* vsource = materialData->m_vertexShaderCode.data();
      glShaderSource(materialData->m_vertexShader, 1, &vsource, &materialData->m_fragmentShaderCodeSize);
      glCompileShader(materialData->m_vertexShader);
      GLenum err = glGetError();

      materialData->m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
      const char* fsource = materialData->m_fragmentShaderCode.data();
      glShaderSource(materialData->m_fragmentShader, 1, &fsource, &materialData->m_fragmentShaderCodeSize);
      glCompileShader(materialData->m_fragmentShader);
      err = glGetError();

      materialData->m_shaderProgram = glCreateProgram();
      glAttachShader(materialData->m_shaderProgram, materialData->m_vertexShader);
      glAttachShader(materialData->m_shaderProgram, materialData->m_fragmentShader);
      glLinkProgram(materialData->m_shaderProgram);
      err = glGetError();

      glUseProgram(materialData->m_shaderProgram);
      err = glGetError();

      if (material->getAlbedoTexture() != nullptr)
      {
        loadTexture(material->getAlbedoTexture());
      }

      materialData->m_projectionUniform = glGetUniformLocation(materialData->m_shaderProgram, "projectionMatrix");
      materialData->m_modelViewUniform = glGetUniformLocation(materialData->m_shaderProgram, "modelViewMatrix");

      if (material->getLightingEnable())
      {
        materialData->m_numLightsUniform = glGetUniformLocation(materialData->m_shaderProgram, "numLights");

        materialData->m_lightsUniform[0].position = glGetUniformLocation(materialData->m_shaderProgram, "lights[0].position");
        materialData->m_lightsUniform[0].direction = glGetUniformLocation(materialData->m_shaderProgram, "lights[0].direction");
        materialData->m_lightsUniform[0].ambient = glGetUniformLocation(materialData->m_shaderProgram, "lights[0].ambient");
        materialData->m_lightsUniform[0].diffuse = glGetUniformLocation(materialData->m_shaderProgram, "lights[0].diffuse");
        materialData->m_lightsUniform[0].specular = glGetUniformLocation(materialData->m_shaderProgram, "lights[0].specular");
        materialData->m_lightsUniform[0].attenuation = glGetUniformLocation(materialData->m_shaderProgram, "lights[0].attenuation");

        materialData->m_lightsUniform[1].position = glGetUniformLocation(materialData->m_shaderProgram, "lights[1].position");
        materialData->m_lightsUniform[1].direction = glGetUniformLocation(materialData->m_shaderProgram, "lights[1].direction");
        materialData->m_lightsUniform[1].ambient = glGetUniformLocation(materialData->m_shaderProgram, "lights[1].ambient");
        materialData->m_lightsUniform[1].diffuse = glGetUniformLocation(materialData->m_shaderProgram, "lights[1].diffuse");
        materialData->m_lightsUniform[1].specular = glGetUniformLocation(materialData->m_shaderProgram, "lights[1].specular");
        materialData->m_lightsUniform[1].attenuation = glGetUniformLocation(materialData->m_shaderProgram, "lights[1].attenuation");

        materialData->m_lightsUniform[2].position = glGetUniformLocation(materialData->m_shaderProgram, "lights[2].position");
        materialData->m_lightsUniform[2].direction = glGetUniformLocation(materialData->m_shaderProgram, "lights[2].direction");
        materialData->m_lightsUniform[2].ambient = glGetUniformLocation(materialData->m_shaderProgram, "lights[2].ambient");
        materialData->m_lightsUniform[2].diffuse = glGetUniformLocation(materialData->m_shaderProgram, "lights[2].diffuse");
        materialData->m_lightsUniform[2].specular = glGetUniformLocation(materialData->m_shaderProgram, "lights[2].specular");
        materialData->m_lightsUniform[2].attenuation = glGetUniformLocation(materialData->m_shaderProgram, "lights[2].attenuation");

        materialData->m_lightsUniform[3].position = glGetUniformLocation(materialData->m_shaderProgram, "lights[3].position");
        materialData->m_lightsUniform[3].direction = glGetUniformLocation(materialData->m_shaderProgram, "lights[3].direction");
        materialData->m_lightsUniform[3].ambient = glGetUniformLocation(materialData->m_shaderProgram, "lights[3].ambient");
        materialData->m_lightsUniform[3].diffuse = glGetUniformLocation(materialData->m_shaderProgram, "lights[3].diffuse");
        materialData->m_lightsUniform[3].specular = glGetUniformLocation(materialData->m_shaderProgram, "lights[3].specular");
        materialData->m_lightsUniform[3].attenuation = glGetUniformLocation(materialData->m_shaderProgram, "lights[3].attenuation");

        materialData->m_specularColorUniform = glGetUniformLocation(materialData->m_shaderProgram, "specularColor");
        materialData->m_ambientColorUniform = glGetUniformLocation(materialData->m_shaderProgram, "ambientColor");
        materialData->m_emissiveColorUniform = glGetUniformLocation(materialData->m_shaderProgram, "emissiveColor");
        materialData->m_transparentColorUniform = glGetUniformLocation(materialData->m_shaderProgram, "transparentColor");
        materialData->m_shininessUniform = glGetUniformLocation(materialData->m_shaderProgram, "shininess");
      }

      materialData->m_diffuseColorUniform = glGetUniformLocation(materialData->m_shaderProgram, "diffuseColor");
      err = glGetError();

      material->setGraphicsData(materialData);
      material->setDirty(false);
    }
  }

  void GraphicsOpenGL::loadTexture(shared_ptr<Texture> texture)
  {
    glTextureData* textureData = new glTextureData();
    texture->setGraphicsData(textureData);

    size_t width = texture->getWidth();
    size_t height = texture->getHeight();

    glGenTextures(1, &textureData->m_diffuseTextureID);
    glBindTexture(GL_TEXTURE_2D, textureData->m_diffuseTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)texture->getBitsPerPixel(), (GLint)width, (GLint)height,
      0, texture->getFormat(), GL_UNSIGNED_BYTE, texture->getData());
  }

  void GraphicsOpenGL::build(shared_ptr<UniformBuffer> buffer)
  {
    glUniformBufferData* uniformBufferData = new glUniformBufferData();
    buffer->setGraphicsData(uniformBufferData);

    void *ptr = nullptr;
    //vkMapMemory(m_device, uniformBufferData->m_deviceMemory, 0, VK_WHOLE_SIZE, 0, &ptr);

    //vkBindBufferMemory(m_device, uniformBufferData->m_buffer, uniformBufferData->m_deviceMemory, 0);
    uniformBufferData->m_data = reinterpret_cast<uint8_t *>(ptr);
  }

  void GraphicsOpenGL::updateUniformData(shared_ptr<UniformBuffer> buffer, size_t offset, float* data, size_t size)
  {
    glUniformBufferData* bufferData = (glUniformBufferData*)buffer->getGraphicsData();
    if (bufferData->m_data != nullptr)
    {
      uint8_t* ptr = bufferData->m_data + offset;
      memcpy(ptr, data, size);
    }
  }

  void GraphicsOpenGL::updateUniformData(shared_ptr<UniformBuffer> buffer, size_t offset, uint8_t* data, size_t size)
  {
    glUniformBufferData* bufferData = (glUniformBufferData*)buffer->getGraphicsData();
    if (bufferData->m_data != nullptr)
    {
      uint8_t* ptr = bufferData->m_data + offset;
      memcpy(ptr, data, size);
    }
  }

  void GraphicsOpenGL::build(shared_ptr<View> view, size_t numFrames)
  {
    glViewData* viewData = new glViewData();
    view->setGraphicsData(viewData);

    vec2 size;
    view->getViewportSize(size);
    viewData->m_currentWidth = (uint32_t)size.x;
    viewData->m_currentHeight = (uint32_t)size.y;

    if (view->getType() == View::SCREEN)
    {
      // Create the surface
      resize(m_onscreenView, viewData->m_currentWidth, viewData->m_currentHeight);
      viewData->m_frameIndex = m_frameIndex++;
    }
  }

  void GraphicsOpenGL::resize(shared_ptr<View> view, uint32_t width, uint32_t height)
  {
    glViewData* viewData = (glViewData*)view->getGraphicsData();
    if (width != viewData->m_currentWidth || height != viewData->m_currentHeight)
    {
      viewData->m_currentWidth = width;
      viewData->m_currentHeight = height;
    }
  }

  vector<char> GraphicsOpenGL::readFile(const string& filename) {
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
}
