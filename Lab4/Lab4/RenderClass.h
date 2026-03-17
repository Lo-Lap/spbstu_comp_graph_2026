#ifndef RENDER_CLASS_H
#define RENDER_CLASS_H

#include <vector>
#include <string>
#include <algorithm>

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <DirectXMath.h>
#include <commctrl.h>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"



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

        m_pSamplerState(nullptr),

        m_pDepthView(nullptr),
        m_pColorBuffer(nullptr),

        m_pLightBuffer(nullptr),
        m_pLightPixelShader(nullptr),
        m_pMaterialBuffer(nullptr),

        m_pEnvironmentSRV(nullptr),
        m_pSkyVertexShader(nullptr),
        m_pSkyPixelShader(nullptr),
        m_pSkyRasterState(nullptr),
        m_pSkyDepthState(nullptr),

        m_CameraPosition(0.0f, 0.0f, -5.0f),
        m_CameraSpeed(0.1f),
        m_LRAngle(0.0f),
        m_UDAngle(0.0f),

        m_CameraR(5.0f),
        m_CubePosition(0.0f, 0.0f, 0.0f),
        m_CubeMoveSpeed(0.2f),
        m_MouseSensitivity(0.0015f),
        m_MinCameraR(1.5f),
        m_MaxCameraR(50.0f),
        m_rbPressed(false),
        m_prevMouseX(0),
        m_prevMouseY(0),
        m_rotateModel(false),
        m_angle(0.0),

        m_pAnnotation(nullptr),
        m_LuminanceLevels(0),
        m_pLuminancePS(nullptr),
        m_pFullScreenVS(nullptr),
        m_pFullScreenQuadVB(nullptr),
        m_pLuminanceQuery(nullptr),
        m_AdaptedLuminance(0.5f),
        m_CurrentLuminance(0.5f),
        m_pFullScreenLayout(nullptr),
        m_pHDRSceneTexture(nullptr),
        m_pHDRSceneSRV(nullptr),
        m_pLuminanceStagingTextures{},
        m_pHDRSceneRTV(nullptr),
        m_pToneMapPS(nullptr),
        m_pToneMapCB(nullptr),
        m_LastFrameTime(0),
        m_EyeAdaptationTime(2.0f),
        m_pDownsamplePS(nullptr),
        m_indexCount(0),

        m_MaterialMetalness(0.6f),
        m_MaterialRoughness(0.6f),
        m_MaterialAO(1.0f),
        m_NormalStrength(1.0f),
        m_MaterialColor(1.0f, 1.0f, 1.0f),
        m_EnableTextures(true),
        m_DebugViewMode(DebugView_Final),

        m_pHdrToCubemapPS(nullptr)
    {
        for (int i = 0; i < kSphereCount; ++i)
        {
            m_pTextureViews[i] = nullptr;
            m_pNormalMapViews[i] = nullptr;
        }

        for (int i = 0; i < 16; i++)
        {
            m_pLuminanceTextures[i] = nullptr;
            m_pLuminanceRTV[i] = nullptr;
            m_pLuminanceSRV[i] = nullptr;
            m_pLuminanceStagingTextures[i] = nullptr;
        }
    }

    HRESULT Init(HWND hWnd, WCHAR szTitle[], WCHAR szWindowClass[]);
    void Terminate();

    HRESULT InitBufferShader();
    void TerminateBufferShader();

    HRESULT CompileShader(const std::wstring& path, ID3D11VertexShader** ppVertexShader, ID3D11PixelShader** ppPixelShader, ID3DBlob** pCodeShader = nullptr);
    void Render();
    void Resize(HWND hWnd);

    void MoveCube(float dx, float dy, float dz);

    void MoveCamera(float dx, float dy, float dz);
    void RotateCamera(float yaw, float pitch);

    void MouseRBPressed(bool pressed, int x, int y);
    void MouseMoved(int x, int y, HWND hWnd);
    void MouseWheel(int delta);

    void SetLightBrightness(int index, float value);
    float GetLightBrightness(int index) const;

    HWND m_hLightSwatch[3] = { nullptr, nullptr, nullptr };

    HRESULT InitLuminanceResources(UINT width, UINT height);
    void CalculateAverageLuminance();
    float ReadLuminanceFromGPU();
    void ApplyToneMapping();

    void InitImGui(HWND hWnd);
    void ShutdownImGui();
    void RenderImGui();

private:

    HRESULT LoadEnvironmentMap(const wchar_t* path);
    HRESULT LoadHDRTexture2D(const wchar_t* path, ID3D11ShaderResourceView** outSRV);
    HRESULT ConvertHDRIToCubemap(
        ID3D11ShaderResourceView* equirectSRV,
        UINT cubeSize,
        ID3D11ShaderResourceView** outCubeSRV
    );
    bool HasExtension(const std::wstring& path, const std::wstring& ext) const;

    HRESULT ConfigureBackBuffer(UINT width, UINT height);
    void SetMVPBuffer();
    HRESULT CreateHDRSceneTexture(UINT width, UINT height);

    float m_LightBrightness[3] = { 1.0f, 0.9f, 0.9f };
    XMFLOAT3 m_LightColors[3] =
    {
         XMFLOAT3(1.0f, 1.0f, 1.0f),
         XMFLOAT3(1.0f, 1.0f, 1.0f),
         XMFLOAT3(1.0f, 1.0f, 1.0f)
    };


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

    static constexpr int kSphereCount = 4;
    ID3D11ShaderResourceView* m_pTextureViews[kSphereCount];
    ID3D11ShaderResourceView* m_pNormalMapViews[kSphereCount];
    ID3D11SamplerState* m_pSamplerState;

    ID3D11DepthStencilView* m_pDepthView;

    ID3D11Buffer* m_pColorBuffer;

    ID3D11Buffer* m_pLightBuffer;
    ID3D11Buffer* m_pMaterialBuffer;
    ID3D11PixelShader* m_pLightPixelShader;

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
    float m_LRAngle;  
    float m_UDAngle;    

    bool m_rbPressed;
    int m_prevMouseX;
    int m_prevMouseY;
    bool m_rotateModel;
    double m_angle;

    ID3DUserDefinedAnnotation* m_pAnnotation;

    ID3D11Texture2D* m_pLuminanceTextures[16]; 
    ID3D11RenderTargetView* m_pLuminanceRTV[16];
    ID3D11ShaderResourceView* m_pLuminanceSRV[16];
    int m_LuminanceLevels; 

    ID3D11PixelShader* m_pLuminancePS; 
    ID3D11VertexShader* m_pFullScreenVS;
    ID3D11Buffer* m_pFullScreenQuadVB;

    ID3D11Query* m_pLuminanceQuery;

    float m_AdaptedLuminance;
    float m_CurrentLuminance;
    ID3D11InputLayout* m_pFullScreenLayout;

    ID3D11Texture2D* m_pHDRSceneTexture;
    ID3D11ShaderResourceView* m_pHDRSceneSRV;
    ID3D11RenderTargetView* m_pHDRSceneRTV;

    ID3D11Texture2D* m_pLuminanceStagingTextures[16];

    ID3D11PixelShader* m_pToneMapPS;
    ID3D11Buffer* m_pToneMapCB;

    ULONGLONG m_LastFrameTime;
    float m_EyeAdaptationTime;

    ID3D11PixelShader* m_pDownsamplePS;

    UINT m_indexCount;

    ID3D11ShaderResourceView* m_pEnvironmentSRV;
    ID3D11VertexShader* m_pSkyVertexShader;
    ID3D11PixelShader* m_pSkyPixelShader;
    ID3D11RasterizerState* m_pSkyRasterState;
    ID3D11DepthStencilState* m_pSkyDepthState;

    float m_MaterialMetalness;
    float m_MaterialRoughness;
    float m_MaterialAO;
    float m_NormalStrength;
    XMFLOAT3 m_MaterialColor;

    bool m_EnableTextures;

    enum DebugViewMode
    {
        DebugView_Final = 0,
        DebugView_NDF = 1,
        DebugView_Geometry = 2,
        DebugView_Fresnel = 3
    };
    int m_DebugViewMode;

    ID3D11PixelShader* m_pHdrToCubemapPS;
};
#endif
