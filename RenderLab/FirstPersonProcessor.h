#pragma once
#include "ProcessorComponent.h"
#include "View.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>

#include <atlstr.h>

using std::string;
using glm::mat4;
using glm::vec3;
using glm::vec4;

namespace RenderLab
{
	class FirstPersonProcessor :
		public ProcessorComponent
	{
	public:
		FirstPersonProcessor(string name, shared_ptr<View> view);
		~FirstPersonProcessor();

    void  handleKeyboard(MSG* event);
    void  handleMouse(MSG* event);
    void  execute(double absoluteTime, double deltaTime);

  private:
    void printLog(string s);

    enum State {
      STOPPED,
      FORWARD,
      BACKWARDS,
      RIGHT,
      LEFT,
      UP,
      DOWN
    };

    shared_ptr<View>    m_view;
    float               m_lastHeight;
    vec3                m_forward;
    vec3                m_up;
    vec3                m_right;
    vec3                m_position;
    mat4                m_transform;
    int                 m_currentX;
    int                 m_currentY;
    int                 m_prevX;
    int                 m_prevY;
    State               m_state;
    float               m_heightOffset;
    bool                m_mouseDown;
	};

}