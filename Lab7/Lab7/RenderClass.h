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

#include "GltfLoader.h"

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
        m_pSkyLayout(nullptr),
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
        m_pGltfRasterState(nullptr),

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
        m_EnableTextures(false),
        m_DebugViewMode(DebugView_Final),

        m_pPrefilteredEnvSRV(nullptr),
        m_pSpecularPrefilterPS(nullptr),
        m_pSpecularPrefilterCB(nullptr),

        m_pIrradianceSRV(nullptr),
        m_pIrradianceConvolutionPS(nullptr),

        m_pHdrToCubemapPS(nullptr),

        m_pBRDFLUTSRV(nullptr),
        m_pBRDFIntegrationPS(nullptr),

        m_pBloomTextureA(nullptr),
        m_pBloomRTVA(nullptr),
        m_pBloomSRVA(nullptr),
        m_pBloomTextureB(nullptr),
        m_pBloomRTVB(nullptr),
        m_pBloomSRVB(nullptr),
        m_pBloomExtractPS(nullptr),
        m_pBloomBlurPS(nullptr),
        m_pBloomCB(nullptr),
        m_EnableBloom(true),
        m_BloomThreshold(1.25f),
        m_BloomIntensity(1.0f),
        m_BloomBlurScale(1.0f),

        m_pGroundVB(nullptr),
        m_pGroundIB(nullptr),
        m_GroundIndexCount(0)

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

    HRESULT PrefilterCubemapSpecular(
        ID3D11ShaderResourceView* environmentCubeSRV,
        UINT prefilterSize,
        UINT mipLevels,
        ID3D11ShaderResourceView** outPrefilterSRV
    );


private:
    struct GltfGpuPrimitive
    {
        ID3D11Buffer* VertexBuffer = nullptr;
        ID3D11Buffer* IndexBuffer = nullptr;
        UINT IndexCount = 0;
        DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;
        int MaterialIndex = -1;
    };

    struct GltfGpuMesh
    {
        std::vector<GltfGpuPrimitive> Primitives;
    };

    struct GroundVertex
    {
        XMFLOAT3 Position;
        XMFLOAT3 Normal;
        XMFLOAT2 TexCoord;
    };

    struct SceneShadowItem
    {
        XMMATRIX World = XMMatrixIdentity();
        UINT IndexCount = 0;
        bool IsGround = false;
    };

    struct GltfModelResource
    {
        std::wstring FilePath;
        LoadedGltfScene Scene;
        std::vector<GltfGpuMesh> GpuMeshes;
        std::vector<ID3D11ShaderResourceView*> TextureSRVs;
    };

    struct SceneModelDesc
    {
        std::wstring FilePath;

        XMFLOAT3 Position = XMFLOAT3(0, 0, 0);
        XMFLOAT3 RotationDeg = XMFLOAT3(0, 0, 0);
        XMFLOAT3 Scale = XMFLOAT3(1, 1, 1);

        bool CastShadow = true;
        bool ReceiveShadow = true;
    };

    struct SceneModelInstance
    {
        int ModelResourceIndex = -1;

        XMFLOAT3 Position = XMFLOAT3(0, 0, 0);
        XMFLOAT3 RotationDeg = XMFLOAT3(0, 0, 0);
        XMFLOAT3 Scale = XMFLOAT3(1, 1, 1);

        XMMATRIX PrecomputedWorld = XMMatrixIdentity();

        bool CastShadow = true;
        bool ReceiveShadow = true;
    };

    std::vector<SceneModelDesc> GetSceneModelDescs() const;
    void LoadSceneModels();

    HRESULT CreateGroundPlane(float halfSize = 20.0f, float uvScale = 8.0f);
    void ReleaseGroundPlane();

    void BuildSceneLayout();
    void RenderGroundPlane(const XMMATRIX& viewProj);

    void CollectShadowCasters();

    HRESULT LoadEnvironmentMap(const wchar_t* path);
    HRESULT LoadHDRTexture2D(const wchar_t* path, ID3D11ShaderResourceView** outSRV);
    HRESULT ConvertHDRIToCubemap(
        ID3D11ShaderResourceView* equirectSRV,
        UINT cubeSize,
        ID3D11ShaderResourceView** outCubeSRV
    );

    HRESULT ConvolveCubemapToIrradiance(
        ID3D11ShaderResourceView* environmentCubeSRV,
        UINT irradianceSize,
        ID3D11ShaderResourceView** outIrradianceSRV
    );

    HRESULT GenerateBRDFLUT(
        UINT lutWidth,
        UINT lutHeight,
        ID3D11ShaderResourceView** outBRDFLUTSRV
    );

    bool HasExtension(const std::wstring& path, const std::wstring& ext) const;
    void ScanCubeMapsFolder();

    HRESULT ConfigureBackBuffer(UINT width, UINT height);
    //void SetMVPBuffer();
    void RenderSkybox(const XMMATRIX& viewProj);

    void UpdateCameraAndLightBuffers(const XMMATRIX& viewProj);
    HRESULT CreateHDRSceneTexture(UINT width, UINT height);

    //bool LoadModelFromFile(const std::wstring& path);

    //HRESULT CreateGltfGpuResources();
    //void ReleaseGltfGpuResources();
    HRESULT CreateTextureSRVFromFile(const std::wstring& path, ID3D11ShaderResourceView** outSRV);
    /*void RenderGltfScene(const XMMATRIX& viewProj);
    void DrawGltfPrimitive(const GltfGpuPrimitive& primitive, const XMMATRIX& world, const XMMATRIX& viewProj);*/

    bool LoadModelResource(const std::wstring& path, int& outResourceIndex);

    HRESULT CreateGltfGpuResources(GltfModelResource& model);
    void ReleaseGltfGpuResources(GltfModelResource& model);
    void ReleaseAllGltfModelResources();

    void PrecomputeSceneModelTransforms();

    void RenderAllSceneModels(const XMMATRIX& viewProj);
    void RenderModelInstance(const SceneModelInstance& instance, const XMMATRIX& viewProj);
    void RenderGltfNode(
        const GltfModelResource& model,
        int nodeIndex,
        const XMMATRIX& viewProj,
        const XMMATRIX& instanceWorld
    );

    void DrawGltfPrimitive(
        const GltfModelResource& model,
        const GltfGpuPrimitive& primitive,
        const XMMATRIX& world,
        const XMMATRIX& viewProj
    );

    HRESULT CreateBloomResources(UINT width, UINT height);
    void ReleaseBloomResources();
    void ApplyBloom();


private:
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
    ID3D11InputLayout* m_pSkyLayout;

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

    ID3D11RasterizerState* m_pGltfRasterState;

    float m_MaterialMetalness;
    float m_MaterialRoughness;
    float m_MaterialAO;
    float m_NormalStrength;
    XMFLOAT3 m_MaterialColor;

    bool m_EnableTextures;
    bool m_EnableSpecularIBL = true;
    float m_DiffuseIBLIntensity = 1.0f;
    float m_SpecularIBLIntensity = 1.0f;

    enum DebugViewMode
    {
        DebugView_Final = 0,
        DebugView_NDF = 1,
        DebugView_Geometry = 2,
        DebugView_Fresnel = 3,
        DebugView_DiffuseIBL = 4,
        DebugView_SpecularIBL = 5,
        DebugView_AmbientIBL = 6,
        DebugView_ReflectionOnly = 7
    };
    int m_DebugViewMode;

    ID3D11PixelShader* m_pHdrToCubemapPS;

    std::vector<std::wstring> m_environmentFiles;
    std::vector<std::string> m_environmentFileNames;
    int m_currentEnvIndex;
    int m_prevEnvIndex;

    ID3D11ShaderResourceView* m_pIrradianceSRV;
    ID3D11PixelShader* m_pIrradianceConvolutionPS;

    ID3D11ShaderResourceView* m_pPrefilteredEnvSRV;
    ID3D11PixelShader* m_pSpecularPrefilterPS;
    ID3D11ShaderResourceView* m_pBRDFLUTSRV;
    ID3D11PixelShader* m_pBRDFIntegrationPS;

    ID3D11Buffer* m_pSpecularPrefilterCB;

    ID3D11Texture2D* m_pBloomTextureA;
    ID3D11RenderTargetView* m_pBloomRTVA;
    ID3D11ShaderResourceView* m_pBloomSRVA;
    ID3D11Texture2D* m_pBloomTextureB;
    ID3D11RenderTargetView* m_pBloomRTVB;
    ID3D11ShaderResourceView* m_pBloomSRVB;
    ID3D11PixelShader* m_pBloomExtractPS;
    ID3D11PixelShader* m_pBloomBlurPS;
    ID3D11Buffer* m_pBloomCB;
    bool m_EnableBloom;
    float m_BloomThreshold;
    float m_BloomIntensity;
    float m_BloomBlurScale;


    ID3D11Buffer* m_pGroundVB;
    ID3D11Buffer* m_pGroundIB;
    UINT m_GroundIndexCount;

    std::vector<SceneShadowItem> m_ShadowCasters;

    std::vector<GltfModelResource> m_ModelResources;
    std::vector<SceneModelInstance> m_SceneModelInstances;

};
#endif
