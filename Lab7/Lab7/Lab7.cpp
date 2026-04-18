// Lab7.cpp : Определяет точку входа для приложения.
//

#include "framework.h"
#include "Lab7.h"
#include "RenderClass.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"

#include <windowsx.h>


#define MAX_LOADSTRING 100

WCHAR szTitle[MAX_LOADSTRING] = L"Lab7 Group: Babakhina, Lapina, Lips";
WCHAR szWindowClass[MAX_LOADSTRING] = L"Lab7Class";

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

RenderClass* g_Render = nullptr;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        OutputDebugString(_T("Error in Init\n"));
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LAB7));


    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        //OutputDebugString(_T("Render\n"));
        g_Render->Render();
    }

    g_Render->Terminate();
    delete g_Render;

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = {};

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hbrBackground = NULL;
    wcex.lpszClassName = szWindowClass;

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd = CreateWindowW(
        szWindowClass, szTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd)
    {
        OutputDebugString(_T("Error in Create window\n"));
        return FALSE;
    }

    g_Render = new RenderClass();
    if (FAILED(g_Render->Init(hWnd, szTitle, szWindowClass)))
    {
        OutputDebugString(_T("Error in Init Renderer\n"));
        delete g_Render;
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    SetFocus(hWnd);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui::GetCurrentContext() != nullptr)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
            return true;
    }

    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_SIZE:
        if (g_Render != nullptr && wParam != SIZE_MINIMIZED)
        {
            g_Render->Resize(hWnd);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_RBUTTONDOWN:
        if (g_Render != nullptr)
        {
            SetCapture(hWnd);
            g_Render->MouseRBPressed(true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        break;

    case WM_RBUTTONUP:
        if (g_Render != nullptr)
        {
            ReleaseCapture();
            g_Render->MouseRBPressed(false, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        break;

    case WM_MOUSEWHEEL:
        if (g_Render != nullptr)
        {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            g_Render->MouseWheel(delta);
        }
        break;

    case WM_KEYDOWN:
        if (g_Render != nullptr)
        {
            switch (wParam)
            {
            case VK_UP:    
                g_Render->MoveCube(0.0f, +1.0f, 0.0f); 
                break;
            case VK_DOWN:  
                g_Render->MoveCube(0.0f, -1.0f, 0.0f); 
                break;
            case VK_LEFT:  
                g_Render->MoveCube(-1.0f, 0.0f, 0.0f); 
                break;
            case VK_RIGHT: 
                g_Render->MoveCube(+1.0f, 0.0f, 0.0f); 
                break;
            case VK_ADD:
            case 0xBB:
                g_Render->MoveCamera(0.0f, 0.0f, 1.0f);
                break;
            case VK_SUBTRACT:
            case 0xBD:
                g_Render->MoveCamera(0.0f, 0.0f, -1.0f);
                break;
            }
            break;
        }
        return 0;

    case WM_MOUSEMOVE:
        if (ImGui::GetCurrentContext() != nullptr)
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.WantCaptureMouse)
                return 0;
        }
        if (g_Render != nullptr)
        {
            g_Render->MouseMoved(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), hWnd);
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}



