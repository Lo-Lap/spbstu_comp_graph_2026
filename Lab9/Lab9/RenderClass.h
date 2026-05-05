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

static constexpr UINT SSAO_MAX_SAMPLE_COUNT = 512;
static constexpr UINT SSAO_NOISE_SIZE = 4;
static constexpr UINT SSAO_NOISE_VECTOR_COUNT = SSAO_NOISE_SIZE * SSAO_NOISE_SIZE;

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
        m_pDepthTexture(nullptr),
        m_pDepthSRV(nullptr),
        m_pNormalTexture(nullptr),
        m_pNormalRTV(nullptr),
        m_pNormalSRV(nullptr),
        m_pGBufferAlbedoTexture(nullptr),
        m_pGBufferAlbedoRTV(nullptr),
        m_pGBufferAlbedoSRV(nullptr),
        m_pGBufferMaterialTexture(nullptr),
        m_pGBufferMaterialRTV(nullptr),
        m_pGBufferMaterialSRV(nullptr),
        m_pGBufferEmissiveTexture(nullptr),
        m_pGBufferEmissiveRTV(nullptr),
        m_pGBufferEmissiveSRV(nullptr),
        m_pSSAOTexture(nullptr),
        m_pSSAORTV(nullptr),
        m_pSSAOSRV(nullptr),
        m_pSSAOPS(nullptr),
        m_pSSAOCB(nullptr),
        m_SSAOSampleCount(32),
        m_SSAORadius(1.25f),
        m_SSAOBias(0.025f),
        m_SSAOStrength(1.2f),
        m_SSAOMaxDepthDiff(4.0f),
        m_SSAOMode(1),
        m_EnableSSAO(true),
        m_GroundNormalStrength(0.2f),
        m_EnableGroundNormalMap(true),
        m_ShadowLightYawDeg(-10.0f),
        m_ShadowLightPitchDeg(-61.0f),
        m_pDebugTexturePS(nullptr),
        m_pDebugTextureCB(nullptr),
        m_pGBufferVS(nullptr),
        m_pGBufferPS(nullptr),
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
        m_ShowCascadeSplitColors(false),

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
        m_BloomThreshold(3.5f),
        m_BloomIntensity(0.0f),
        m_BloomBlurScale(0.85f),
        m_EnableFXAA(true),
        m_FXAAStrength(1.35f),

        m_pGroundVB(nullptr),
        m_pGroundIB(nullptr),
        m_GroundIndexCount(0),

        //shadow
        m_pShadowMapTexture(nullptr),
        m_pShadowMapSRV(nullptr),
        m_pShadowVertexShader(nullptr),
        m_pShadowPixelShader(nullptr),
        m_pShadowCameraBuffer(nullptr),
        m_pShadowMaterialBuffer(nullptr),
        m_pShadowParamsBuffer(nullptr),
        m_pShadowLightBuffer(nullptr),
        m_pShadowRasterState(nullptr),
        m_pShadowSampler(nullptr),
        m_pShadowDepthState(nullptr),
        m_ShadowMapSize(4096),
        m_ShadowLightDirection(XMFLOAT3(-0.10f, -1.0f, 0.55f)),
        m_ShadowBias(0.0012f),
        m_ShadowSlopeBias(6.0f),
        m_ShadowStrength(0.95f)

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
        XMFLOAT3 LocalBoundsMin = XMFLOAT3(0, 0, 0);
        XMFLOAT3 LocalBoundsMax = XMFLOAT3(0, 0, 0);
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

    struct ShadowCameraBuffer
    {
        XMMATRIX LightViewProj;
    };

    struct ShadowParamsCB
    {
        XMFLOAT4 ShadowBiasAndTexelSize;
    };

    struct ShadowLightBuffer
    {
        XMMATRIX LightViewProj;
        XMFLOAT3 LightDirection;
        float ShadowStrength;
    };

    struct ShadowMaterialCB
    {
        XMFLOAT4 AlphaParams;
    };

    static constexpr UINT kShadowCascadeCount = 4;

    enum ShadowMode
    {
        ShadowModeSimple = 0,
        ShadowModePSSM = 1,
        ShadowModeCSM = 2
    };

    struct CascadeData
    {
        XMMATRIX LightViewProj = XMMatrixIdentity();
        XMMATRIX WorldToLightUV = XMMatrixIdentity();
        float SplitDepth = 0.0f;
        float Padding[3] = {};
    };

    struct CascadedShadowBuffer
    {
        XMMATRIX WorldToLightUV[kShadowCascadeCount];
        XMFLOAT4 CascadeSplits;
        XMFLOAT4 ShadowLightDirStrength;
        XMFLOAT4 CsmRatio;
        XMFLOAT4 CameraForward;
    };

    struct CascadedShadowParamsCB
    {
        XMFLOAT4 ShadowBiasTexelSizeBlend;
        XMFLOAT4 ShadowOptions;
    };

    void RenderLightSources(const XMMATRIX& viewProj);

    //scene
    std::vector<SceneModelDesc> GetSceneModelDescs() const;
    void LoadSceneModels();
    void BuildSceneLayout();

    //ground plane
    HRESULT CreateGroundPlane(float halfSize = 100.0f, float uvScale = 50.0f);
    void ReleaseGroundPlane();
    void RenderGroundPlane(const XMMATRIX& viewProj);

    // ssao
    HRESULT CreateGBufferResources(UINT width, UINT height);
    void ReleaseGBufferResources();
    HRESULT CreateSSAOResources(UINT width, UINT height);
    void ReleaseSSAOResources();
    void RenderGBufferPass(const XMMATRIX& viewProj);
    void GenerateSSAOKernel();
    void RenderSSAO(const XMMATRIX& cameraView, const XMMATRIX& cameraProj);
    void RenderDebugTexture(ID3D11ShaderResourceView* textureSRV, int mode);
    bool IsFullScreenDebugView() const;

    void RenderGroundPlaneGBuffer();
    void RenderAllSceneModelsGBuffer();
    void RenderModelInstanceGBuffer(const SceneModelInstance& instance);
    void RenderGltfNodeGBuffer(
        const GltfModelResource& model,
        int nodeIndex,
        const XMMATRIX& instanceWorld
    );
    void DrawGltfPrimitiveGBuffer(
        const GltfModelResource& model,
        const GltfGpuPrimitive& primitive,
        const XMMATRIX& world
    );

    //shadow
    void CollectShadowCasters();
    HRESULT CreateShadowResources(UINT shadowMapSize = 2048);
    void ReleaseShadowMapResources();
    void ReleaseShadowResources();

    void RenderCascadedShadowPass();
    void ComputeCascadeSplits();
    void BuildCascadeMatrices(const XMMATRIX& cameraView, const XMMATRIX& cameraProj);

    void RenderGroundPlaneShadow();
    void RenderAllSceneModelsShadow();
    void RenderModelInstanceShadow(const SceneModelInstance& instance);
    void RenderGltfNodeShadow(
        const GltfModelResource& model,
        int nodeIndex,
        const XMMATRIX& instanceWorld
    );
    void DrawGltfPrimitiveShadow(
        const GltfModelResource& model,
        const GltfGpuPrimitive& primitive,
        const XMMATRIX& world

    );
    void UpdateCascadedShadowData(const XMMATRIX& cameraView, const XMMATRIX& cameraProj);

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
    void RenderSkybox(const XMMATRIX& viewProj);

    void UpdateCameraAndLightBuffers(const XMMATRIX& view, const XMMATRIX& viewProj);
    void UpdateShadowLightDirectionFromAngles();

    HRESULT CreateHDRSceneTexture(UINT width, UINT height);

    HRESULT CreateTextureSRVFromFile(const std::wstring& path, ID3D11ShaderResourceView** outSRV);

    bool LoadModelResource(const std::wstring& path, int& outResourceIndex);

    HRESULT CreateGltfGpuResources(GltfModelResource& model);
    void ReleaseGltfGpuResources(GltfModelResource& model);
    void ReleaseAllGltfModelResources();

    void PrecomputeSceneModelTransforms();

    void RenderAllSceneModels(const XMMATRIX& viewProj);

    void RenderModelInstance(
        const SceneModelInstance& instance,
        const XMMATRIX& viewProj
    );

    void RenderGltfNode(
        const GltfModelResource& model,
        int nodeIndex,
        const XMMATRIX& viewProj,
        const XMMATRIX& instanceWorld,
        bool receiveShadow
    );

    void DrawGltfPrimitive(
        const GltfModelResource& model,
        const GltfGpuPrimitive& primitive,
        const XMMATRIX& world,
        const XMMATRIX& viewProj,
        bool receiveShadow
    );

    //bloom
    HRESULT CreateBloomResources(UINT width, UINT height);
    void ReleaseBloomResources();
    void ApplyBloom();


private:
    //float m_LightBrightness[3] = { 1.0f, 0.9f, 0.9f };
    float m_LightBrightness[3] = { 0.9f, 0.0f, 0.0f };

    XMFLOAT3 m_LightColors[3] =
    {
         XMFLOAT3(1.0f, 1.0f, 1.0f),
         XMFLOAT3(1.0f, 1.0f, 1.0f),
         XMFLOAT3(1.0f, 1.0f, 1.0f)
    };

    //XMFLOAT3 m_LightPositions[3] =
    //{
    // XMFLOAT3(0.0f, 5.5f, -5.5f),
    // XMFLOAT3(-6.5f, 10.0f, 2.0f),
    // XMFLOAT3(5.5f, 5.0f, 0.0f)
    //};

    XMFLOAT3 m_LightPositions[3] =
    {
     XMFLOAT3(0.5f, 20.0f, 0.0f),
     XMFLOAT3(-6.5f, 10.0f, 2.0f),
     XMFLOAT3(5.5f, 5.0f, 0.0f)
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
    ID3D11Texture2D* m_pDepthTexture;
    ID3D11ShaderResourceView* m_pDepthSRV;

    ID3D11Texture2D* m_pNormalTexture;
    ID3D11RenderTargetView* m_pNormalRTV;
    ID3D11ShaderResourceView* m_pNormalSRV;

    ID3D11Texture2D* m_pGBufferAlbedoTexture;
    ID3D11RenderTargetView* m_pGBufferAlbedoRTV;
    ID3D11ShaderResourceView* m_pGBufferAlbedoSRV;
    ID3D11Texture2D* m_pGBufferMaterialTexture;
    ID3D11RenderTargetView* m_pGBufferMaterialRTV;
    ID3D11ShaderResourceView* m_pGBufferMaterialSRV;
    ID3D11Texture2D* m_pGBufferEmissiveTexture;
    ID3D11RenderTargetView* m_pGBufferEmissiveRTV;
    ID3D11ShaderResourceView* m_pGBufferEmissiveSRV;

    ID3D11Texture2D* m_pSSAOTexture;
    ID3D11RenderTargetView* m_pSSAORTV;
    ID3D11ShaderResourceView* m_pSSAOSRV;

    ID3D11PixelShader* m_pSSAOPS;
    ID3D11Buffer* m_pSSAOCB;
    XMFLOAT4 m_SSAOSphereSamples[SSAO_MAX_SAMPLE_COUNT];
    XMFLOAT4 m_SSAOHemisphereSamples[SSAO_MAX_SAMPLE_COUNT];
    XMFLOAT4 m_SSAONoise[16];
    UINT m_SSAOSampleCount;
    int m_SSAOMode;
    float m_SSAORadius;
    float m_SSAOBias;
    float m_SSAOStrength;
    float m_SSAOMaxDepthDiff;
    bool m_EnableSSAO;

    ID3D11PixelShader* m_pDebugTexturePS;
    ID3D11Buffer* m_pDebugTextureCB;

    ID3D11VertexShader* m_pGBufferVS;
    ID3D11PixelShader* m_pGBufferPS;

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
    float m_DiffuseIBLIntensity = 0.18f;
    float m_SpecularIBLIntensity = 0.30f;

    enum DebugViewMode
    {
        DebugView_Final = 0,
        DebugView_NDF = 1,
        DebugView_Geometry = 2,
        DebugView_Fresnel = 3,
        DebugView_DiffuseIBL = 4,
        DebugView_SpecularIBL = 5,
        DebugView_AmbientIBL = 6,
        DebugView_ReflectionOnly = 7,
        DebugView_SSAO = 8,
        DebugView_NormalBuffer = 9,
        DebugView_DepthBuffer = 10,
        DebugView_GroundNormalMapMarkers = 11,
        DebugView_GBufferAlbedo = 12,
        DebugView_GBufferMaterial = 13,
        DebugView_GBufferEmissive = 14
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

    bool m_EnableFXAA;
    float m_FXAAStrength;

    ID3D11Buffer* m_pGroundVB;
    ID3D11Buffer* m_pGroundIB;
    UINT m_GroundIndexCount;
    float m_GroundHalfSize = 100.0f;
    float m_GroundNormalStrength;
    bool m_EnableGroundNormalMap;

    std::vector<GltfModelResource> m_ModelResources;
    std::vector<SceneModelInstance> m_SceneModelInstances;

    //shadow
    std::vector<SceneShadowItem> m_ShadowCasters;

    ID3D11Texture2D* m_pShadowMapTexture;
    ID3D11DepthStencilView* m_pShadowMapDSV[kShadowCascadeCount] = {};
    ID3D11ShaderResourceView* m_pShadowMapSRV;

    ID3D11VertexShader* m_pShadowVertexShader;
    ID3D11PixelShader* m_pShadowPixelShader;

    ID3D11Buffer* m_pShadowCameraBuffer;
    ID3D11Buffer* m_pShadowMaterialBuffer;

    ID3D11Buffer* m_pShadowParamsBuffer;
    ID3D11Buffer* m_pShadowLightBuffer;

    ID3D11RasterizerState* m_pShadowRasterState;
    ID3D11SamplerState* m_pShadowSampler;
    ID3D11DepthStencilState* m_pShadowDepthState;
    UINT m_ShadowMapSize;
    XMFLOAT3 m_ShadowLightDirection;
    float m_ShadowLightYawDeg;
    float m_ShadowLightPitchDeg;
    float m_ShadowBias;
    float m_ShadowSlopeBias;
    float m_ShadowStrength;

    int m_ShadowDepthBias = 4096;
    float m_ShadowSlopeScaledDepthBias = 6.0f;
    float m_ShadowDepthBiasClamp = 0.001f;

    float m_ShadowReceiverConstBias = 0.00035f;
    float m_ShadowReceiverSlopeBias = 0.0015f;

    float m_ShadowPcfMinRadius = 1.25f;
    float m_ShadowPcfMaxRadius = 5.0f;

    CascadeData m_CascadeData[kShadowCascadeCount];
    float m_CascadeSplits[kShadowCascadeCount] = { 0.05f, 0.15f, 0.35f, 1.0f };
    float m_CascadeWorldHalfSize[kShadowCascadeCount] = { 12.0f, 28.0f, 60.0f, 120.0f };

    float m_CascadeLambda = 0.65f;
    float m_CascadeBlendBand = 0.08f;

    ShadowMode m_ShadowMode = ShadowModeCSM;

    bool  m_TintSplits = false;

    bool m_ShowCascadeSplitColors;

    float m_CameraNearZ = 0.1f;
    float m_CameraFarZ = 350.0f;

    float m_ShadowSplitDists[kShadowCascadeCount] = { 10.0f, 33.0f, 100.0f, 300.0f };
    float m_ShadowCascadeFarZ = 120.0f;

};
#endif
