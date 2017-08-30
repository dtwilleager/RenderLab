#pragma once
#include "Material.h"
#include "RenderComponent.h"

#include <string>
#include <memory>

using std::string;
using std::shared_ptr;

namespace RenderLab
{
  class RenderComponent;

  class Mesh
  {
  public:
    Mesh(string name, size_t numVerts, size_t numVertexArrayBuffers);
    ~Mesh();

    void                  addVertexBuffer(unsigned int index, size_t size, size_t numBytes, float* data);
    void                  addIndexBuffer(size_t size, unsigned int* data);
    size_t                getVertexBufferSize(size_t index);
    size_t                getVertexBufferNumBytes(size_t index);
    float*				        getVertexBufferData(size_t index);
    size_t                getIndexBufferSize();
    size_t                getNumVerts();
    unsigned int*         getIndexBuffer();
    size_t                getNumBuffers();
    void                  setMaterial(shared_ptr<Material> material);
    shared_ptr<Material>  getMaterial();
    void                  setRenderComponent(shared_ptr<RenderComponent> renderComponent);
    shared_ptr<RenderComponent> getRenderComponent();
    void                  setDirty(bool dirty);
    bool                  isDirty();
    void                  setGraphicsData(void * graphicsData);
    void*                 getGraphicsData();

  private:
    struct vertexData
    {
      size_t  size;
      size_t  numBytes;
      float*  data;
    };
 
    string                m_name;
    size_t                m_numVerts;
    size_t                m_numVertexArrayBuffers;
    struct vertexData*    m_vertexData;
    unsigned int*         m_indexBuffer;
    size_t                m_indexBufferSize;
    shared_ptr<Material>  m_material;
    shared_ptr<RenderComponent>  m_renderComponent;
    bool                  m_dirty;
    void*                 m_graphicsData;
  };
}

