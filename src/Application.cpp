// Application.cpp

#include "Application.h"
#include "D3d12Context.h"

namespace Vnm
{
    constexpr uint32_t MoveForwardBit = 1 << 0;
    constexpr uint32_t MoveBackBit    = 1 << 1;
    constexpr uint32_t TurnLeftBit    = 1 << 2;
    constexpr uint32_t TurnRightBit   = 1 << 3;
    constexpr uint32_t TiltUpBit      = 1 << 4;
    constexpr uint32_t TiltDownBit    = 1 << 5;

    D3dContext gContext; // TODO: Unglobalize

    static void HandleMovement(uint32_t key, Camera& camera)
    {
        const float rotationScale = 0.01f;
        const float forwardScale = 0.1f;

        if (key & MoveForwardBit)
        {
            camera.MoveForward(forwardScale);
        }
        if (key & MoveBackBit)
        {
            camera.MoveForward(-forwardScale);
        }
        if (key & TurnLeftBit)
        {
            camera.Yaw(-DirectX::XM_PI * rotationScale);
        }
        if (key & TurnRightBit)
        {
            camera.Yaw(DirectX::XM_PI * rotationScale);
        }
        if (key & TiltDownBit)
        {
            camera.Pitch(DirectX::XM_PI * rotationScale);
        }
        if (key & TiltUpBit)
        {
            camera.Pitch(-DirectX::XM_PI * rotationScale);
        }
    }

    static void HandleMouse(const MouseState& mouseState, Camera& camera)
    {
        static int prevMouseX = mouseState.mMouseX;
        static int prevMouseY = mouseState.mMouseY;

        if (mouseState.mLeftButtonDown)
        {
            const float rotationSpeedFactor = 0.001f;
            float deltaY = ((float)mouseState.mMouseY - (float)prevMouseY) * rotationSpeedFactor;
            float deltaX = ((float)mouseState.mMouseX - (float)prevMouseX) * rotationSpeedFactor;
            camera.Pitch(deltaY);
            camera.Yaw(deltaX);
        }

        prevMouseX = mouseState.mMouseX;
        prevMouseY = mouseState.mMouseY;
    }

    void Application::Startup(HINSTANCE instance, int cmdShow)
    {
        // Create main window and device
        Window::WindowDesc winDesc;
        winDesc.mWidth = 2560;
        winDesc.mHeight = 1600;
        winDesc.mParentApplication = this;
        mWindow.Create(instance, cmdShow, winDesc);

        gContext.Init(mWindow.GetHandle());
        mCamera.SetPosition(DirectX::XMVectorSet(0.0f, 0.0f, -10.0f, 0.0f));
    }

    void Application::Mainloop()
    {
        HandleMovement(mMoveState, mCamera);
        HandleMouse(mMouseState, mCamera);

        static uint32_t lastTime = GetTickCount();
        uint32_t elapsedTime = GetTickCount() - lastTime;
        lastTime = GetTickCount();
        float elapsedSeconds = static_cast<float>(elapsedTime) * 0.001f;

        gContext.Update(mCamera.CalcLookAt(), elapsedSeconds);
        gContext.Render();
    }

    void Application::Shutdown()
    {
        mWindow.Destroy();
    }

    void Application::OnKeyUp(UINT8 key)
    {
        switch (key)
        {
        case VK_SPACE:
        case 'W':
            mMoveState &= ~MoveForwardBit;
            break;
        case VK_SHIFT:
        case 'S':
            mMoveState &= ~MoveBackBit;
            break;
        case VK_LEFT:
        case 'A':
            mMoveState &= ~TurnLeftBit;
            break;
        case VK_RIGHT:
        case 'D':
            mMoveState &= ~TurnRightBit;
            break;
        case VK_UP:
            mMoveState &= ~TiltDownBit;
            break;
        case VK_DOWN:
            mMoveState &= ~TiltUpBit;
            break;
        default: break;
        }
    }

    void Application::OnKeyDown(UINT8 key)
    {
        switch (key)
        {
        case VK_TAB:
            break;
        case VK_SPACE:
        case 'W':
            mMoveState |= MoveForwardBit;
            break;
        case VK_SHIFT:
        case 'S':
            mMoveState |= MoveBackBit;
            break;
        case VK_LEFT:
        case 'A':
            mMoveState |= TurnLeftBit;
            break;
        case VK_RIGHT:
        case 'D':
            mMoveState |= TurnRightBit;
            break;
        case VK_UP:
            mMoveState |= TiltDownBit;
            break;
        case VK_DOWN:
            mMoveState |= TiltUpBit;
            break;
        default: break;
        }
    }

    void Application::OnMouseDown(uint32_t buttonMask)
    {
        if (buttonMask & LeftMouseButtonBit)
        {
            mMouseState.mLeftButtonDown = true;
        }

        if (buttonMask & MiddleMouseButtonBit)
        {
            mMouseState.mMiddleButtonDown = true;
        }

        if (buttonMask & RightMouseButtonBit)
        {
            mMouseState.mRightButtonDown = true;
        }
    }

    void Application::OnMouseUp(uint32_t buttonMask)
    {
        if (buttonMask & LeftMouseButtonBit)
        {
            mMouseState.mLeftButtonDown = false;
        }

        if (buttonMask & MiddleMouseButtonBit)
        {
            mMouseState.mMiddleButtonDown = false;
        }

        if (buttonMask & RightMouseButtonBit)
        {
            mMouseState.mRightButtonDown = false;
        }
    }

    void Application::OnMouseMove(int x, int y)
    {
        mMouseState.mMouseX = x;
        mMouseState.mMouseY = y;
    }

} // namesoace Vnm
