#pragma once

#include "GraphicsContext.h"
#include "Mesh.h"
#include "UniformBuffer.h"
#include "View.h"
#include "RenderTechnique.h"
#include "LightComponent.h"

#include <string>
#include <memory>
#include <vector>

using std::string;
using std::shared_ptr;
using std::vector;

namespace RenderLab
{
  class RenderTechnique;

  class Graphics
  {
  public:
    Graphics(string name, HINSTANCE hinstance, HWND window);
    ~Graphics();

    virtual void                initialize(uint32_t numFrames);
    virtual void                setRenderTechnique(shared_ptr<RenderTechnique> renderTechnique);
    virtual void                build(shared_ptr<Mesh> mesh, vector<shared_ptr<UniformBuffer>>& frameDataUniformBuffers, vector<shared_ptr<UniformBuffer>>& objectDataUniformBuffers, size_t numFrames, vector<shared_ptr<LightComponent>>& lightComponents);
    virtual size_t              getBufferAlignment();
    virtual void                build(shared_ptr<UniformBuffer> buffer);
    virtual void                build(shared_ptr<View> view, size_t numFrames);
    virtual void                setOnscreenView(shared_ptr<View> view);
    virtual void                resize(shared_ptr<View> view, uint32_t width, uint32_t height);
    virtual void                setDepthBias(float constant, float slope);

    virtual void                updateUniformData(shared_ptr<UniformBuffer> buffer, size_t offset, float* data, size_t size);
    virtual void                updateUniformData(shared_ptr<UniformBuffer> buffer, size_t offset, uint8_t* data, size_t size);

    virtual uint32_t            acquireBackBuffer(shared_ptr<View> view);
    virtual void                renderBegin(shared_ptr<View> view, shared_ptr<View> lastView, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer, uint32_t frameIndex);
    virtual void                renderEnd(shared_ptr<View> view, shared_ptr<View> lastView, bool lastLight, shared_ptr<UniformBuffer> frameDataUniformBuffer, shared_ptr<UniformBuffer> objectDataUniformBuffer, uint32_t frameIndex);
    virtual void                swapBackBuffer(shared_ptr<View> view, uint32_t frameIndex);
    virtual void                bindPipeline(shared_ptr<Mesh> mesh, shared_ptr<View> view, uint32_t frameIndex, bool depthPrepass);
    virtual void                endDepthPrepass(shared_ptr<View> view, uint32_t frameIndex);
    virtual void                render(shared_ptr<Mesh> mesh, uint32_t meshOffset, shared_ptr<View> view, uint32_t frameIndex, bool depthPrepass);
    virtual float				        getGPUFrameTime();
    virtual float				        getGPUFrameTime2();

    shared_ptr<GraphicsContext> getGraphicsContext();

  protected:
    shared_ptr<GraphicsContext>   m_graphicsContext;
    HINSTANCE                     m_hinstance;
    HWND                          m_window;
    shared_ptr<View>              m_onscreenView;
    shared_ptr<RenderTechnique>   m_renderTechnique;

  private:
    string                        m_name;

  };
}
