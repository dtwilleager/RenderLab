#include "stdafx.h"
#include "FirstPersonProcessor.h"

#include <WindowsX.h>

namespace RenderLab
{
  FirstPersonProcessor::FirstPersonProcessor(string name, shared_ptr<View> view) : ProcessorComponent(name, true, true),
    m_view(view),
    m_forward(vec3(0.0f, 0.0f, 1.0f)),
    m_right(vec3(1.0f, 0.0f, 0.0f)),
    m_up(vec3(0.0f, 1.0f, 0.0f)),
    m_position(vec3(0.0f, -15.0f, 0.0f)),
    m_lastHeight(0.0f),
    m_heightOffset(2.0f),
    m_state(STOPPED),
    m_mouseDown(false)
  {
  }


  FirstPersonProcessor::~FirstPersonProcessor()
  {
  }

  void FirstPersonProcessor::handleKeyboard(MSG* event)
  {
    if (event->message == WM_KEYDOWN)
    {
      WPARAM keyCode = event->wParam;
      switch (keyCode)
      {
      case 0x57: // W
        m_state = FORWARD;
        break;
      case 0x53: // S
        m_state = BACKWARDS;
        break;
      case 0x41: // A
        m_state = LEFT;
        break;
      case 0x44: // D
        m_state = RIGHT;
        break;
      case 0x43: // C
        m_heightOffset -= 0.2f;
        break;
      case 0x42: // B
        printLog("Position: " + std::to_string(m_position.x) + ", " + std::to_string(m_position.z));
        break;
      case VK_SPACE: // Space
        m_heightOffset += 0.2f;
        break;
      }
    }

    if (event->message == WM_KEYUP)
    {
      m_state = STOPPED;
    }
  }

  void FirstPersonProcessor::handleMouse(MSG* event)
  {
    int x = GET_X_LPARAM(event->lParam);
    int y = GET_Y_LPARAM(event->lParam);
    int delta = GET_WHEEL_DELTA_WPARAM(event->wParam);

    switch (event->message)
    {
    case WM_MOUSEMOVE:
      if (m_mouseDown)
      {
        m_prevX = m_currentX;
        m_prevY = m_currentY;
        m_currentX = x;
        m_currentY = y;
      }
      break;
    case WM_MOUSEWHEEL:
      break;
    case WM_LBUTTONDOWN:
      m_mouseDown = true;
      m_currentX = x;
      m_currentY = y;
      m_prevX = m_currentX;
      m_prevY = m_currentY;
      break;
    case WM_LBUTTONUP:
      m_mouseDown = false;
      break;
    }
  }

  void FirstPersonProcessor::execute(double absoluteTime, double deltaTime)
  {
    float speed = 0.5f;

    switch (m_state)
    {
    case FORWARD: // W
      m_position += m_forward * speed;
      break;
    case BACKWARDS: // S
      m_position -= m_forward * speed;
      break;
    case LEFT: // A
      m_position -= m_right * speed;
      break;
    case RIGHT: // D
      m_position += m_right * speed;
      break;
    }

    float mouseSpeed = 0.1f;

    if (m_mouseDown)
    {
      if (m_currentY != m_prevY)
      {
        float rightRotation = (m_currentY - m_prevY) * mouseSpeed;
        rightRotation = 0.0f;
        float upRotation = (m_currentX - m_prevX) * mouseSpeed;
        mat4 transform = glm::rotate(mat4(), rightRotation, m_right);
        transform = glm::rotate(transform, upRotation, m_up);
        vec4 forward = vec4(m_forward.x, m_forward.y, m_forward.z, 1.0f);
        vec4 up = vec4(m_up.x, m_up.y, m_up.z, 1.0f);

        forward = transform * forward;
        up = transform * up;
        m_forward = vec3(forward.x, forward.y, forward.z);
        m_up = vec3(up.x, up.y, up.z);
        m_right = glm::cross(m_forward, m_up);
      }
    }

    m_transform = glm::lookAt(m_position, m_position + m_forward, m_up);
    m_view->setViewTransform(m_transform);
  }

  void FirstPersonProcessor::printLog(string s)
  {
    string st = s + "\n";
    TCHAR name[256];
    _tcscpy_s(name, CA2T(st.c_str()));
    OutputDebugString(name);
  }
}
