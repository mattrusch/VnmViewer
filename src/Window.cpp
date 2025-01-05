// Window.cpp

#include "Window.h"
#include "Application.h"
#include <cassert>

static inline bool RangeContaninsValue(int value, int start, int end)
{
    return(value >= start && value <= end);
}

LRESULT CALLBACK DefaultWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    Vnm::Window* pWindow = reinterpret_cast<Vnm::Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_NCCREATE:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)LPCREATESTRUCT(lparam)->lpCreateParams);
        break;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            return 0;
        }

        if (pWindow != nullptr)
        {
            WORD keyFlags = HIWORD(lparam);
            bool repeat = (keyFlags & KF_REPEAT) == KF_REPEAT;
            if (!repeat)
            {
                pWindow->OnKeyDown(static_cast<UINT8>(wparam));
            }
        }

        break;
    case WM_KEYUP:
        if (pWindow != nullptr)
        {
            pWindow->OnKeyUp(static_cast<UINT8>(wparam));
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (pWindow != nullptr)
        {
            pWindow->OnMouseDown(message, lparam);
        }
        break;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        if (pWindow != nullptr)
        {
            pWindow->OnMouseUp(message, lparam);
        }
        break;
    case WM_MOUSEMOVE:
        if (pWindow != nullptr)
        {
            pWindow->OnMouseMove(hwnd, lparam);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        break;
    }

    return DefWindowProc(hwnd, message, wparam, lparam);
}

namespace Vnm
{
    void Window::Create(HINSTANCE instance, int cmdShow, const WindowDesc& desc)
    {
        assert(desc.mParentApplication != nullptr);
        mApplication = desc.mParentApplication;

        WNDCLASSEX wndClass = {0};
        wndClass.cbSize = sizeof(WNDCLASSEX);
        wndClass.hbrBackground = (HBRUSH)COLOR_WINDOW;
        wndClass.hInstance = instance;
        wndClass.lpfnWndProc = desc.mWndProc;
        wndClass.lpszClassName = desc.mpTitle;
        wndClass.style = CS_HREDRAW | CS_VREDRAW;
        wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);

        RegisterClassEx(&wndClass);

        mHandle = CreateWindow(
            desc.mpTitle, 
            desc.mpTitle, 
            WS_OVERLAPPEDWINDOW, 
            desc.mTop, 
            desc.mLeft, 
            desc.mWidth,
            desc.mHeight, 
            NULL, 
            NULL, 
            instance,
            (void*)this);

        DWORD error = GetLastError();

        ShowWindow(mHandle, cmdShow);

        mWidth = desc.mWidth;
        mHeight = desc.mHeight;
    }

    void Window::OnKeyDown(UINT8 key)
    {
        // Forward to application
        if (mApplication != nullptr)
        {
            mApplication->OnKeyDown(key);
        }
    }

    void Window::OnKeyUp(UINT8 key)
    {
        // Forward to application
        if (mApplication != nullptr)
        {
            mApplication->OnKeyUp(key);
        }
    }

    void Window::OnMouseDown(UINT message, LPARAM lparam)
    {
        // Translate message
        uint32_t buttonMask = 0;

        switch (message)
        {
        case WM_LBUTTONDOWN:
            buttonMask |= LeftMouseButtonBit;
            break;
        case WM_MBUTTONDOWN:
            buttonMask |= MiddleMouseButtonBit;
            break;
        case WM_RBUTTONDOWN:
            buttonMask |= RightMouseButtonBit;
            break;
        default:
            assert(0);
            break;
        }

        // Forward to application
        mApplication->OnMouseDown(buttonMask);
    }

    void Window::OnMouseUp(UINT message, LPARAM lparam)
    {
        // Translate message
        uint32_t buttonMask = 0;

        switch (message)
        {
        case WM_LBUTTONUP:
            buttonMask |= LeftMouseButtonBit;
            break;
        case WM_MBUTTONUP:
            buttonMask |= MiddleMouseButtonBit;
            break;
        case WM_RBUTTONUP:
            buttonMask |= RightMouseButtonBit;
            break;
        default:
            assert(0);
            break;
        }

        // Forward to application
        mApplication->OnMouseUp(buttonMask);
    }

    void Window::OnMouseMove(HWND hwnd, LPARAM lparam)
    {
        SetCapture(hwnd);
        int mouseX = LOWORD(lparam);
        int mouseY = HIWORD(lparam);
        if (!RangeContaninsValue(mouseX, 0, mWidth) || !RangeContaninsValue(mouseY, 0, mHeight))
        {
            mApplication->OnMouseUp(LeftMouseButtonBit | MiddleMouseButtonBit | RightMouseButtonBit);
            ReleaseCapture();
        }

        // Forward to application
        mApplication->OnMouseMove(mouseX, mouseY);
    }

    void Window::Destroy()
    {
        DestroyWindow(mHandle);
    }

} // namespace Vnm
