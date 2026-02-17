#ifndef RENDER_CLASS_H
#define RENDER_CLASS_H

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <DirectXMath.h>

using namespace DirectX;

class RenderClass
{
public:
    RenderClass() :
        m_pDevice(nullptr),
        m_pDeviceContext(nullptr),
        m_pSwapChain(nullptr),
        m_pRenderTargetView(nullptr),
        m_pVertexBuffer(nullptr),
        m_pIndexBuffer(nullptr),
        m_pPixelShader(nullptr),
        m_pVertexShader(nullptr),
        m_pLayout(nullptr),
        m_pModelBuffer(nullptr),
        m_pVPBuffer(nullptr),
        m_szTitle(nullptr),
        m_szWindowClass(nullptr),
        m_CameraPosition(0.0f, 0.0f, -5.0f), 
        m_CameraSpeed(0.1f),
        m_LRAngle(0.0f), 
        m_UDAngle(0.0f),

        m_CameraR(5.0f),
        m_CubePosition(0.0f, 0.0f, 0.0f),
        m_CubeMoveSpeed(0.2f),
        m_MouseSensitivity(0.0005f),
        m_MinCameraR(1.5f),
        m_MaxCameraR(50.0f),
        m_rbPressed(false),
        m_prevMouseX(0),
        m_prevMouseY(0),
        m_rotateModel(false),
        m_angle(0.0),

        m_pAnnotation(nullptr) 

    {}

    HRESULT Init(HWND hWnd, WCHAR szTitle[], WCHAR szWindowClass[]);
    void Terminate();

    HRESULT InitBufferShader();
    void TerminateBufferShader();

    HRESULT CompileShader(const std::wstring& path, ID3DBlob** pCodeShader=nullptr);

    void Render();
    void Resize(HWND hWnd);

    void MoveCube(float dx, float dy, float dz);

    void MoveCamera(float dx, float dy, float dz);
    void RotateCamera(float yaw, float pitch);

    void MouseRBPressed(bool pressed, int x, int y);
    void MouseMoved(int x, int y, HWND hWnd);
    void MouseWheel(int delta);


private:

    HRESULT ConfigureBackBuffer();
    void SetMVPBuffer();

    ID3D11Device* m_pDevice;
    ID3D11DeviceContext* m_pDeviceContext;

    IDXGISwapChain* m_pSwapChain;
    ID3D11RenderTargetView* m_pRenderTargetView;

    ID3D11Buffer* m_pModelBuffer;
    ID3D11Buffer* m_pVPBuffer;

    ID3D11Buffer* m_pVertexBuffer;
    ID3D11Buffer* m_pIndexBuffer;

    ID3D11PixelShader* m_pPixelShader;
    ID3D11VertexShader* m_pVertexShader;
    ID3D11InputLayout* m_pLayout;

    float m_CubeAngle = 0.0f;
    WCHAR* m_szTitle;
    WCHAR* m_szWindowClass;

    XMFLOAT3 m_CubePosition; 
    float m_CubeMoveSpeed;    
    float m_MouseSensitivity; 
    float m_MinCameraR;
    float m_MaxCameraR;

    XMFLOAT3 m_CameraPosition; 
    float m_CameraSpeed;
    float m_CameraR;
    float m_LRAngle;    //turn left/right / m_camera.phi 
    float m_UDAngle;    //turn up / down / m_camera.theta

    bool m_rbPressed;
    int m_prevMouseX;
    int m_prevMouseY;
    bool m_rotateModel;
    double m_angle;

    ID3DUserDefinedAnnotation* m_pAnnotation;

};
#endif
