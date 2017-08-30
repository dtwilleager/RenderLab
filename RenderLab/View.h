#pragma once
#include <string>
#include <vector>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

using glm::mat4;
using glm::vec3;
using glm::vec4;
using glm::vec2;

using std::string;
using std::vector;
using std::shared_ptr;

namespace RenderLab
{
  class Mesh;

  class View
  {
  public:

    enum Type
    {
      SCREEN,
      SHADOW,
      SHADOW_CUBE,
      REFLECTION
    };

    View(string name, Type type);
    ~View();

    Type getType();
    void setViewTransform(mat4& viewTransform);
    void getViewTransform(mat4& viewTransform);
    void getProjectionTransform(mat4& projectionTransform);

    void  setFieldOfView(float fieldOfView);
    float getFieldOfView();
    void  setNearClip(float nearClip);
    float getNearClip();
    void  setFarClip(float farClip);
    float getFarClip();
    void  setViewportPosition(vec2& position);
    void  getViewportPosition(vec2& position);
    void  setViewportSize(vec2& size);
    void  getViewportSize(vec2& size);
    void  addCompositeMesh(shared_ptr<Mesh> mesh);
    shared_ptr<Mesh>  getCompositeMesh(size_t index);
    size_t numCompositeMeshes();
    void  setGraphicsData(void * graphicsData);
    void* getGraphicsData();

  private:
    Type      m_type;
    float     m_fieldOfView;
    float     m_nearClip;
    float     m_farClip;
    vec2      m_viewportPosition;
    vec2      m_viewportSize;

    void      computeTransforms();

    mat4      m_viewMatrix;
    mat4      m_projectionMatrix;
    void*     m_graphicsData;

    vector<shared_ptr<Mesh>>  m_compositeMeshes;
  };
}
