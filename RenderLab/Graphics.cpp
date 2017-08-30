#include "stdafx.h"
#include "Graphics.h"

namespace RenderLab
{
  Graphics::Graphics(string name, HINSTANCE hinstance, HWND window):
    m_name(name),
    m_hinstance(hinstance),
    m_window(window),
    m_onscreenView(nullptr)
  {
  }


  Graphics::~Graphics()
  {
  }

  void Graphics::initialize(uint32_t numFrames)
  {
  }

  void Graphics::setRenderTechnique(shared_ptr<RenderTechnique> renderTechnique)
  {
    m_renderTechnique = renderTechnique;
  }

  void Graphics::build(shared_ptr<Mesh> mesh, vector<shared_ptr<UniformBuffer>>& frameDataUniformBuffers, vector<shared_ptr<UniformBuffer>>& objectDataUniformBuffers, size_t numFrames, vector<shared_ptr<LightComponent>>& lightComponents)
  {
  }

  size_t Graphics::getBufferAlignment()
  {
    return 0;
  }

  void Graphics::build(shared_ptr<UniformBuffer> buffer)
  {
  }

  void Graphics::build(shared_ptr<View> view, size_t numFrames)
  {
  }

  void Graphics::setOnscreenView(shared_ptr<View> view)
  {
    m_onscreenView = view;
  }

  void Graphics::resize(shared_ptr<View> view, uint32_t width, uint32_t height)
  {
  }

  void Graphics::setDepthBias(float constant, float slope)
  {
  }

  uint32_t Graphics::acquireBackBuffer(shared_ptr<View> view)
  {
    return 0;
  }

  void Graphics::renderBegin(shared_ptr<View> view, shared_ptr<View> lastView, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer, uint32_t frameIndex)
  {
  }

  void Graphics::updateUniformData(shared_ptr<UniformBuffer> buffer, size_t offset, float* data, size_t size)
  {
  }

  void Graphics::updateUniformData(shared_ptr<UniformBuffer> buffer, size_t offset, uint8_t* data, size_t size)
  {
  }

  void Graphics::renderEnd(shared_ptr<View> view, shared_ptr<View> lastView, bool lastLight, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer, uint32_t frameIndex)
  {
  }

  void Graphics::bindPipeline(shared_ptr<Mesh> mesh, shared_ptr<View> view, uint32_t frameIndex, bool depthPrepass)
  {
  }

  void Graphics::endDepthPrepass(shared_ptr<View> view, uint32_t frameIndex)
  {
  }

  void Graphics::render(shared_ptr<Mesh> mesh, uint32_t meshOffset, shared_ptr<View> view, uint32_t frameIndex, bool depthPrepass)
  {
  }

  float Graphics::getGPUFrameTime()
  {
    return 0.0f;
  }

  float Graphics::getGPUFrameTime2()
  {
    return 0.0f;
  }

  void Graphics::swapBackBuffer(shared_ptr<View> view, uint32_t frameIndex)
  {
  }


  shared_ptr<GraphicsContext> Graphics::getGraphicsContext()
  {
    return m_graphicsContext;
  }
}