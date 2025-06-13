// Application.h

#pragma once

#include "Window.h"
#include "Camera.h"
#include "D3d12Context.h"

namespace Vnm
{
    class Device;

    class MouseState
    {
    public:
        MouseState() = default;
        ~MouseState() = default;

        bool mLeftButtonDown = false;
        bool mMiddleButtonDown = false;
        bool mRightButtonDown = false;
        int  mMouseX = 0;
        int  mMouseY = 0;
    };

    constexpr uint32_t LeftMouseButtonBit   = 1 << 0;
    constexpr uint32_t MiddleMouseButtonBit = 1 << 1;
    constexpr uint32_t RightMouseButtonBit  = 1 << 2;

    class Application
    {
    public:
        Application() = default;
        ~Application() = default;

        void Startup(HINSTANCE instance, int cmdShow);
        void Mainloop();
        void Shutdown();

        void OnKeyUp(UINT8 key);
        void OnKeyDown(UINT8 key);
        void OnMouseDown(uint32_t buttonMask);
        void OnMouseUp(uint32_t buttonMask);
        void OnMouseMove(int x, int y);

    private:
        Window     mWindow;
        D3dContext mContext;
        Camera     mCamera;
        MouseState mMouseState;
        uint32_t   mMoveState = 0;
    };
}
