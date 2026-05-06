#include "framework.h"
#include "RenderClass.h"
#include "DDSTextureLoader11.h"

#include <filesystem>
#include <random>

#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "src/stb_image.h"

#pragma comment(lib, "comctl32.lib")


struct CubeVertex
{
    XMFLOAT3 xyz;
    XMFLOAT3 normal;
    XMFLOAT2 uv;
};

struct MatrixBuffer
{
    XMMATRIX m;
};

struct CameraBuffer
{
    XMMATRIX vp;
    XMMATRIX view;
    XMFLOAT3 cameraPos;
    float padding;

};

struct ColorBuffer
{
    XMFLOAT4 color;
};

struct PointLight
{
    XMFLOAT3 Position;
    float Range;
    XMFLOAT3 Color;
    float Intensity;
};

struct DeferredLightingFrameCB
{
    XMMATRIX InvViewProj;
    XMFLOAT4 CameraPositionLightCount;
    XMFLOAT4 ScreenSizeParams;
    XMFLOAT4 DirectionalLight;
    XMFLOAT4 IBLParams;
};

struct DeferredLightingLightCB
{
    XMFLOAT4 PositionRange;
    XMFLOAT4 ColorIntensity;
    XMFLOAT4 Params;
};

struct FullScreenVertex
{
    XMFLOAT3 Pos;
    XMFLOAT2 TexCoord;
};

struct ToneMapParamsCB
{
    XMFLOAT4 Params;
};

struct MaterialParamsCB
{
    XMFLOAT4 Surface;
    XMFLOAT4 Albedo;
    XMFLOAT4 DebugView;
    XMFLOAT4 Emissive;
    XMFLOAT4 Extra;
    XMFLOAT4 AlphaParams;
    XMFLOAT4 TextureFlags;
};

struct BloomParamsCB
{
    XMFLOAT4 Params0;
    XMFLOAT4 Params1;
};

struct DebugTextureParamsCB
{
    XMFLOAT4 Params;
};

struct SpecularPrefilterCB
{
    XMFLOAT4 Params;
};

struct SSAOParamsCB
{
    XMMATRIX Proj;
    XMMATRIX InvProj;
    XMMATRIX View;
    XMFLOAT4 Params0; 
    XMFLOAT4 Params1; 
    XMFLOAT4 Samples[SSAO_MAX_SAMPLE_COUNT];
    XMFLOAT4 Noise[SSAO_NOISE_VECTOR_COUNT];
};

static XMMATRIX BuildViewMatrix(
    const XMFLOAT3& cameraPos,
    float lrAngle,
    float udAngle)
{
    XMVECTOR direction = XMVectorSet(
        cosf(udAngle) * sinf(lrAngle),
        sinf(udAngle),
        cosf(udAngle) * cosf(lrAngle),
        0.0f
    );
    XMVECTOR eyePos = XMVectorSet(cameraPos.x, cameraPos.y, cameraPos.z, 1.0f);
    XMVECTOR focusPoint = XMVectorAdd(eyePos, direction);
    XMVECTOR upDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    return XMMatrixLookAtLH(eyePos, focusPoint, upDir);
}

static XMMATRIX BuildProjectionMatrix(float aspect, float nearZ, float farZ)
{
    return XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        aspect,
        nearZ,
        farZ
    );
}

void RenderClass::UpdateShadowLightDirectionFromAngles()
{
    m_ShadowLightPitchDeg = std::clamp(m_ShadowLightPitchDeg, -89.0f, 89.0f);

    const float yaw = XMConvertToRadians(m_ShadowLightYawDeg);
    const float pitch = XMConvertToRadians(m_ShadowLightPitchDeg);

    XMVECTOR dir = XMVectorSet(
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw),
        0.0f
    );
    dir = XMVector3Normalize(dir);
    XMStoreFloat3(&m_ShadowLightDirection, dir);
}

static std::wstring ToLowerCopy(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
};

bool RenderClass::HasExtension(const std::wstring& path, const std::wstring& ext) const
{
    std::filesystem::path p(path);
    return ToLowerCopy(p.extension().wstring()) == ToLowerCopy(ext);
};

std::vector<RenderClass::SceneModelDesc> RenderClass::GetSceneModelDescs() const
{
    return
    {
        {
            L"models/penguin/penguin.gltf",
            XMFLOAT3(-2.5f, 0.0f, 0.0f),
            XMFLOAT3(90.0f, 135.0f, 0.0f),
            XMFLOAT3(0.1f, 0.1f, 0.1f),
            true,
            true
        },
        {
            L"models/cakepop/Cake_ Pop.gltf",
            XMFLOAT3(10.0f, 0.0f, 1.5f),
            XMFLOAT3(0.0f, 180.0f, 0.0f),
            XMFLOAT3(1.0f, 1.0f, 1.0f),
            true,
            true
        },
        {
            L"models/bunny/bunny_blend.gltf",
            XMFLOAT3(10.0f, 0.0f, 1.5f),
            XMFLOAT3(0.0f, -135.0f, 0.0f),
            XMFLOAT3(1.0f, 1.0f, 1.0f),
            true,
            true
        }/*,
        {
            L"models/columns/colonne.gltf",
            XMFLOAT3(-10.0f, 0.0f, 30.5f),
            XMFLOAT3(0.0f, -45.0f, 0.0f),
            XMFLOAT3(0.02f, 0.02f, 0.02f),
            true,
            true
        }*/
    };
}

HRESULT RenderClass::Init(HWND hWnd, WCHAR szTitle[], WCHAR szWindowClass[])
{
    m_szTitle = szTitle;
    m_szWindowClass = szWindowClass;

    HRESULT result;

    IDXGIFactory* pFactory = nullptr;
    result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);

    IDXGIAdapter* pSelectedAdapter = NULL;
    if (SUCCEEDED(result))
    {
        IDXGIAdapter* pAdapter = NULL;
        UINT adapterIdx = 0;
        while (SUCCEEDED(pFactory->EnumAdapters(adapterIdx, &pAdapter)))
        {
            DXGI_ADAPTER_DESC desc;
            pAdapter->GetDesc(&desc);

            if (wcscmp(desc.Description, L"Microsoft Basic Render Driver") != 0)
            {
                pSelectedAdapter = pAdapter;
                break;
            }

            pAdapter->Release();

            adapterIdx++;
        }
    }

    D3D_FEATURE_LEVEL level;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    if (SUCCEEDED(result))
    {
        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        result = D3D11CreateDevice(pSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL,
            flags, levels, 1, D3D11_SDK_VERSION, &m_pDevice, &level, &m_pDeviceContext);
    }

    if (SUCCEEDED(result) && m_pDeviceContext)
        m_pDeviceContext->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&m_pAnnotation);

    if (SUCCEEDED(result))
    {
        DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hWnd;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Windowed = true;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.Flags = 0;

        result = pFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
    }

    if (SUCCEEDED(result))
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        UINT width = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;
        result = ConfigureBackBuffer(width, height);

        if (SUCCEEDED(result))
            result = CreateGBufferResources(width, height);

        if (SUCCEEDED(result))
            result = CreateSSAOResources(width, height);

        if (SUCCEEDED(result))
            result = CreateHDRSceneTexture(width, height);

        if (SUCCEEDED(result))
            result = CreateBloomResources(width, height);

        if (SUCCEEDED(result))
            result = InitLuminanceResources(width, height);

        D3D11_VIEWPORT vp = {};
        vp.Width = (FLOAT)width;
        vp.Height = (FLOAT)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        m_pDeviceContext->RSSetViewports(1, &vp);
    }

    if (SUCCEEDED(result))
        result = InitBufferShader();

    if (SUCCEEDED(result))
    {
        result = CreateShadowResources(m_ShadowMapSize);
        if (FAILED(result))
            return result;
    }

    if (SUCCEEDED(result))
    {
        result = CreateGroundPlane(140.0f, 1.0f);
        if (FAILED(result))
            return result;

        LoadSceneModels();
        BuildDeferredPointLights();
    }

    if (SUCCEEDED(result))
    {
        m_CameraR = 5.0f;
        m_CameraPosition = XMFLOAT3(0.0f, 10.0f, -m_CameraR * 7);
        m_CubeAngle = 0.0f;
        m_LRAngle = 0.0f;
        m_UDAngle = 0.0f;
        m_CubePosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    if (pSelectedAdapter)
        pSelectedAdapter->Release();

    if (pFactory)
        pFactory->Release();

    if (FAILED(result))
        Terminate();

    InitImGui(hWnd);

    return result;
}

HRESULT RenderClass::InitBufferShader()
{
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GltfVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GltfVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(GltfVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    HRESULT result = S_OK;

    ID3DBlob* pVertexCode = nullptr;
    if (SUCCEEDED(result))
        result = CompileShader(L"ColorVertex.vs", &m_pVertexShader, nullptr, &pVertexCode);

    if (SUCCEEDED(result))
        result = CompileShader(L"ColorPixel.ps", nullptr, &m_pPixelShader);

    if (SUCCEEDED(result))
        result = CompileShader(L"GBufferVertex.vs", &m_pGBufferVS, nullptr);

    if (SUCCEEDED(result))
        result = CompileShader(L"GBufferPixel.ps", nullptr, &m_pGBufferPS);

    if (SUCCEEDED(result))
        result = CompileShader(L"DeferredLighting.ps", nullptr, &m_pDeferredLightingPS);

    if (SUCCEEDED(result))
        result = CompileShader(L"SSAO.ps", nullptr, &m_pSSAOPS);

    if (SUCCEEDED(result))
        result = CompileShader(L"DebugTexture.ps", nullptr, &m_pDebugTexturePS);

    if (SUCCEEDED(result))
        result = CompileShader(L"ShadowVertex.vs", &m_pShadowVertexShader, nullptr);

    if (SUCCEEDED(result))
        result = CompileShader(L"ShadowPixel.ps", nullptr, &m_pShadowPixelShader);

    if (FAILED(result))
        return result;

    D3D11_SAMPLER_DESC shadowSamplerDesc = {};
    shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.MipLODBias = 0.0f;
    shadowSamplerDesc.MaxAnisotropy = 1;
    shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    shadowSamplerDesc.BorderColor[0] = 1.0f;
    shadowSamplerDesc.BorderColor[1] = 1.0f;
    shadowSamplerDesc.BorderColor[2] = 1.0f;
    shadowSamplerDesc.BorderColor[3] = 1.0f;
    shadowSamplerDesc.MinLOD = 0.0f;
    shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    result = m_pDevice->CreateSamplerState(
        &shadowSamplerDesc,
        &m_pShadowSampler
    );

    if (FAILED(result) || m_pShadowSampler == nullptr)
    {
        MessageBox(
            nullptr,
            L"CreateSamplerState failed for m_pShadowSampler",
            L"D3D11 Error",
            MB_OK
        );
        return FAILED(result) ? result : E_FAIL;
    }


    D3D11_RASTERIZER_DESC shadowRsDesc = {};
    shadowRsDesc.FillMode = D3D11_FILL_SOLID;
    shadowRsDesc.CullMode = D3D11_CULL_BACK;

    shadowRsDesc.FrontCounterClockwise = FALSE;
    shadowRsDesc.DepthClipEnable = TRUE;

    shadowRsDesc.DepthBias = m_ShadowDepthBias;
    shadowRsDesc.SlopeScaledDepthBias = m_ShadowSlopeScaledDepthBias;
    shadowRsDesc.DepthBiasClamp = m_ShadowDepthBiasClamp;

    result = m_pDevice->CreateRasterizerState(&shadowRsDesc, &m_pShadowRasterState);

    if (FAILED(result))
        return result;

    if (SUCCEEDED(result))
        result = m_pDevice->CreateInputLayout(layout, 3, pVertexCode->GetBufferPointer(), pVertexCode->GetBufferSize(), &m_pLayout);

    if (pVertexCode)
        pVertexCode->Release();

    if (SUCCEEDED(result))
        result = CompileShader(L"LightPixel.ps", nullptr, &m_pLightPixelShader);

    result = CompileShader(L"ToneMapPixel.ps", nullptr, &m_pToneMapPS);

    if (SUCCEEDED(result))
        result = CompileShader(L"BloomExtract.ps", nullptr, &m_pBloomExtractPS);

    if (SUCCEEDED(result))
        result = CompileShader(L"BloomBlur.ps", nullptr, &m_pBloomBlurPS);

    if (FAILED(result))
        return result;


    ID3DBlob* pSkyVSCode = nullptr;
    if (SUCCEEDED(result))
        result = CompileShader(L"SkyVertex.vs", &m_pSkyVertexShader, nullptr, &pSkyVSCode);

    if (SUCCEEDED(result))
    {
        D3D11_INPUT_ELEMENT_DESC skyLayout[] =
        {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        result = m_pDevice->CreateInputLayout(
            skyLayout,
            1,
            pSkyVSCode->GetBufferPointer(),
            pSkyVSCode->GetBufferSize(),
            &m_pSkyLayout
        );
    }
    if (pSkyVSCode)
        pSkyVSCode->Release();

    if (SUCCEEDED(result))
        result = CompileShader(L"SkyPixel.ps", nullptr, &m_pSkyPixelShader);

    if (FAILED(result))
        return result;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = sizeof(ToneMapParamsCB);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pToneMapCB);
    if (FAILED(result))
        return result;

    D3D11_BUFFER_DESC bloomCBDesc = {};
    bloomCBDesc.Usage = D3D11_USAGE_DYNAMIC;
    bloomCBDesc.ByteWidth = sizeof(BloomParamsCB);
    bloomCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bloomCBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = m_pDevice->CreateBuffer(&bloomCBDesc, nullptr, &m_pBloomCB);
    if (FAILED(result))
        return result;

    GenerateSSAOKernel();

    D3D11_BUFFER_DESC ssaoCBDesc = {};
    ssaoCBDesc.Usage = D3D11_USAGE_DYNAMIC;
    ssaoCBDesc.ByteWidth = sizeof(SSAOParamsCB);
    ssaoCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ssaoCBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = m_pDevice->CreateBuffer(&ssaoCBDesc, nullptr, &m_pSSAOCB);
    if (FAILED(result))
        return result;

    D3D11_BUFFER_DESC debugTextureCBDesc = {};
    debugTextureCBDesc.Usage = D3D11_USAGE_DYNAMIC;
    debugTextureCBDesc.ByteWidth = sizeof(DebugTextureParamsCB);
    debugTextureCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    debugTextureCBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = m_pDevice->CreateBuffer(&debugTextureCBDesc, nullptr, &m_pDebugTextureCB);
    if (FAILED(result))
        return result;

    D3D11_BUFFER_DESC deferredFrameCBDesc = {};
    deferredFrameCBDesc.Usage = D3D11_USAGE_DYNAMIC;
    deferredFrameCBDesc.ByteWidth = sizeof(DeferredLightingFrameCB);
    deferredFrameCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    deferredFrameCBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = m_pDevice->CreateBuffer(&deferredFrameCBDesc, nullptr, &m_pDeferredFrameBuffer);
    if (FAILED(result))
        return result;

    D3D11_BUFFER_DESC deferredLightCBDesc = {};
    deferredLightCBDesc.Usage = D3D11_USAGE_DYNAMIC;
    deferredLightCBDesc.ByteWidth = sizeof(DeferredLightingLightCB);
    deferredLightCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    deferredLightCBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = m_pDevice->CreateBuffer(&deferredLightCBDesc, nullptr, &m_pDeferredLightBuffer);
    if (FAILED(result))
        return result;

    D3D11_DEPTH_STENCIL_DESC deferredDepthOffDesc = {};
    deferredDepthOffDesc.DepthEnable = FALSE;
    deferredDepthOffDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    deferredDepthOffDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    result = m_pDevice->CreateDepthStencilState(&deferredDepthOffDesc, &m_pDeferredLightingDepthOffState);
    if (FAILED(result))
        return result;

    D3D11_DEPTH_STENCIL_DESC pointDepthDesc = {};
    pointDepthDesc.DepthEnable = TRUE;
    pointDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    pointDepthDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
    result = m_pDevice->CreateDepthStencilState(&pointDepthDesc, &m_pPointLightDepthState);
    if (FAILED(result))
        return result;

    D3D11_RASTERIZER_DESC pointRasterDesc = {};
    pointRasterDesc.FillMode = D3D11_FILL_SOLID;
    pointRasterDesc.CullMode = D3D11_CULL_FRONT;
    pointRasterDesc.FrontCounterClockwise = FALSE;
    pointRasterDesc.DepthClipEnable = TRUE;
    result = m_pDevice->CreateRasterizerState(&pointRasterDesc, &m_pPointLightRasterState);
    if (FAILED(result))
        return result;

    D3D11_BLEND_DESC additiveBlendDesc = {};
    additiveBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    additiveBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    additiveBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    additiveBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    additiveBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    additiveBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    additiveBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    additiveBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    result = m_pDevice->CreateBlendState(&additiveBlendDesc, &m_pAdditiveBlendState);
    if (FAILED(result))
        return result;

    const int stacks = 32;
    const int slices = 64;
    const float radius = 1.0f;
    std::vector<CubeVertex> vertices;
    std::vector<WORD> indices;
    vertices.reserve((stacks + 1) * (slices + 1));
    indices.reserve(stacks * slices * 6);
    const float PI = 3.14159265358979323846f;
    for (int i = 0; i <= stacks; ++i)
    {
        float v = (float)i / stacks;
        float phi = v * PI;
        for (int j = 0; j <= slices; ++j)
        {
            float u = (float)j / slices;
            float theta = u * 2.0f * PI;
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);
            CubeVertex vert;
            vert.xyz = XMFLOAT3(radius * x, radius * y, radius * z);
            vert.normal = XMFLOAT3(x, y, z);
            vert.uv = XMFLOAT2(u, 1.0f - v);
            vertices.push_back(vert);
        }
    }
    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            WORD a = (WORD)(i * (slices + 1) + j);
            WORD b = (WORD)(a + (slices + 1));
            WORD c = (WORD)(a + 1);
            WORD d = (WORD)(b + 1);
            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(b);

            indices.push_back(c);
            indices.push_back(d);
            indices.push_back(b);
        }
    }

    m_indexCount = (UINT)indices.size();

    D3D11_BUFFER_DESC lightBufferDesc = {};
    lightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    lightBufferDesc.ByteWidth = sizeof(PointLight) * 3;
    lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    lightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = m_pDevice->CreateBuffer(&lightBufferDesc, nullptr, &m_pLightBuffer);
    if (FAILED(result))
        return result;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = (UINT)(sizeof(CubeVertex) * vertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices.data();
    result = m_pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer);
    if (FAILED(result))
        return result;

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = (UINT)(sizeof(WORD) * indices.size());
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = indices.data();
    result = m_pDevice->CreateBuffer(&bd, &initData, &m_pIndexBuffer);
    if (FAILED(result))
        return result;

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(XMMATRIX);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    result = m_pDevice->CreateBuffer(&bd, nullptr, &m_pModelBuffer);
    if (FAILED(result))
        return result;

    D3D11_BUFFER_DESC vpBufferDesc = {};
    vpBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vpBufferDesc.ByteWidth = sizeof(CameraBuffer);
    vpBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vpBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = m_pDevice->CreateBuffer(&vpBufferDesc, nullptr, &m_pVPBuffer);
    if (FAILED(result))
        return result;

    D3D11_BUFFER_DESC colorBufferDesc = {};
    colorBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    colorBufferDesc.ByteWidth = sizeof(ColorBuffer);
    colorBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    colorBufferDesc.CPUAccessFlags = 0;
    result = m_pDevice->CreateBuffer(&colorBufferDesc, nullptr, &m_pColorBuffer);
    if (FAILED(result))
        return result;

    D3D11_BUFFER_DESC materialBufferDesc = {};
    materialBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    materialBufferDesc.ByteWidth = sizeof(MaterialParamsCB);
    materialBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    materialBufferDesc.CPUAccessFlags = 0;
    result = m_pDevice->CreateBuffer(&materialBufferDesc, nullptr, &m_pMaterialBuffer);
    if (FAILED(result))
        return result;

    D3D11_BUFFER_DESC shadowMaterialBufferDesc = {};
    shadowMaterialBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    shadowMaterialBufferDesc.ByteWidth = sizeof(ShadowMaterialCB);
    shadowMaterialBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    shadowMaterialBufferDesc.CPUAccessFlags = 0;
    result = m_pDevice->CreateBuffer(&shadowMaterialBufferDesc, nullptr, &m_pShadowMaterialBuffer);
    if (FAILED(result))
        return result;


    D3D11_BUFFER_DESC prefilterCBDesc = {};
    prefilterCBDesc.Usage = D3D11_USAGE_DEFAULT;
    prefilterCBDesc.ByteWidth = sizeof(SpecularPrefilterCB);
    prefilterCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    prefilterCBDesc.CPUAccessFlags = 0;
    result = m_pDevice->CreateBuffer(&prefilterCBDesc, nullptr, &m_pSpecularPrefilterCB);
    if (FAILED(result))
        return result;

    const wchar_t* albedoFiles[kSphereCount] =
    {
         L"textures/bark_brown_02_diff_4k.dds",
         L"textures/blue_metal_plate_diff_4k.dds",
         L"textures/fabric_leather_02_diff_4k.dds",
         L"textures/old_stone_wall_diff_4k.dds"
    };
    const wchar_t* normalFiles[kSphereCount] =
    {
         L"textures/KORA_DEREVA.dds",
         L"textures/sinushnaya_zhelezka.dds",
         L"textures/gladkaya_kozha.dds",
         L"textures/kameshki.dds"
    };
    for (int i = 0; i < kSphereCount; ++i)
    {
        result = DirectX::CreateDDSTextureFromFile(
            m_pDevice,
            albedoFiles[i],
            nullptr,
            &m_pTextureViews[i]
        );
        if (FAILED(result))
            return result;
        result = DirectX::CreateDDSTextureFromFile(
            m_pDevice,
            normalFiles[i],
            nullptr,
            &m_pNormalMapViews[i]
        );
        if (FAILED(result))
            return result;
    }

    result = CompileShader(L"HdrToCubemap.ps", nullptr, &m_pHdrToCubemapPS);
    if (FAILED(result))
        return result;

    result = CompileShader(L"IrradianceConvolution.ps", nullptr, &m_pIrradianceConvolutionPS);
    if (FAILED(result))
        return result;

    result = CompileShader(L"SpecularPrefilter.ps", nullptr, &m_pSpecularPrefilterPS);
    if (FAILED(result))
        return result;

    result = CompileShader(L"BRDFIntegration.ps", nullptr, &m_pBRDFIntegrationPS);
    if (FAILED(result))
        return result;

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    result = m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState);

    if (FAILED(result))
        return result;

    result = LoadEnvironmentMap(L"cubemaps/sunset_jhbcentral_4k.hdr");
    if (FAILED(result))
    {
        MessageBox(nullptr, L"HDR environment load failed", L"Error", MB_OK);
        return result;
    }
    else
    {
        OutputDebugString(L"HDR environment loaded successfully\n");
    }

    if (!m_pBRDFLUTSRV)
    {
        result = GenerateBRDFLUT(512, 512, &m_pBRDFLUTSRV);
        if (FAILED(result))
        {
            MessageBox(nullptr, L"BRDF LUT generation failed", L"Error", MB_OK);
            return result;
        }
    }

    D3D11_RASTERIZER_DESC gltfRsDesc = {};
    gltfRsDesc.FillMode = D3D11_FILL_SOLID;
    gltfRsDesc.CullMode = D3D11_CULL_NONE;
    gltfRsDesc.FrontCounterClockwise = FALSE;
    gltfRsDesc.DepthClipEnable = TRUE;
    result = m_pDevice->CreateRasterizerState(&gltfRsDesc, &m_pGltfRasterState);
    if (FAILED(result))
        return result;

    ScanCubeMapsFolder();
    std::string currentFilename = "sunset_jhbcentral_4k.hdr";
    m_currentEnvIndex = -1;
    for (int i = 0; i < m_environmentFileNames.size(); ++i)
    {
        if (m_environmentFileNames[i] == currentFilename)
        {
            m_currentEnvIndex = i;
            m_prevEnvIndex = m_currentEnvIndex;
            break;
        }
    }

    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_FRONT;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable = TRUE;
    result = m_pDevice->CreateRasterizerState(&rsDesc, &m_pSkyRasterState);
    if (FAILED(result))
        return result;

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    result = m_pDevice->CreateDepthStencilState(&dsDesc, &m_pSkyDepthState);
    if (FAILED(result))
        return result;

    return result;
}

HRESULT RenderClass::InitLuminanceResources(UINT width, UINT height)
{
    HRESULT result;

    for (int i = 0; i < 16; i++)
    {
        if (m_pLuminanceTextures[i])
        {
            m_pLuminanceTextures[i]->Release();
            m_pLuminanceTextures[i] = nullptr;
        }
        if (m_pLuminanceRTV[i])
        {
            m_pLuminanceRTV[i]->Release();
            m_pLuminanceRTV[i] = nullptr;
        }
        if (m_pLuminanceSRV[i])
        {
            m_pLuminanceSRV[i]->Release();
            m_pLuminanceSRV[i] = nullptr;
        }
        if (m_pLuminanceStagingTextures[i])
        {
            m_pLuminanceStagingTextures[i]->Release();
            m_pLuminanceStagingTextures[i] = nullptr;
        }
    }

    if (m_pLuminanceQuery)
    {
        m_pLuminanceQuery->Release();
        m_pLuminanceQuery = nullptr;
    }

    UINT minDim = std::min(width, height);
    m_LuminanceLevels = 0;

    UINT size = minDim;
    while (size >= 1)
    {
        m_LuminanceLevels++;
        size /= 2;
    }

    if (!m_pFullScreenVS)
    {
        result = CompileShader(L"FullScreenVS.vs", &m_pFullScreenVS, nullptr);
        if (FAILED(result))
            return result;
    }

    if (!m_pLuminancePS)
    {
        result = CompileShader(L"LuminancePixel.ps", nullptr, &m_pLuminancePS);
        if (FAILED(result))
            return result;
    }

    if (!m_pDownsamplePS)
    {
        result = CompileShader(L"LuminanceDownsample.ps", nullptr, &m_pDownsamplePS);
        if (FAILED(result))
            return result;
    }

    if (!m_pFullScreenLayout)
    {
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        ID3DBlob* pVSBlob = nullptr;
        result = CompileShader(L"FullScreenVS.vs", nullptr, nullptr, &pVSBlob);
        if (FAILED(result))
            return result;

        result = m_pDevice->CreateInputLayout(layout, 2, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pFullScreenLayout);
        pVSBlob->Release();
        if (FAILED(result))
            return result;
    }

    if (!m_pFullScreenQuadVB)
    {
        FullScreenVertex vertices[] =
        {
            { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
            { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
            { XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
            { XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
        };

        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(FullScreenVertex) * 4;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = vertices;

        result = m_pDevice->CreateBuffer(&bd, &initData, &m_pFullScreenQuadVB);
        if (FAILED(result))
            return result;
    }

    size = minDim;
    for (int i = 0; i < m_LuminanceLevels; i++)
    {
        UINT currentSize = (size < 1) ? 1 : size;

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = currentSize;
        texDesc.Height = currentSize;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        result = m_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pLuminanceTextures[i]);
        if (FAILED(result))
            return result;

        result = m_pDevice->CreateRenderTargetView(m_pLuminanceTextures[i], nullptr, &m_pLuminanceRTV[i]);
        if (FAILED(result))
            return result;

        result = m_pDevice->CreateShaderResourceView(m_pLuminanceTextures[i], nullptr, &m_pLuminanceSRV[i]);
        if (FAILED(result))
            return result;

        size /= 2;
    }

    size = minDim;
    for (int i = 0; i < m_LuminanceLevels; i++)
    {
        UINT currentSize = (size < 1) ? 1 : size;

        D3D11_TEXTURE2D_DESC stagingDesc = {};
        stagingDesc.Width = currentSize;
        stagingDesc.Height = currentSize;
        stagingDesc.MipLevels = 1;
        stagingDesc.ArraySize = 1;
        stagingDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        stagingDesc.SampleDesc.Count = 1;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        result = m_pDevice->CreateTexture2D(&stagingDesc, nullptr, &m_pLuminanceStagingTextures[i]);
        if (FAILED(result))
            return result;

        size /= 2;
    }

    if (!m_pLuminanceQuery)
    {
        D3D11_QUERY_DESC queryDesc = {};
        queryDesc.Query = D3D11_QUERY_EVENT;
        result = m_pDevice->CreateQuery(&queryDesc, &m_pLuminanceQuery);
    }

    return result;
}

void RenderClass::CalculateAverageLuminance()
{
    if (!m_pDeviceContext || !m_pHDRSceneSRV)
        return;

    ID3D11RenderTargetView* pOldRTV = nullptr;
    ID3D11DepthStencilView* pOldDSV = nullptr;
    m_pDeviceContext->OMGetRenderTargets(1, &pOldRTV, &pOldDSV);

    D3D11_VIEWPORT oldViewport;
    UINT numViewports = 1;
    m_pDeviceContext->RSGetViewports(&numViewports, &oldViewport);

    ID3D11VertexShader* pOldVS = nullptr;
    ID3D11PixelShader* pOldPS = nullptr;
    ID3D11InputLayout* pOldLayout = nullptr;
    m_pDeviceContext->VSGetShader(&pOldVS, nullptr, nullptr);
    m_pDeviceContext->PSGetShader(&pOldPS, nullptr, nullptr);
    m_pDeviceContext->IAGetInputLayout(&pOldLayout);

    UINT stride = sizeof(FullScreenVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pFullScreenQuadVB, &stride, &offset);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pDeviceContext->IASetInputLayout(m_pFullScreenLayout);

    m_pDeviceContext->VSSetShader(m_pFullScreenVS, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pLuminancePS, nullptr, 0);

    D3D11_VIEWPORT vp = {};
    D3D11_TEXTURE2D_DESC texDesc;
    m_pLuminanceTextures[0]->GetDesc(&texDesc);
    vp.Width = (FLOAT)texDesc.Width;
    vp.Height = (FLOAT)texDesc.Height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;

    m_pDeviceContext->OMSetRenderTargets(1, &m_pLuminanceRTV[0], nullptr);
    m_pDeviceContext->RSSetViewports(1, &vp);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pHDRSceneSRV);
    m_pDeviceContext->Draw(4, 0);

    m_pDeviceContext->PSSetShader(m_pDownsamplePS, nullptr, 0);

    for (int i = 1; i < m_LuminanceLevels; i++)
    {
        m_pLuminanceTextures[i]->GetDesc(&texDesc);
        vp.Width = (FLOAT)texDesc.Width;
        vp.Height = (FLOAT)texDesc.Height;
        m_pDeviceContext->RSSetViewports(1, &vp);

        m_pDeviceContext->OMSetRenderTargets(1, &m_pLuminanceRTV[i], nullptr);
        m_pDeviceContext->PSSetShaderResources(0, 1, &m_pLuminanceSRV[i - 1]);
        m_pDeviceContext->Draw(4, 0);
    }

    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

    m_pDeviceContext->OMSetRenderTargets(1, &pOldRTV, pOldDSV);
    m_pDeviceContext->RSSetViewports(1, &oldViewport);
    m_pDeviceContext->VSSetShader(pOldVS, nullptr, 0);
    m_pDeviceContext->PSSetShader(pOldPS, nullptr, 0);
    m_pDeviceContext->IASetInputLayout(pOldLayout);

    if (pOldVS)
        pOldVS->Release();
    if (pOldPS)
        pOldPS->Release();
    if (pOldLayout)
        pOldLayout->Release();
    if (pOldRTV)
        pOldRTV->Release();
    if (pOldDSV)
        pOldDSV->Release();

    if (m_pLuminanceQuery)
    {
        m_pDeviceContext->End(m_pLuminanceQuery);
    }
}

float RenderClass::ReadLuminanceFromGPU()
{
    while (m_pDeviceContext->GetData(m_pLuminanceQuery, nullptr, 0, 0) == S_FALSE)
    {
        Sleep(1);
    }

    int lastLevel = m_LuminanceLevels - 1;

    m_pDeviceContext->CopyResource(m_pLuminanceStagingTextures[lastLevel], m_pLuminanceTextures[lastLevel]);

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_pDeviceContext->Map(m_pLuminanceStagingTextures[lastLevel], 0, D3D11_MAP_READ, 0, &mapped);

    float luminance = 0.5f;
    if (SUCCEEDED(hr))
    {
        float logAvg = ((float*)mapped.pData)[0];
        luminance = expf(logAvg) - 1.0f;
        m_pDeviceContext->Unmap(m_pLuminanceStagingTextures[lastLevel], 0);
    }

    return luminance;
}

void RenderClass::Terminate()
{
    if (m_pDeviceContext)
    {
        m_pDeviceContext->ClearState();
        m_pDeviceContext->Flush();
    }
    if (ImGui::GetCurrentContext() != nullptr)
    {
        ShutdownImGui();
    }

    m_environmentFiles.clear();
    m_environmentFileNames.clear();

    ReleaseAllGltfModelResources();

    TerminateBufferShader();

    ReleaseGroundPlane();

    ReleaseShadowResources();

    if (m_pBRDFLUTSRV)
    {
        m_pBRDFLUTSRV->Release();
        m_pBRDFLUTSRV = nullptr;
    }
    if (m_pPrefilteredEnvSRV)
    {
        m_pPrefilteredEnvSRV->Release();
        m_pPrefilteredEnvSRV = nullptr;
    }

    for (int i = 0; i < 16; i++)
    {
        if (m_pLuminanceTextures[i])
        {
            m_pLuminanceTextures[i]->Release();
            m_pLuminanceTextures[i] = nullptr;
        }
        if (m_pLuminanceRTV[i])
        {
            m_pLuminanceRTV[i]->Release();
            m_pLuminanceRTV[i] = nullptr;
        }
        if (m_pLuminanceSRV[i])
        {
            m_pLuminanceSRV[i]->Release();
            m_pLuminanceSRV[i] = nullptr;
        }
        if (m_pLuminanceStagingTextures[i])
        {
            m_pLuminanceStagingTextures[i]->Release();
            m_pLuminanceStagingTextures[i] = nullptr;
        }
    }

    if (m_pLuminanceQuery)
    {
        if (m_pDeviceContext)
        {
            m_pDeviceContext->End(m_pLuminanceQuery);
            m_pDeviceContext->Flush();
        }
        m_pLuminanceQuery->Release();
        m_pLuminanceQuery = nullptr;
    }

    if (m_pFullScreenLayout)
    {
        m_pFullScreenLayout->Release();
        m_pFullScreenLayout = nullptr;
    }

    if (m_pHDRSceneSRV)
    {
        m_pHDRSceneSRV->Release();
        m_pHDRSceneSRV = nullptr;
    }

    if (m_pIrradianceSRV)
    {
        m_pIrradianceSRV->Release();
        m_pIrradianceSRV = nullptr;
    }

    if (m_pHDRSceneRTV)
    {
        m_pHDRSceneRTV->Release();
        m_pHDRSceneRTV = nullptr;
    }

    if (m_pHDRSceneTexture)
    {
        m_pHDRSceneTexture->Release();
        m_pHDRSceneTexture = nullptr;
    }

    ReleaseSSAOResources();
    ReleaseGBufferResources();

    if (m_pRenderTargetView)
    {
        m_pRenderTargetView->Release();
        m_pRenderTargetView = nullptr;
    }

    if (m_pDepthSRV)
    {
        m_pDepthSRV->Release();
        m_pDepthSRV = nullptr;
    }

    if (m_pDepthView)
    {
        m_pDepthView->Release();
        m_pDepthView = nullptr;
    }

    if (m_pDepthReadOnlyView)
    {
        m_pDepthReadOnlyView->Release();
        m_pDepthReadOnlyView = nullptr;
    }

    if (m_pDepthTexture)
    {
        m_pDepthTexture->Release();
        m_pDepthTexture = nullptr;
    }

    if (m_pSwapChain)
    {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }

    if (m_pDeviceContext)
    {
        m_pDeviceContext->Release();
        m_pDeviceContext = nullptr;
    }

    if (m_pAnnotation)
    {
        m_pAnnotation->Release();
        m_pAnnotation = nullptr;
    }

    if (m_pDevice)
    {
#ifdef _DEBUG
        ID3D11Debug* pDebug = nullptr;
        HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&pDebug);
        if (SUCCEEDED(hr) && pDebug)
        {
            pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
            pDebug->Release();
        }
#endif
        m_pDevice->Release();
        m_pDevice = nullptr;
    }
}

void RenderClass::TerminateBufferShader()
{
    if (m_pVertexShader)
    {
        m_pVertexShader->Release();
        m_pVertexShader = nullptr;
    }

    if (m_pPixelShader)
    {
        m_pPixelShader->Release();
        m_pPixelShader = nullptr;
    }

    if (m_pGBufferVS)
    {
        m_pGBufferVS->Release();
        m_pGBufferVS = nullptr;
    }

    if (m_pGBufferPS)
    {
        m_pGBufferPS->Release();
        m_pGBufferPS = nullptr;
    }

    if (m_pSSAOPS)
    {
        m_pSSAOPS->Release();
        m_pSSAOPS = nullptr;
    }

    if (m_pDeferredLightingPS)
    {
        m_pDeferredLightingPS->Release();
        m_pDeferredLightingPS = nullptr;
    }

    if (m_pSSAOCB)
    {
        m_pSSAOCB->Release();
        m_pSSAOCB = nullptr;
    }

    if (m_pDebugTexturePS)
    {
        m_pDebugTexturePS->Release();
        m_pDebugTexturePS = nullptr;
    }

    if (m_pDebugTextureCB)
    {
        m_pDebugTextureCB->Release();
        m_pDebugTextureCB = nullptr;
    }

    if (m_pDeferredFrameBuffer)
    {
        m_pDeferredFrameBuffer->Release();
        m_pDeferredFrameBuffer = nullptr;
    }

    if (m_pDeferredLightBuffer)
    {
        m_pDeferredLightBuffer->Release();
        m_pDeferredLightBuffer = nullptr;
    }

    if (m_pDeferredLightingDepthOffState)
    {
        m_pDeferredLightingDepthOffState->Release();
        m_pDeferredLightingDepthOffState = nullptr;
    }

    if (m_pPointLightDepthState)
    {
        m_pPointLightDepthState->Release();
        m_pPointLightDepthState = nullptr;
    }

    if (m_pPointLightRasterState)
    {
        m_pPointLightRasterState->Release();
        m_pPointLightRasterState = nullptr;
    }

    if (m_pAdditiveBlendState)
    {
        m_pAdditiveBlendState->Release();
        m_pAdditiveBlendState = nullptr;
    }

    if (m_pLightPixelShader)
    {
        m_pLightPixelShader->Release();
        m_pLightPixelShader = nullptr;
    }

    if (m_pFullScreenVS)
    {
        m_pFullScreenVS->Release();
        m_pFullScreenVS = nullptr;
    }

    if (m_pLuminancePS)
    {
        m_pLuminancePS->Release();
        m_pLuminancePS = nullptr;
    }

    if (m_pDownsamplePS)
    {
        m_pDownsamplePS->Release();
        m_pDownsamplePS = nullptr;
    }

    if (m_pToneMapPS)
    {
        m_pToneMapPS->Release();
        m_pToneMapPS = nullptr;
    }

    if (m_pIrradianceConvolutionPS)
    {
        m_pIrradianceConvolutionPS->Release();
        m_pIrradianceConvolutionPS = nullptr;
    }

    if (m_pLayout)
    {
        m_pLayout->Release();
        m_pLayout = nullptr;
    }

    if (m_pSkyLayout)
    {
        m_pSkyLayout->Release();
        m_pSkyLayout = nullptr;
    }

    if (m_pFullScreenLayout)
    {
        m_pFullScreenLayout->Release();
        m_pFullScreenLayout = nullptr;
    }

    if (m_pVertexBuffer)
    {
        m_pVertexBuffer->Release();
        m_pVertexBuffer = nullptr;
    }

    if (m_pIndexBuffer)
    {
        m_pIndexBuffer->Release();
        m_pIndexBuffer = nullptr;
    }

    if (m_pFullScreenQuadVB)
    {
        m_pFullScreenQuadVB->Release();
        m_pFullScreenQuadVB = nullptr;
    }

    if (m_pModelBuffer)
    {
        m_pModelBuffer->Release();
        m_pModelBuffer = nullptr;
    }

    if (m_pVPBuffer)
    {
        m_pVPBuffer->Release();
        m_pVPBuffer = nullptr;
    }

    if (m_pLightBuffer)
    {
        m_pLightBuffer->Release();
        m_pLightBuffer = nullptr;
    }

    if (m_pMaterialBuffer)
    {
        m_pMaterialBuffer->Release();
        m_pMaterialBuffer = nullptr;
    }

    if (m_pColorBuffer)
    {
        m_pColorBuffer->Release();
        m_pColorBuffer = nullptr;
    }

    if (m_pToneMapCB)
    {
        m_pToneMapCB->Release();
        m_pToneMapCB = nullptr;
    }

    if (m_pEnvironmentSRV)
    {
        m_pEnvironmentSRV->Release();
        m_pEnvironmentSRV = nullptr;
    }

    if (m_pSkyVertexShader)
    {
        m_pSkyVertexShader->Release();
        m_pSkyVertexShader = nullptr;
    }

    if (m_pSkyPixelShader)
    {
        m_pSkyPixelShader->Release();
        m_pSkyPixelShader = nullptr;
    }

    if (m_pSkyRasterState)
    {
        m_pSkyRasterState->Release();
        m_pSkyRasterState = nullptr;
    }

    if (m_pGltfRasterState)
    {
        m_pGltfRasterState->Release();
        m_pGltfRasterState = nullptr;
    }

    if (m_pSkyDepthState)
    {
        m_pSkyDepthState->Release();
        m_pSkyDepthState = nullptr;
    }

    for (int i = 0; i < kSphereCount; ++i)
    {
        if (m_pTextureViews[i])
        {
            m_pTextureViews[i]->Release();
            m_pTextureViews[i] = nullptr;
        }
        if (m_pNormalMapViews[i])
        {
            m_pNormalMapViews[i]->Release();
            m_pNormalMapViews[i] = nullptr;
        }
    }

    if (m_pSamplerState)
    {
        m_pSamplerState->Release();
        m_pSamplerState = nullptr;
    }

    if (m_pHdrToCubemapPS)
    {
        m_pHdrToCubemapPS->Release();
        m_pHdrToCubemapPS = nullptr;
    }

    if (m_pSpecularPrefilterPS)
    {
        m_pSpecularPrefilterPS->Release();
        m_pSpecularPrefilterPS = nullptr;
    }

    if (m_pSpecularPrefilterCB)
    {
        m_pSpecularPrefilterCB->Release();
        m_pSpecularPrefilterCB = nullptr;
    }

    if (m_pBRDFIntegrationPS)
    {
        m_pBRDFIntegrationPS->Release();
        m_pBRDFIntegrationPS = nullptr;
    }

    if (m_pBloomExtractPS)
    {
        m_pBloomExtractPS->Release();
        m_pBloomExtractPS = nullptr;
    }

    if (m_pBloomBlurPS)
    {
        m_pBloomBlurPS->Release();
        m_pBloomBlurPS = nullptr;
    }

    if (m_pBloomCB)
    {
        m_pBloomCB->Release();
        m_pBloomCB = nullptr;
    }

    ReleaseBloomResources();
}

std::wstring Extension(const std::wstring& path)
{
    size_t dotPos = path.find_last_of(L".");
    if (dotPos == std::wstring::npos || dotPos == 0)
    {
        return L"";
    }
    return path.substr(dotPos + 1);
}

HRESULT RenderClass::CompileShader(const std::wstring& path, ID3D11VertexShader** ppVertexShader, ID3D11PixelShader** ppPixelShader, ID3DBlob** pCodeShader)
{
    std::wstring extension = Extension(path);

    std::string platform = "";

    if (extension == L"vs")
    {
        platform = "vs_5_0";
    }
    else if (extension == L"ps")
    {
        platform = "ps_5_0";
    }

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif 

    ID3DBlob* pCode = nullptr;
    ID3DBlob* pErr = nullptr;

    HRESULT result = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", platform.c_str(), 0, 0, &pCode, &pErr);
    if (!SUCCEEDED(result) && pErr != nullptr)
    {
        OutputDebugStringA((const char*)pErr->GetBufferPointer());
    }
    if (pErr)
        pErr->Release();

    if (SUCCEEDED(result))
    {
        if (extension == L"vs" && ppVertexShader)
        {
            result = m_pDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, ppVertexShader);
            if (FAILED(result))
            {
                pCode->Release();
                return result;
            }
        }
        else if (extension == L"ps" && ppPixelShader)
        {
            result = m_pDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, ppPixelShader);
            if (FAILED(result))
            {
                pCode->Release();
                return result;
            }
        }
    }

    if (pCodeShader)
    {
        *pCodeShader = pCode;
    }
    else
    {
        pCode->Release();
    }
    return result;
}

HRESULT RenderClass::LoadHDRTexture2D(const wchar_t* path, ID3D11ShaderResourceView** outSRV)
{
    if (!outSRV)
        return E_INVALIDARG;
    *outSRV = nullptr;
    std::string narrowPath(path, path + wcslen(path));
    int width = 0;
    int height = 0;
    int channels = 0;
    float* data = stbi_loadf(narrowPath.c_str(), &width, &height, &channels, 3);
    if (!data)
        return E_FAIL;
    std::vector<float> rgba(width * height * 4);
    for (int i = 0; i < width * height; ++i)
    {
        rgba[i * 4 + 0] = data[i * 3 + 0];
        rgba[i * 4 + 1] = data[i * 3 + 1];
        rgba[i * 4 + 2] = data[i * 3 + 2];
        rgba[i * 4 + 3] = 1.0f;
    }
    stbi_image_free(data);
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = rgba.data();
    initData.SysMemPitch = width * sizeof(float) * 4;
    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = m_pDevice->CreateTexture2D(&desc, &initData, &tex);
    if (FAILED(hr))
        return hr;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = m_pDevice->CreateShaderResourceView(tex, &srvDesc, outSRV);
    tex->Release();
    return hr;
}

HRESULT RenderClass::ConvertHDRIToCubemap(
    ID3D11ShaderResourceView* equirectSRV,
    UINT cubeSize,
    ID3D11ShaderResourceView** outCubeSRV)
{
    if (!equirectSRV || !outCubeSRV)
        return E_INVALIDARG;

    UINT oldViewportCount = 1;
    D3D11_VIEWPORT oldViewport = {};
    m_pDeviceContext->RSGetViewports(&oldViewportCount, &oldViewport);

    ID3D11RenderTargetView* oldRTV = nullptr;
    ID3D11DepthStencilView* oldDSV = nullptr;
    m_pDeviceContext->OMGetRenderTargets(1, &oldRTV, &oldDSV);

    *outCubeSRV = nullptr;
    D3D11_TEXTURE2D_DESC cubeDesc = {};
    cubeDesc.Width = cubeSize;
    cubeDesc.Height = cubeSize;
    cubeDesc.MipLevels = 0;
    cubeDesc.ArraySize = 6;
    cubeDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    cubeDesc.SampleDesc.Count = 1;
    cubeDesc.Usage = D3D11_USAGE_DEFAULT;
    cubeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    cubeDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* cubeTex = nullptr;
    HRESULT hr = m_pDevice->CreateTexture2D(&cubeDesc, nullptr, &cubeTex);
    if (FAILED(hr))
        return hr;

    D3D11_TEXTURE2D_DESC verifyDesc = {};
    cubeTex->GetDesc(&verifyDesc);

    wchar_t dbg[256];
    swprintf_s(
        dbg,
        L"CubeTex: %u x %u, ArraySize=%u, Mips=%u, MiscFlags=%u\n",
        verifyDesc.Width,
        verifyDesc.Height,
        verifyDesc.ArraySize,
        verifyDesc.MipLevels,
        verifyDesc.MiscFlags
    );
    OutputDebugString(dbg);

    D3D11_SHADER_RESOURCE_VIEW_DESC cubeSRVDesc = {};
    cubeSRVDesc.Format = cubeDesc.Format;
    cubeSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    cubeSRVDesc.TextureCube.MostDetailedMip = 0;
    cubeSRVDesc.TextureCube.MipLevels = -1;

    ID3D11ShaderResourceView* cubeSRV = nullptr;
    hr = m_pDevice->CreateShaderResourceView(cubeTex, &cubeSRVDesc, &cubeSRV);
    if (FAILED(hr))
    {
        cubeTex->Release();
        return hr;
    }

    ID3D11RenderTargetView* faceRTV[6] = {};
    for (UINT i = 0; i < 6; ++i)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = cubeDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        rtvDesc.Texture2DArray.ArraySize = 1;

        hr = m_pDevice->CreateRenderTargetView(cubeTex, &rtvDesc, &faceRTV[i]);
        if (FAILED(hr))
        {
            for (UINT k = 0; k < i; ++k)
                if (faceRTV[k])
                    faceRTV[k]->Release();
            cubeSRV->Release();
            cubeTex->Release();
            return hr;
        }
    }

    ID3D11RasterizerState* pOldRS = nullptr;
    m_pDeviceContext->RSGetState(&pOldRS);

    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_FRONT;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable = TRUE;

    ID3D11RasterizerState* pCubeRS = nullptr;
    hr = m_pDevice->CreateRasterizerState(&rsDesc, &pCubeRS);
    if (FAILED(hr))
    {
        for (UINT i = 0; i < 6; ++i)
            faceRTV[i]->Release();
        cubeSRV->Release();
        cubeTex->Release();
        if (pOldRS)
            pOldRS->Release();
        return hr;
    }
    m_pDeviceContext->RSSetState(pCubeRS);

    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<float>(cubeSize);
    vp.Height = static_cast<float>(cubeSize);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &vp);

    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pSkyLayout);

    m_pDeviceContext->VSSetShader(m_pSkyVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pHdrToCubemapPS, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &equirectSRV);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);

    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);

    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 10.0f);
    XMVECTOR eye = XMVectorZero();
    const XMVECTOR targets[6] =
    {
        XMVectorSet(1, 0, 0, 0),
        XMVectorSet(-1, 0, 0, 0),
        XMVectorSet(0, 1, 0, 0),
        XMVectorSet(0, -1, 0, 0),
        XMVectorSet(0, 0, 1, 0),
        XMVectorSet(0, 0, -1, 0)
    };
    const XMVECTOR ups[6] =
    {
        XMVectorSet(0, 1, 0, 0),
        XMVectorSet(0, 1, 0, 0),
        XMVectorSet(0, 0, -1, 0),
        XMVectorSet(0, 0, 1, 0),
        XMVectorSet(0, 1, 0, 0),
        XMVectorSet(0, 1, 0, 0)
    };
    XMMATRIX model = XMMatrixIdentity();
    XMMATRIX modelT = XMMatrixTranspose(model);
    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &modelT, 0, 0);

    for (UINT face = 0; face < 6; ++face)
    {
        float clearColor[4] = { 0, 0, 0, 1 };
        m_pDeviceContext->OMSetRenderTargets(1, &faceRTV[face], nullptr);
        m_pDeviceContext->ClearRenderTargetView(faceRTV[face], clearColor);
        CameraBuffer cb = {};
        cb.vp = XMMatrixTranspose(XMMatrixLookToLH(eye, targets[face], ups[face]) * proj);
        cb.cameraPos = XMFLOAT3(0, 0, 0);
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        hr = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr))
            break;
        memcpy(mapped.pData, &cb, sizeof(cb));
        m_pDeviceContext->Unmap(m_pVPBuffer, 0);
        m_pDeviceContext->DrawIndexed(m_indexCount, 0, 0);
    }

    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

    if (SUCCEEDED(hr))
        m_pDeviceContext->GenerateMips(cubeSRV);

    m_pDeviceContext->RSSetState(pOldRS);
    if (pOldRS)
        pOldRS->Release();
    if (pCubeRS)
        pCubeRS->Release();

    for (UINT i = 0; i < 6; ++i)
        if (faceRTV[i])
            faceRTV[i]->Release();
    cubeTex->Release();

    if (FAILED(hr))
    {
        cubeSRV->Release();
        return hr;
    }
    *outCubeSRV = cubeSRV;

    m_pDeviceContext->OMSetRenderTargets(1, &oldRTV, oldDSV);
    m_pDeviceContext->RSSetViewports(1, &oldViewport);

    if (oldRTV)
        oldRTV->Release();
    if (oldDSV)
        oldDSV->Release();

    return S_OK;
}

HRESULT RenderClass::LoadEnvironmentMap(const wchar_t* path)
{
    if (m_pEnvironmentSRV)
    {
        m_pEnvironmentSRV->Release();
        m_pEnvironmentSRV = nullptr;
    }
    if (m_pIrradianceSRV)
    {
        m_pIrradianceSRV->Release();
        m_pIrradianceSRV = nullptr;
    }
    if (m_pPrefilteredEnvSRV)
    {
        m_pPrefilteredEnvSRV->Release();
        m_pPrefilteredEnvSRV = nullptr;
    }
    HRESULT hr = E_FAIL;
    if (HasExtension(path, L".dds"))
    {
        hr = DirectX::CreateDDSTextureFromFileEx(
            m_pDevice,
            path,
            0,
            D3D11_USAGE_DEFAULT,
            D3D11_BIND_SHADER_RESOURCE,
            0,
            0,
            DirectX::DDS_LOADER_FORCE_SRGB,
            nullptr,
            &m_pEnvironmentSRV
        );
    }
    else if (HasExtension(path, L".hdr"))
    {
        ID3D11ShaderResourceView* hdr2DSRV = nullptr;
        hr = LoadHDRTexture2D(path, &hdr2DSRV);
        if (FAILED(hr))
            return hr;
        hr = ConvertHDRIToCubemap(hdr2DSRV, 1024, &m_pEnvironmentSRV);
        hdr2DSRV->Release();
    }
    if (FAILED(hr))
        return hr;

    hr = ConvolveCubemapToIrradiance(
        m_pEnvironmentSRV,
        32,
        &m_pIrradianceSRV
    );
    if (FAILED(hr))
        return hr;

    hr = PrefilterCubemapSpecular(
        m_pEnvironmentSRV,
        256,
        5,
        &m_pPrefilteredEnvSRV
    );
    if (FAILED(hr))
        return hr;

    return S_OK;
}

void RenderClass::ScanCubeMapsFolder()
{
    m_environmentFiles.clear();
    m_environmentFileNames.clear();

    std::filesystem::path folder(L"cubemaps");
    if (!std::filesystem::exists(folder))
        return;

    for (const auto& entry : std::filesystem::directory_iterator(folder))
    {
        if (entry.is_regular_file())
        {
            auto ext = ToLowerCopy(entry.path().extension().wstring());
            if (ext == L".hdr")
            {
                m_environmentFiles.push_back(entry.path().wstring());
                m_environmentFileNames.push_back(entry.path().filename().string());
            }
        }
    }
}

void RenderClass::Render()
{
    struct ScopedEvent
    {
        ID3DUserDefinedAnnotation* ann = nullptr;
        bool active = false;

        ScopedEvent(ID3DUserDefinedAnnotation* a, const wchar_t* name) : ann(a)
        {
            if (ann && ann->GetStatus())
            {
                ann->BeginEvent(name);
                active = true;
            }
        }

        ~ScopedEvent()
        {
            if (active && ann)
                ann->EndEvent();
        }
    };

    ScopedEvent frameEvent(m_pAnnotation, L"Frame");


    ScopedEvent evt(m_pAnnotation, L"Clear");

    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);
    float aspect = static_cast<float>(rc.right - rc.left) / (rc.bottom - rc.top);

    XMMATRIX cameraView = BuildViewMatrix(
        m_CameraPosition,
        m_LRAngle,
        m_UDAngle
    );

    XMMATRIX cameraProj = BuildProjectionMatrix(
        aspect,
        m_CameraNearZ,
        m_CameraFarZ
    );

    XMMATRIX viewProj = cameraView * cameraProj;
    UpdateShadowLightDirectionFromAngles();
    UpdateCascadedShadowData(cameraView, cameraProj);

    ID3D11RenderTargetView* sceneRTV = (m_DebugViewMode == DebugView_Final) ? m_pHDRSceneRTV : m_pRenderTargetView;
    m_pDeviceContext->OMSetRenderTargets(1, &sceneRTV, m_pDepthView);


    float hdrClear[4] = { 0, 0, 0, 0 };
    if (m_DebugViewMode == DebugView_Final)
    {
        float BackColor[4] = { 0.48f, 0.57f, 0.48f, 1.0f };
        m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, BackColor);

        if (m_pHDRSceneRTV)
            m_pDeviceContext->ClearRenderTargetView(m_pHDRSceneRTV, hdrClear);
    }
    else
    {
        m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, hdrClear);
    }
    m_pDeviceContext->ClearDepthStencilView(m_pDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)(rc.right - rc.left);
    vp.Height = (FLOAT)(rc.bottom - rc.top);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &vp);
    RenderCascadedShadowPass();

    sceneRTV = (m_DebugViewMode == DebugView_Final) ? m_pHDRSceneRTV : m_pRenderTargetView;
    m_pDeviceContext->OMSetRenderTargets(1, &sceneRTV, m_pDepthView);
    vp.Width = (FLOAT)(rc.right - rc.left);
    vp.Height = (FLOAT)(rc.bottom - rc.top);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    m_pDeviceContext->RSSetViewports(1, &vp);

    UpdateCameraAndLightBuffers(cameraView, viewProj);

    RenderGBufferPass(viewProj);
    RenderSSAO(cameraView, cameraProj);


    if (m_DebugViewMode == DebugView_DeferredLighting)
    {
        ID3D11RenderTargetView* debugRTV = m_pRenderTargetView;
        m_pDeviceContext->OMSetRenderTargets(1, &debugRTV, m_pDepthView);
        float debugClear[4] = { 0, 0, 0, 1 };
        m_pDeviceContext->ClearRenderTargetView(debugRTV, debugClear);
        RenderDeferredLighting(viewProj, debugRTV);
        RenderImGui();
        m_pSwapChain->Present(1, 0);
        return;
    }

    if (IsFullScreenDebugView())
    {
        ID3D11RenderTargetView* debugRTV = m_pRenderTargetView;
        m_pDeviceContext->OMSetRenderTargets(1, &debugRTV, nullptr);
        float debugClear[4] = { 0, 0, 0, 1 };
        m_pDeviceContext->ClearRenderTargetView(debugRTV, debugClear);

        if (m_DebugViewMode == DebugView_SSAO)
            RenderDebugTexture(m_pSSAOSRV, 0);
        else if (m_DebugViewMode == DebugView_NormalBuffer)
            RenderDebugTexture(m_pNormalSRV, 1);
        else if (m_DebugViewMode == DebugView_DepthBuffer)
            RenderDebugTexture(m_pDepthSRV, 2);
        else if (m_DebugViewMode == DebugView_GBufferAlbedo)
            RenderDebugTexture(m_pGBufferAlbedoSRV, 3);
        else if (m_DebugViewMode == DebugView_GBufferMaterial)
            RenderDebugTexture(m_pGBufferMaterialSRV, 4);
        else if (m_DebugViewMode == DebugView_GBufferEmissive)
            RenderDebugTexture(m_pGBufferEmissiveSRV, 5);

        RenderImGui();
        m_pSwapChain->Present(1, 0);
        return;
    }


    sceneRTV = (m_DebugViewMode == DebugView_Final) ? m_pHDRSceneRTV : m_pRenderTargetView;
    m_pDeviceContext->OMSetRenderTargets(1, &sceneRTV, m_pDepthView);
    m_pDeviceContext->ClearDepthStencilView(m_pDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    RenderSkybox(viewProj);

    RenderGroundPlane(viewProj);
    RenderAllSceneModels(viewProj);

    //RenderLightSources(viewProj);

    if (m_DebugViewMode == DebugView_Final)
        ApplyBloom();

    if (m_DebugViewMode == DebugView_Final)
    {
        ScopedEvent evt(m_pAnnotation, L"Luminance Calculation");
        CalculateAverageLuminance();
        m_CurrentLuminance = ReadLuminanceFromGPU();

        static int frameCount = 0;
        frameCount++;
        if (frameCount % 20 == 0)
        {
            char buf[256];
            sprintf_s(buf, "CurrLum=%.4f AdaptLum=%.4f\n", m_CurrentLuminance, m_AdaptedLuminance);
            OutputDebugStringA(buf);
        }

        ULONGLONG now = GetTickCount64();
        float dt = (m_LastFrameTime == 0) ? (1.0f / 60.0f)
            : float(now - m_LastFrameTime) / 10.0f;
        m_LastFrameTime = now;

        if (dt > 0.1f) dt = 0.1f;

        float tauUp = m_EyeAdaptationTime;
        float tauDown = m_EyeAdaptationTime;
        float tau = (m_CurrentLuminance > m_AdaptedLuminance) ? tauUp : tauDown;
        float k = 1.0f - expf(-dt / tau);
        m_AdaptedLuminance += (m_CurrentLuminance - m_AdaptedLuminance) * k;
    }

    if (m_DebugViewMode == DebugView_Final)
    {
        ScopedEvent evt(m_pAnnotation, L"Apply tone mapping");
        ApplyToneMapping();
    }

    {
        ScopedEvent evt(m_pAnnotation, L"ImGui");
        RenderImGui();
    }

    {
        ScopedEvent evt(m_pAnnotation, L"Present");
        m_pSwapChain->Present(1, 0);
    }
}

void RenderClass::UpdateCameraAndLightBuffers(const XMMATRIX& view, const XMMATRIX& viewProj)
{
    CameraBuffer cameraBuffer = {};
    cameraBuffer.vp = XMMatrixTranspose(viewProj);
    cameraBuffer.view = XMMatrixTranspose(view);
    cameraBuffer.cameraPos = m_CameraPosition;
    cameraBuffer.padding = 0.0f;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_pDeviceContext->Map(
        m_pVPBuffer,
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mappedResource
    );
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResource.pData, &cameraBuffer, sizeof(CameraBuffer));
        m_pDeviceContext->Unmap(m_pVPBuffer, 0);
    }
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);

    PointLight lights[3];
    const float range = 150.0f;
    const float baseIntensity = 100.0f;

    lights[0].Position = m_LightPositions[0];
    lights[0].Range = range;
    lights[0].Color = m_LightColors[0];
    lights[0].Intensity = m_LightBrightness[0] * baseIntensity;
    //lights[1].Position = m_LightPositions[1];
    //lights[1].Range = range;
    //lights[1].Color = m_LightColors[1];
    //lights[1].Intensity = m_LightBrightness[1] * baseIntensity;
    //lights[2].Position = m_LightPositions[2];
    //lights[2].Range = range;
    //lights[2].Color = m_LightColors[2];
    //lights[2].Intensity = m_LightBrightness[2] * baseIntensity;

    lights[1].Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    lights[1].Range = 0.0f;
    lights[1].Color = XMFLOAT3(0.0f, 0.0f, 0.0f);
    lights[1].Intensity = 0.0f;
    lights[2].Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    lights[2].Range = 0.0f;
    lights[2].Color = XMFLOAT3(0.0f, 0.0f, 0.0f);
    lights[2].Intensity = 0.0f;

    D3D11_MAPPED_SUBRESOURCE mappedLight;
    hr = m_pDeviceContext->Map(m_pLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedLight);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedLight.pData, lights, sizeof(lights));
        m_pDeviceContext->Unmap(m_pLightBuffer, 0);
    }
    m_pDeviceContext->PSSetConstantBuffers(2, 1, &m_pLightBuffer);
}

void RenderClass::RenderSkybox(const XMMATRIX& viewProj)
{
    XMMATRIX skyModel =
        XMMatrixScaling(40.0f, 40.0f, 40.0f) *
        XMMatrixTranslation(m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z);

    XMMATRIX skyModelT = XMMatrixTranspose(skyModel);

    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &skyModelT, 0, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);
    m_pDeviceContext->RSSetState(m_pSkyRasterState);
    m_pDeviceContext->OMSetDepthStencilState(m_pSkyDepthState, 0);

    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pSkyLayout);
    m_pDeviceContext->VSSetShader(m_pSkyVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pSkyPixelShader, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pEnvironmentSRV);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    m_pDeviceContext->DrawIndexed(m_indexCount, 0, 0);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);
    m_pDeviceContext->RSSetState(nullptr);
    m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);
}

HRESULT RenderClass::ConfigureBackBuffer(UINT width, UINT height)
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr))
        return hr;

    hr = m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr))
        return hr;

    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_R32_TYPELESS;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    hr = m_pDevice->CreateTexture2D(&descDepth, nullptr, &m_pDepthTexture);
    if (FAILED(hr))
        return hr;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = m_pDevice->CreateDepthStencilView(m_pDepthTexture, &dsvDesc, &m_pDepthView);
    if (FAILED(hr))
        return hr;

    D3D11_DEPTH_STENCIL_VIEW_DESC readOnlyDsvDesc = dsvDesc;
    readOnlyDsvDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
    hr = m_pDevice->CreateDepthStencilView(m_pDepthTexture, &readOnlyDsvDesc, &m_pDepthReadOnlyView);
    if (FAILED(hr))
        return hr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = m_pDevice->CreateShaderResourceView(m_pDepthTexture, &srvDesc, &m_pDepthSRV);
    if (FAILED(hr))
        return hr;

    return hr;
}

HRESULT RenderClass::CreateGBufferResources(UINT width, UINT height)
{
    ReleaseGBufferResources();

    auto createTarget = [&](DXGI_FORMAT format,
        ID3D11Texture2D** texture,
        ID3D11RenderTargetView** rtv,
        ID3D11ShaderResourceView** srv) -> HRESULT
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = m_pDevice->CreateTexture2D(&desc, nullptr, texture);
        if (FAILED(hr))
            return hr;

        hr = m_pDevice->CreateRenderTargetView(*texture, nullptr, rtv);
        if (FAILED(hr))
            return hr;

        return m_pDevice->CreateShaderResourceView(*texture, nullptr, srv);
    };

    HRESULT hr = createTarget(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        &m_pGBufferAlbedoTexture,
        &m_pGBufferAlbedoRTV,
        &m_pGBufferAlbedoSRV
    );
    if (FAILED(hr))
        return hr;

    hr = createTarget(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        &m_pGBufferMaterialTexture,
        &m_pGBufferMaterialRTV,
        &m_pGBufferMaterialSRV
    );
    if (FAILED(hr))
        return hr;

    hr = createTarget(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        &m_pNormalTexture,
        &m_pNormalRTV,
        &m_pNormalSRV
    );
    if (FAILED(hr))
        return hr;

    hr = createTarget(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        &m_pGBufferEmissiveTexture,
        &m_pGBufferEmissiveRTV,
        &m_pGBufferEmissiveSRV
    );
    if (FAILED(hr))
        return hr;

    return S_OK;
}

void RenderClass::ReleaseGBufferResources()
{
    if (m_pGBufferEmissiveSRV)
    {
        m_pGBufferEmissiveSRV->Release();
        m_pGBufferEmissiveSRV = nullptr;
    }
    if (m_pGBufferEmissiveRTV)
    {
        m_pGBufferEmissiveRTV->Release();
        m_pGBufferEmissiveRTV = nullptr;
    }
    if (m_pGBufferEmissiveTexture)
    {
        m_pGBufferEmissiveTexture->Release();
        m_pGBufferEmissiveTexture = nullptr;
    }

    if (m_pNormalSRV)
    {
        m_pNormalSRV->Release();
        m_pNormalSRV = nullptr;
    }
    if (m_pNormalRTV)
    {
        m_pNormalRTV->Release();
        m_pNormalRTV = nullptr;
    }
    if (m_pNormalTexture)
    {
        m_pNormalTexture->Release();
        m_pNormalTexture = nullptr;
    }

    if (m_pGBufferMaterialSRV)
    {
        m_pGBufferMaterialSRV->Release();
        m_pGBufferMaterialSRV = nullptr;
    }
    if (m_pGBufferMaterialRTV)
    {
        m_pGBufferMaterialRTV->Release();
        m_pGBufferMaterialRTV = nullptr;
    }
    if (m_pGBufferMaterialTexture)
    {
        m_pGBufferMaterialTexture->Release();
        m_pGBufferMaterialTexture = nullptr;
    }

    if (m_pGBufferAlbedoSRV)
    {
        m_pGBufferAlbedoSRV->Release();
        m_pGBufferAlbedoSRV = nullptr;
    }
    if (m_pGBufferAlbedoRTV)
    {
        m_pGBufferAlbedoRTV->Release();
        m_pGBufferAlbedoRTV = nullptr;
    }
    if (m_pGBufferAlbedoTexture)
    {
        m_pGBufferAlbedoTexture->Release();
        m_pGBufferAlbedoTexture = nullptr;
    }
}

HRESULT RenderClass::CreateSSAOResources(UINT width, UINT height)
{
    ReleaseSSAOResources();

    D3D11_TEXTURE2D_DESC ssaoDesc = {};
    ssaoDesc.Width = width;
    ssaoDesc.Height = height;
    ssaoDesc.MipLevels = 1;
    ssaoDesc.ArraySize = 1;
    ssaoDesc.Format = DXGI_FORMAT_R8_UNORM;
    ssaoDesc.SampleDesc.Count = 1;
    ssaoDesc.SampleDesc.Quality = 0;
    ssaoDesc.Usage = D3D11_USAGE_DEFAULT;
    ssaoDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = m_pDevice->CreateTexture2D(&ssaoDesc, nullptr, &m_pSSAOTexture);
    if (FAILED(hr))
        return hr;

    hr = m_pDevice->CreateRenderTargetView(m_pSSAOTexture, nullptr, &m_pSSAORTV);
    if (FAILED(hr))
        return hr;

    hr = m_pDevice->CreateShaderResourceView(m_pSSAOTexture, nullptr, &m_pSSAOSRV);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

void RenderClass::ReleaseSSAOResources()
{
    if (m_pSSAOSRV)
    {
        m_pSSAOSRV->Release();
        m_pSSAOSRV = nullptr;
    }
    if (m_pSSAORTV)
    {
        m_pSSAORTV->Release();
        m_pSSAORTV = nullptr;
    }
    if (m_pSSAOTexture)
    {
        m_pSSAOTexture->Release();
        m_pSSAOTexture = nullptr;
    }
}

HRESULT RenderClass::CreateHDRSceneTexture(UINT width, UINT height)
{
    HRESULT hr;

    if (m_pHDRSceneSRV)
        m_pHDRSceneSRV->Release();
    if (m_pHDRSceneRTV)
        m_pHDRSceneRTV->Release();
    if (m_pHDRSceneTexture)
        m_pHDRSceneTexture->Release();

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = m_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pHDRSceneTexture);
    if (FAILED(hr))
        return hr;

    hr = m_pDevice->CreateRenderTargetView(m_pHDRSceneTexture, nullptr, &m_pHDRSceneRTV);
    if (FAILED(hr))
        return hr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = m_pDevice->CreateShaderResourceView(m_pHDRSceneTexture, &srvDesc, &m_pHDRSceneSRV);

    return hr;
}

// ground plane

HRESULT RenderClass::CreateGroundPlane(float halfSize, float uvScale)
{
    ReleaseGroundPlane();
    m_GroundHalfSize = halfSize;

    GroundVertex vertices[] =
    {
        { XMFLOAT3(-halfSize, 0.0f, -halfSize), XMFLOAT3(0, 1, 0), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(-halfSize, 0.0f,  halfSize), XMFLOAT3(0, 1, 0), XMFLOAT2(0.0f, uvScale) },
        { XMFLOAT3(halfSize, 0.0f,  halfSize), XMFLOAT3(0, 1, 0), XMFLOAT2(uvScale, uvScale) },
        { XMFLOAT3(halfSize, 0.0f, -halfSize), XMFLOAT3(0, 1, 0), XMFLOAT2(uvScale, 0.0f) },
    };

    uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };
    m_GroundIndexCount = _countof(indices);

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;

    HRESULT hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pGroundVB);
    if (FAILED(hr))
        return hr;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;

    hr = m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pGroundIB);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

void RenderClass::RenderGroundPlane(const XMMATRIX& viewProj)
{
    if (!m_pGroundVB || !m_pGroundIB || m_GroundIndexCount == 0)
        return;

    UINT stride = sizeof(GroundVertex);
    UINT offset = 0;

    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pLayout);
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    m_pDeviceContext->PSSetSamplers(2, 1, &m_pShadowSampler);

    XMMATRIX world = XMMatrixIdentity();
    XMMATRIX worldT = XMMatrixTranspose(world);

    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &worldT, 0, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);

    MaterialParamsCB materialParams = {};
    materialParams.Surface = XMFLOAT4(
        0.05f, // metallic
        0.82f, // roughness
        1.0f, // ao
        m_EnableGroundNormalMap ? m_GroundNormalStrength : 0.0f // normal strength
    );

    materialParams.Albedo = XMFLOAT4(0.34f, 0.34f, 0.33f, 1.0f);
    materialParams.DebugView = XMFLOAT4(
        (float)m_DebugViewMode,
        m_EnableSpecularIBL ? 1.0f : 0.0f,
        m_DiffuseIBLIntensity,
        m_SpecularIBLIntensity
    );
    materialParams.Emissive = XMFLOAT4(0, 0, 0, 0);

    materialParams.Extra = XMFLOAT4(
        m_ShowCascadeSplitColors ? 1.0f : 0.0f,
        1.0f,
        m_EnableSSAO ? 1.0f : 0.0f,
        (m_DebugViewMode == DebugView_GroundNormalMapMarkers) ? 1.0f : 0.0f
    );;

    m_pDeviceContext->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &materialParams, 0, 0);
    m_pDeviceContext->PSSetConstantBuffers(3, 1, &m_pMaterialBuffer);

    ID3D11ShaderResourceView* groundDiffuseSRV = m_pTextureViews[3];
    ID3D11ShaderResourceView* groundNormalSRV = m_pNormalMapViews[3];
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &groundDiffuseSRV);
    m_pDeviceContext->PSSetShaderResources(1, 1, &groundNormalSRV);
    m_pDeviceContext->PSSetShaderResources(2, 1, &m_pIrradianceSRV);
    m_pDeviceContext->PSSetShaderResources(3, 1, &m_pPrefilteredEnvSRV);
    m_pDeviceContext->PSSetShaderResources(4, 1, &m_pBRDFLUTSRV);
    m_pDeviceContext->PSSetShaderResources(5, 1, &nullSRV);
    m_pDeviceContext->PSSetShaderResources(6, 1, &m_pShadowMapSRV);
    m_pDeviceContext->PSSetShaderResources(7, 1, &m_pSSAOSRV);

    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pGroundVB, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pGroundIB, DXGI_FORMAT_R32_UINT, 0);

    m_pDeviceContext->VSSetConstantBuffers(4, 1, &m_pShadowLightBuffer);
    m_pDeviceContext->PSSetConstantBuffers(4, 1, &m_pShadowParamsBuffer);
    m_pDeviceContext->PSSetConstantBuffers(5, 1, &m_pShadowLightBuffer);

    m_pDeviceContext->PSSetSamplers(2, 1, &m_pShadowSampler);

    m_pDeviceContext->DrawIndexed(m_GroundIndexCount, 0, 0);
}


void RenderClass::GenerateSSAOKernel()
{
    std::mt19937 rng(1337u);
    std::uniform_real_distribution<float> distMinusOneToOne(-1.0f, 1.0f);
    std::uniform_real_distribution<float> distZeroToOne(0.0f, 1.0f);

    auto makeScaledSample = [&](float zMin, float zMax, UINT index) -> XMVECTOR
    {
        XMVECTOR sample = XMVectorSet(
            distMinusOneToOne(rng),
            distMinusOneToOne(rng),
            zMin + (zMax - zMin) * distZeroToOne(rng),
            0.0f
        );

        const float lenSq = XMVectorGetX(XMVector3LengthSq(sample));
        if (lenSq < 1e-6f)
        {
            sample = XMVectorSet(0.0f, 0.0f, zMax >= 0.0f ? 1.0f : -1.0f, 0.0f);
        }
        else
        {
            sample = XMVector3Normalize(sample);
        }

        sample = XMVectorScale(sample, distZeroToOne(rng));

        float scale = static_cast<float>(index) / static_cast<float>(SSAO_MAX_SAMPLE_COUNT);
        scale = 0.1f + 0.9f * scale * scale;
        return XMVectorScale(sample, scale);
    };

    for (UINT i = 0; i < SSAO_MAX_SAMPLE_COUNT; ++i)
    {
        XMStoreFloat4(&m_SSAOSphereSamples[i], makeScaledSample(-1.0f, 1.0f, i));
        XMStoreFloat4(&m_SSAOHemisphereSamples[i], makeScaledSample(0.0f, 1.0f, i));
    }

    for (UINT i = 0; i < SSAO_NOISE_VECTOR_COUNT; ++i)
    {
        XMVECTOR noise = XMVectorSet(
            distMinusOneToOne(rng),
            distMinusOneToOne(rng),
            0.0f,
            0.0f
        );

        const float lenSq = XMVectorGetX(XMVector3LengthSq(noise));
        if (lenSq < 1e-6f)
        {
            noise = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        }
        else
        {
            noise = XMVector3Normalize(noise);
        }

        XMStoreFloat4(&m_SSAONoise[i], noise);
    }
}

void RenderClass::RenderSSAO(const XMMATRIX& cameraView, const XMMATRIX& cameraProj)
{
    if (!m_pSSAORTV || !m_pDepthSRV || !m_pNormalSRV || !m_pSSAOPS || !m_pSSAOCB ||
        !m_pFullScreenVS || !m_pFullScreenQuadVB || !m_pFullScreenLayout)
    {
        return;
    }

    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);
    const UINT width = static_cast<UINT>(std::max<LONG>(1, rc.right - rc.left));
    const UINT height = static_cast<UINT>(std::max<LONG>(1, rc.bottom - rc.top));

    ID3D11ShaderResourceView* nullSRVs[8] = {};
    m_pDeviceContext->PSSetShaderResources(0, 8, nullSRVs);
    m_pDeviceContext->OMSetRenderTargets(1, &m_pSSAORTV, nullptr);

    float clearSSAO[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pSSAORTV, clearSSAO);
    if (!m_EnableSSAO)
    {
        m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        return;
    }


    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<FLOAT>(width);
    vp.Height = static_cast<FLOAT>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    m_pDeviceContext->RSSetViewports(1, &vp);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_pDeviceContext->Map(m_pSSAOCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        SSAOParamsCB* cb = reinterpret_cast<SSAOParamsCB*>(mapped.pData);
        cb->Proj = XMMatrixTranspose(cameraProj);
        cb->InvProj = XMMatrixTranspose(XMMatrixInverse(nullptr, cameraProj));
        cb->View = XMMatrixTranspose(cameraView);
        cb->Params0 = XMFLOAT4(
            m_SSAORadius,
            m_SSAOBias,
            m_SSAOStrength,
            m_SSAOMaxDepthDiff
        );
        cb->Params1 = XMFLOAT4(
            static_cast<float>(std::min<UINT>(m_SSAOSampleCount, SSAO_MAX_SAMPLE_COUNT)),
            static_cast<float>(width),
            static_cast<float>(height),
            static_cast<float>(std::clamp(m_SSAOMode, 0, 2))
        );

        const XMFLOAT4* selectedSamples = (m_SSAOMode == 0) ? m_SSAOSphereSamples : m_SSAOHemisphereSamples;
        for (UINT i = 0; i < SSAO_MAX_SAMPLE_COUNT; ++i)
        {
            cb->Samples[i] = selectedSamples[i];
        }

        for (UINT i = 0; i < SSAO_NOISE_VECTOR_COUNT; ++i)
        {
            cb->Noise[i] = m_SSAONoise[i];
        }

        m_pDeviceContext->Unmap(m_pSSAOCB, 0);
    }

    UINT stride = sizeof(FullScreenVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pFullScreenQuadVB, &stride, &offset);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pDeviceContext->IASetInputLayout(m_pFullScreenLayout);
    m_pDeviceContext->VSSetShader(m_pFullScreenVS, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pSSAOPS, nullptr, 0);

    ID3D11ShaderResourceView* srvs[2] = { m_pDepthSRV, m_pNormalSRV };
    m_pDeviceContext->PSSetShaderResources(0, 2, srvs);
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pSSAOCB);

    m_pDeviceContext->Draw(4, 0);

    ID3D11ShaderResourceView* nullPair[2] = {};
    m_pDeviceContext->PSSetShaderResources(0, 2, nullPair);
    ID3D11Buffer* nullCB = nullptr;
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &nullCB);
    m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}


bool RenderClass::IsFullScreenDebugView() const
{
    return m_DebugViewMode == DebugView_SSAO ||
        m_DebugViewMode == DebugView_NormalBuffer ||
        m_DebugViewMode == DebugView_DepthBuffer ||
        m_DebugViewMode == DebugView_GBufferAlbedo ||
        m_DebugViewMode == DebugView_GBufferMaterial ||
        m_DebugViewMode == DebugView_GBufferEmissive;
}

void RenderClass::RenderDebugTexture(ID3D11ShaderResourceView* textureSRV, int mode)
{
    if (!textureSRV || !m_pDebugTexturePS || !m_pDebugTextureCB ||
        !m_pFullScreenVS || !m_pFullScreenQuadVB || !m_pFullScreenLayout)
    {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_pDeviceContext->Map(m_pDebugTextureCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        DebugTextureParamsCB* cb = reinterpret_cast<DebugTextureParamsCB*>(mapped.pData);
        cb->Params = XMFLOAT4(
            static_cast<float>(mode),
            m_CameraNearZ,
            m_CameraFarZ,
            25.0f
        );
        m_pDeviceContext->Unmap(m_pDebugTextureCB, 0);
    }

    UINT stride = sizeof(FullScreenVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pFullScreenQuadVB, &stride, &offset);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pDeviceContext->IASetInputLayout(m_pFullScreenLayout);
    m_pDeviceContext->VSSetShader(m_pFullScreenVS, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pDebugTexturePS, nullptr, 0);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    m_pDeviceContext->PSSetShaderResources(0, 1, &textureSRV);
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pDebugTextureCB);

    m_pDeviceContext->Draw(4, 0);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    ID3D11Buffer* nullCB = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &nullCB);
}
void RenderClass::RenderGBufferPass(const XMMATRIX& viewProj)
{
    if (!m_pGBufferAlbedoRTV || !m_pGBufferMaterialRTV || !m_pNormalRTV ||
        !m_pGBufferEmissiveRTV || !m_pDepthView || !m_pGBufferVS || !m_pGBufferPS)
    {
        return;
    }

    ID3D11ShaderResourceView* nullSRVs[8] = {};
    m_pDeviceContext->PSSetShaderResources(0, 8, nullSRVs);

    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);

    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)(rc.right - rc.left);
    vp.Height = (FLOAT)(rc.bottom - rc.top);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    m_pDeviceContext->RSSetViewports(1, &vp);

    ID3D11RenderTargetView* gbufferRTVs[4] =
    {
        m_pGBufferAlbedoRTV,
        m_pGBufferMaterialRTV,
        m_pNormalRTV,
        m_pGBufferEmissiveRTV
    };

    float clearAlbedo[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float clearMaterial[4] = { 0.04f, 0.0f, 1.0f, 1.0f };
    float clearNormal[4] = { 0.5f, 0.5f, 1.0f, 1.0f };
    float clearEmissive[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    m_pDeviceContext->OMSetRenderTargets(4, gbufferRTVs, m_pDepthView);
    m_pDeviceContext->ClearRenderTargetView(m_pGBufferAlbedoRTV, clearAlbedo);
    m_pDeviceContext->ClearRenderTargetView(m_pGBufferMaterialRTV, clearMaterial);
    m_pDeviceContext->ClearRenderTargetView(m_pNormalRTV, clearNormal);
    m_pDeviceContext->ClearRenderTargetView(m_pGBufferEmissiveRTV, clearEmissive);
    m_pDeviceContext->ClearDepthStencilView(m_pDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pLayout);
    m_pDeviceContext->VSSetShader(m_pGBufferVS, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pGBufferPS, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);
    ID3D11Buffer* nullCBs[4] = {};
    m_pDeviceContext->VSSetConstantBuffers(2, 4, nullCBs);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    m_pDeviceContext->RSSetState(m_pGltfRasterState);

    RenderGroundPlaneGBuffer();
    RenderAllSceneModelsGBuffer();

    ID3D11ShaderResourceView* nullAfter[8] = {};
    m_pDeviceContext->PSSetShaderResources(0, 8, nullAfter);
    m_pDeviceContext->RSSetState(nullptr);
    m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void RenderClass::RenderGroundPlaneGBuffer()
{
    if (!m_pGroundVB || !m_pGroundIB || m_GroundIndexCount == 0)
        return;

    UINT stride = sizeof(GroundVertex);
    UINT offset = 0;

    XMMATRIX world = XMMatrixIdentity();
    XMMATRIX worldT = XMMatrixTranspose(world);

    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &worldT, 0, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);

    MaterialParamsCB materialParams = {};
    materialParams.Surface = XMFLOAT4(
        0.05f,
        0.82f,
        1.0f,
        m_EnableGroundNormalMap ? m_GroundNormalStrength : 0.0f
    );
    materialParams.Albedo = XMFLOAT4(0.34f, 0.34f, 0.33f, 1.0f);
    materialParams.DebugView = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    materialParams.Emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    materialParams.Extra = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    materialParams.AlphaParams = XMFLOAT4(0.0f, 1.0f, 0.5f, 0.0f);
    materialParams.TextureFlags = XMFLOAT4(
        m_pTextureViews[3] ? 1.0f : 0.0f,
        (m_EnableGroundNormalMap && m_pNormalMapViews[3]) ? 1.0f : 0.0f,
        0.0f,
        0.0f
    );

    m_pDeviceContext->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &materialParams, 0, 0);
    m_pDeviceContext->PSSetConstantBuffers(3, 1, &m_pMaterialBuffer);

    ID3D11ShaderResourceView* groundDiffuseSRV = m_pTextureViews[3];
    ID3D11ShaderResourceView* groundNormalSRV = m_pNormalMapViews[3];
    ID3D11ShaderResourceView* nullSRVs[2] = {};
    m_pDeviceContext->PSSetShaderResources(0, 1, &groundDiffuseSRV);
    m_pDeviceContext->PSSetShaderResources(1, 1, &groundNormalSRV);
    m_pDeviceContext->PSSetShaderResources(2, 2, nullSRVs);

    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pGroundVB, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pGroundIB, DXGI_FORMAT_R32_UINT, 0);
    m_pDeviceContext->DrawIndexed(m_GroundIndexCount, 0, 0);
}

void RenderClass::RenderAllSceneModelsGBuffer()
{
    for (const SceneModelInstance& instance : m_SceneModelInstances)
    {
        RenderModelInstanceGBuffer(instance);
    }

    ID3D11ShaderResourceView* nullSRVs[4] = {};
    m_pDeviceContext->PSSetShaderResources(0, 4, nullSRVs);
}

void RenderClass::RenderModelInstanceGBuffer(const SceneModelInstance& instance)
{
    if (instance.ModelResourceIndex < 0 ||
        instance.ModelResourceIndex >= (int)m_ModelResources.size())
    {
        return;
    }

    const GltfModelResource& model = m_ModelResources[instance.ModelResourceIndex];

    for (int rootNode : model.Scene.RootNodes)
    {
        RenderGltfNodeGBuffer(
            model,
            rootNode,
            instance.PrecomputedWorld
        );
    }
}

void RenderClass::RenderGltfNodeGBuffer(
    const GltfModelResource& model,
    int nodeIndex,
    const XMMATRIX& instanceWorld)
{
    if (nodeIndex < 0 || nodeIndex >= (int)model.Scene.Nodes.size())
        return;

    const GltfNodeData& node = model.Scene.Nodes[nodeIndex];
    XMMATRIX nodeWorld = XMLoadFloat4x4(&node.WorldMatrix);
    XMMATRIX finalWorld = nodeWorld * instanceWorld;

    if (node.MeshIndex >= 0 && node.MeshIndex < (int)model.GpuMeshes.size())
    {
        const GltfGpuMesh& mesh = model.GpuMeshes[node.MeshIndex];
        for (const auto& prim : mesh.Primitives)
        {
            DrawGltfPrimitiveGBuffer(model, prim, finalWorld);
        }
    }

    for (int child : node.Children)
    {
        RenderGltfNodeGBuffer(model, child, finalWorld);
    }
}

void RenderClass::DrawGltfPrimitiveGBuffer(
    const GltfModelResource& model,
    const GltfGpuPrimitive& primitive,
    const XMMATRIX& world)
{
    if (!primitive.VertexBuffer || !primitive.IndexBuffer || primitive.IndexCount == 0)
        return;

    UINT stride = sizeof(GltfVertex);
    UINT offset = 0;

    XMMATRIX worldT = XMMatrixTranspose(world);
    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &worldT, 0, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);

    MaterialParamsCB materialParams = {};
    materialParams.Surface = XMFLOAT4(
        m_MaterialMetalness,
        m_MaterialRoughness,
        m_MaterialAO,
        m_NormalStrength
    );
    materialParams.Albedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    materialParams.DebugView = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    materialParams.Emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    materialParams.Extra = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    materialParams.AlphaParams = XMFLOAT4(0.0f, 1.0f, 0.5f, 0.0f);
    materialParams.TextureFlags = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

    ID3D11ShaderResourceView* albedoSRV = nullptr;
    ID3D11ShaderResourceView* normalSRV = nullptr;
    ID3D11ShaderResourceView* metallicRoughnessSRV = nullptr;
    ID3D11ShaderResourceView* emissiveSRV = nullptr;

    if (primitive.MaterialIndex >= 0 &&
        primitive.MaterialIndex < (int)model.Scene.Materials.size())
    {
        const GltfMaterial& mat = model.Scene.Materials[primitive.MaterialIndex];

        materialParams.Surface.x = mat.MetallicFactor;
        materialParams.Surface.y = mat.RoughnessFactor;
        materialParams.Surface.z = 1.0f;
        materialParams.Surface.w = 1.0f;
        materialParams.Albedo = XMFLOAT4(
            mat.BaseColorFactor.x,
            mat.BaseColorFactor.y,
            mat.BaseColorFactor.z,
            mat.BaseColorFactor.w
        );
        materialParams.Emissive = XMFLOAT4(
            mat.EmissiveFactor.x,
            mat.EmissiveFactor.y,
            mat.EmissiveFactor.z,
            1.0f
        );
        materialParams.AlphaParams = XMFLOAT4(
            0.0f,
            mat.BaseColorFactor.w,
            mat.AlphaCutoff,
            mat.AlphaMask ? 1.0f : 0.0f
        );

        if (mat.BaseColorTexture >= 0 &&
            mat.BaseColorTexture < (int)model.TextureSRVs.size())
        {
            albedoSRV = model.TextureSRVs[mat.BaseColorTexture];
            materialParams.TextureFlags.x = albedoSRV ? 1.0f : 0.0f;
            materialParams.AlphaParams.x = materialParams.TextureFlags.x;
        }

        if (mat.NormalTexture >= 0 &&
            mat.NormalTexture < (int)model.TextureSRVs.size())
        {
            normalSRV = model.TextureSRVs[mat.NormalTexture];
            materialParams.TextureFlags.y = normalSRV ? 1.0f : 0.0f;
        }

        if (mat.MetallicRoughnessTexture >= 0 &&
            mat.MetallicRoughnessTexture < (int)model.TextureSRVs.size())
        {
            metallicRoughnessSRV = model.TextureSRVs[mat.MetallicRoughnessTexture];
            materialParams.TextureFlags.z = metallicRoughnessSRV ? 1.0f : 0.0f;
        }

        if (mat.EmissiveTexture >= 0 &&
            mat.EmissiveTexture < (int)model.TextureSRVs.size())
        {
            emissiveSRV = model.TextureSRVs[mat.EmissiveTexture];
            materialParams.TextureFlags.w = emissiveSRV ? 1.0f : 0.0f;
        }
    }

    m_pDeviceContext->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &materialParams, 0, 0);
    m_pDeviceContext->PSSetConstantBuffers(3, 1, &m_pMaterialBuffer);

    ID3D11ShaderResourceView* srvs[4] =
    {
        albedoSRV,
        normalSRV,
        metallicRoughnessSRV,
        emissiveSRV
    };

    m_pDeviceContext->IASetVertexBuffers(0, 1, &primitive.VertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(primitive.IndexBuffer, primitive.IndexFormat, 0);
    m_pDeviceContext->PSSetShaderResources(0, 4, srvs);
    m_pDeviceContext->DrawIndexed(primitive.IndexCount, 0, 0);
}

void RenderClass::ReleaseGroundPlane()
{
    if (m_pGroundVB)
    {
        m_pGroundVB->Release();
        m_pGroundVB = nullptr;
    }

    if (m_pGroundIB)
    {
        m_pGroundIB->Release();
        m_pGroundIB = nullptr;
    }

    m_GroundIndexCount = 0;
}

void RenderClass::BuildSceneLayout()
{
    m_SceneModelInstances.clear();

    const std::vector<SceneModelDesc> descs = GetSceneModelDescs();

    const size_t count = std::min(descs.size(), m_ModelResources.size());

    for (size_t i = 0; i < count; ++i)
    {
        SceneModelInstance inst = {};
        inst.ModelResourceIndex = (int)i;
        inst.Position = descs[i].Position;
        inst.RotationDeg = descs[i].RotationDeg;
        inst.Scale = descs[i].Scale;
        inst.CastShadow = descs[i].CastShadow;
        inst.ReceiveShadow = descs[i].ReceiveShadow;

        m_SceneModelInstances.push_back(inst);
    }
}

void RenderClass::PrecomputeSceneModelTransforms()
{
    for (SceneModelInstance& inst : m_SceneModelInstances)
    {
        const float rx = XMConvertToRadians(inst.RotationDeg.x);
        const float ry = XMConvertToRadians(inst.RotationDeg.y);
        const float rz = XMConvertToRadians(inst.RotationDeg.z);

        XMMATRIX scaleM =
            XMMatrixScaling(inst.Scale.x, inst.Scale.y, inst.Scale.z);

        XMMATRIX rotM =
            XMMatrixRotationRollPitchYaw(rx, ry, rz);

        XMMATRIX transM =
            XMMatrixTranslation(inst.Position.x, inst.Position.y, inst.Position.z);

        inst.PrecomputedWorld = scaleM * rotM * transM;
    }
}


void RenderClass::BuildDeferredPointLights()
{
    m_DeferredPointLights.clear();
    m_DeferredPointLights.reserve(DEFERRED_MAX_POINT_LIGHTS);

    const XMFLOAT3 colors[] =
    {
        XMFLOAT3(1.0f, 0.24f, 0.18f),
        XMFLOAT3(0.18f, 0.45f, 1.0f),
        XMFLOAT3(0.18f, 1.0f, 0.48f),
        XMFLOAT3(1.0f, 0.85f, 0.25f),
        XMFLOAT3(0.75f, 0.32f, 1.0f),
        XMFLOAT3(0.25f, 1.0f, 0.95f)
    };

    const int columns = 16;
    const int rows = 8;
    const float spacingX = 8.0f;
    const float spacingZ = 8.0f;

    for (UINT i = 0; i < DEFERRED_MAX_POINT_LIGHTS; ++i)
    {
        const int xIndex = static_cast<int>(i % columns);
        const int zIndex = static_cast<int>((i / columns) % rows);
        const float x = (static_cast<float>(xIndex) - (columns - 1) * 0.5f) * spacingX;
        const float z = (static_cast<float>(zIndex) - (rows - 1) * 0.5f) * spacingZ;
        const float y = 2.5f + 1.6f * sinf(static_cast<float>(i) * 0.73f);

        DeferredPointLightData light = {};
        light.Position = XMFLOAT3(x, y, z);
        light.Range = m_DeferredLightRadius;
        light.Color = colors[i % _countof(colors)];
        light.Intensity = 300.0f + 80.0f * static_cast<float>(i % 5);
        m_DeferredPointLights.push_back(light);
    }
}

void RenderClass::RenderDeferredLighting(const XMMATRIX& viewProj, ID3D11RenderTargetView* targetRTV)
{
    if (!targetRTV || !m_pDeferredLightingPS || !m_pDeferredFrameBuffer || !m_pDeferredLightBuffer ||
        !m_pGBufferAlbedoSRV || !m_pGBufferMaterialSRV || !m_pNormalSRV || !m_pGBufferEmissiveSRV ||
        !m_pDepthSRV || !m_pFullScreenVS || !m_pFullScreenQuadVB || !m_pFullScreenLayout)
    {
        return;
    }

    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);
    const float width = static_cast<float>(std::max<LONG>(1, rc.right - rc.left));
    const float height = static_cast<float>(std::max<LONG>(1, rc.bottom - rc.top));
    const int activeLightCount = std::clamp(m_DeferredPointLightCount, 0, static_cast<int>(std::min<size_t>(m_DeferredPointLights.size(), DEFERRED_MAX_POINT_LIGHTS)));

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_pDeviceContext->Map(m_pDeferredFrameBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        DeferredLightingFrameCB* cb = reinterpret_cast<DeferredLightingFrameCB*>(mapped.pData);
        cb->InvViewProj = XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj));
        cb->CameraPositionLightCount = XMFLOAT4(
            m_CameraPosition.x,
            m_CameraPosition.y,
            m_CameraPosition.z,
            static_cast<float>(activeLightCount)
        );
        cb->ScreenSizeParams = XMFLOAT4(width, height, 1.0f / width, 1.0f / height);
        cb->DirectionalLight = XMFLOAT4(
            m_ShadowLightDirection.x,
            m_ShadowLightDirection.y,
            m_ShadowLightDirection.z,
            m_LightBrightness[0] * 1.8f
        );
        cb->IBLParams = XMFLOAT4(
            m_EnableSpecularIBL ? 1.0f : 0.0f,
            m_DiffuseIBLIntensity,
            m_SpecularIBLIntensity,
            m_EnableSSAO ? 1.0f : 0.0f
        );
        m_pDeviceContext->Unmap(m_pDeferredFrameBuffer, 0);
    }

    ID3D11ShaderResourceView* nullSRVs[12] = {};
    m_pDeviceContext->PSSetShaderResources(0, 12, nullSRVs);
    ID3D11ShaderResourceView* srvs[10] =
    {
        m_pGBufferAlbedoSRV,
        m_pGBufferMaterialSRV,
        m_pNormalSRV,
        m_pGBufferEmissiveSRV,
        m_pDepthSRV,
        m_pSSAOSRV,
        m_pShadowMapSRV,
        m_pIrradianceSRV,
        m_pPrefilteredEnvSRV,
        m_pBRDFLUTSRV
    };

    m_pDeviceContext->PSSetShaderResources(0, 10, srvs);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    m_pDeviceContext->PSSetSamplers(2, 1, &m_pShadowSampler);
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pDeferredFrameBuffer);
    m_pDeviceContext->PSSetConstantBuffers(1, 1, &m_pVPBuffer);
    m_pDeviceContext->PSSetConstantBuffers(4, 1, &m_pShadowParamsBuffer);
    m_pDeviceContext->PSSetConstantBuffers(5, 1, &m_pShadowLightBuffer);
    m_pDeviceContext->OMSetRenderTargets(1, &targetRTV, m_pDepthReadOnlyView ? m_pDepthReadOnlyView : m_pDepthView);

    D3D11_VIEWPORT vp = {};
    vp.Width = width;
    vp.Height = height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    m_pDeviceContext->RSSetViewports(1, &vp);

    RenderDeferredDirectionalLighting();
    RenderDeferredPointLighting(viewProj);

    m_pDeviceContext->PSSetShaderResources(0, 12, nullSRVs);
    ID3D11Buffer* nullCBs[7] = {};
    m_pDeviceContext->PSSetConstantBuffers(0, 7, nullCBs);
    float blendFactor[4] = { 0, 0, 0, 0 };
    m_pDeviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);
    m_pDeviceContext->RSSetState(nullptr);
}

void RenderClass::RenderDeferredDirectionalLighting()
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_pDeviceContext->Map(m_pDeferredLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        DeferredLightingLightCB* cb = reinterpret_cast<DeferredLightingLightCB*>(mapped.pData);
        cb->PositionRange = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        cb->ColorIntensity = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        cb->Params = XMFLOAT4(0.0f, m_ShowCascadeSplitColors ? 1.0f : 0.0f, 0.0f, 0.0f);
        m_pDeviceContext->Unmap(m_pDeferredLightBuffer, 0);
    }

    UINT stride = sizeof(FullScreenVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pFullScreenQuadVB, &stride, &offset);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pDeviceContext->IASetInputLayout(m_pFullScreenLayout);
    m_pDeviceContext->VSSetShader(m_pFullScreenVS, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pDeferredLightingPS, nullptr, 0);
    m_pDeviceContext->PSSetConstantBuffers(6, 1, &m_pDeferredLightBuffer);
    m_pDeviceContext->OMSetDepthStencilState(m_pDeferredLightingDepthOffState, 0);
    float blendFactor[4] = { 0, 0, 0, 0 };
    m_pDeviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    m_pDeviceContext->Draw(4, 0);
}

void RenderClass::RenderDeferredPointLighting(const XMMATRIX& viewProj)
{
    if (!m_pVertexBuffer || !m_pIndexBuffer || m_indexCount == 0)
        return;

    const int activeLightCount = std::clamp(m_DeferredPointLightCount, 0, static_cast<int>(std::min<size_t>(m_DeferredPointLights.size(), DEFERRED_MAX_POINT_LIGHTS)));
    if (activeLightCount <= 0)
        return;

    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pLayout);
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->VSSetShader(m_pGBufferVS, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pDeferredLightingPS, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);
    m_pDeviceContext->PSSetConstantBuffers(6, 1, &m_pDeferredLightBuffer);
    m_pDeviceContext->RSSetState(m_pPointLightRasterState);
    m_pDeviceContext->OMSetDepthStencilState(m_pPointLightDepthState, 0);
    float blendFactor[4] = { 0, 0, 0, 0 };
    m_pDeviceContext->OMSetBlendState(m_pAdditiveBlendState, blendFactor, 0xffffffff);

    const float range = std::max(0.1f, m_DeferredLightRadius);

    for (int i = 0; i < activeLightCount; ++i)
    {
        const DeferredPointLightData& light = m_DeferredPointLights[i];
        const float intensity = light.Intensity * std::max(0.0f, m_DeferredLightIntensityScale);
        if (intensity <= 0.0001f)
            continue;

        XMMATRIX world =
            XMMatrixScaling(range, range, range) *
            XMMatrixTranslation(light.Position.x, light.Position.y, light.Position.z);
        XMMATRIX worldT = XMMatrixTranspose(world);
        m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &worldT, 0, 0);
        m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(m_pDeviceContext->Map(m_pDeferredLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            DeferredLightingLightCB* cb = reinterpret_cast<DeferredLightingLightCB*>(mapped.pData);
            cb->PositionRange = XMFLOAT4(light.Position.x, light.Position.y, light.Position.z, range);
            cb->ColorIntensity = XMFLOAT4(light.Color.x, light.Color.y, light.Color.z, intensity);
            cb->Params = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
            m_pDeviceContext->Unmap(m_pDeferredLightBuffer, 0);
        }

        m_pDeviceContext->DrawIndexed(m_indexCount, 0, 0);
    }

    m_pDeviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);
    m_pDeviceContext->RSSetState(nullptr);
}

// light

void RenderClass::RenderLightSources(const XMMATRIX& viewProj)
{
    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;

    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pLayout);
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pLightPixelShader, nullptr, 0);

    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);
    m_pDeviceContext->PSSetShaderResources(1, 1, &nullSRV);
    m_pDeviceContext->PSSetShaderResources(5, 1, &nullSRV);

    const float lightSphereRadius = 0.25f;

    for (int i = 0; i < 3; ++i)
    {
        if (m_LightBrightness[i] <= 0.001f)
            continue;

        XMMATRIX world =
            XMMatrixScaling(lightSphereRadius, lightSphereRadius, lightSphereRadius) *
            XMMatrixTranslation(
                m_LightPositions[i].x,
                m_LightPositions[i].y,
                m_LightPositions[i].z
            );

        XMMATRIX worldT = XMMatrixTranspose(world);
        m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &worldT, 0, 0);
        m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);

        ColorBuffer colorData = {};

        float visualIntensity = 2.0f * m_LightBrightness[i];

        colorData.color = XMFLOAT4(
            m_LightColors[i].x * visualIntensity,
            m_LightColors[i].y * visualIntensity,
            m_LightColors[i].z * visualIntensity,
            1.0f
        );


        m_pDeviceContext->UpdateSubresource(m_pColorBuffer, 0, nullptr, &colorData, 0, 0);
        m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pColorBuffer);

        m_pDeviceContext->DrawIndexed(m_indexCount, 0, 0);
    }
}

void RenderClass::SetLightBrightness(int index, float value)
{
    if (index < 0 || index >= 3) return;
    if (value < 0.0f) value = 0.0f;
    if (value > 3.0) value = 3.0;
    m_LightBrightness[index] = value;
}

float RenderClass::GetLightBrightness(int index) const
{
    if (index < 0 || index >= 3) return 0.0f;
    return m_LightBrightness[index];
}

void RenderClass::ApplyToneMapping()
{
    if (!m_pHDRSceneSRV || !m_pToneMapPS || !m_pToneMapCB) return;
    m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);
    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);
    D3D11_VIEWPORT vp = {};
    vp.Width = float(rc.right - rc.left);
    vp.Height = float(rc.bottom - rc.top);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &vp);
    UINT stride = sizeof(FullScreenVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pFullScreenQuadVB, &stride, &offset);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pDeviceContext->IASetInputLayout(m_pFullScreenLayout);
    m_pDeviceContext->VSSetShader(m_pFullScreenVS, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pToneMapPS, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pHDRSceneSRV);
    ID3D11ShaderResourceView* bloomSRV = m_EnableBloom ? m_pBloomSRVA : nullptr;
    m_pDeviceContext->PSSetShaderResources(1, 1, &bloomSRV);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_pDeviceContext->Map(m_pToneMapCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        ToneMapParamsCB* cb = (ToneMapParamsCB*)mapped.pData;
        cb->Params = XMFLOAT4(
            m_AdaptedLuminance,
            m_BloomIntensity,
            m_EnableBloom ? 1.0f : 0.0f,
            m_EnableFXAA ? m_FXAAStrength : 0.0f
        );
        m_pDeviceContext->Unmap(m_pToneMapCB, 0);
    }
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pToneMapCB);
    m_pDeviceContext->Draw(4, 0);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);
    m_pDeviceContext->PSSetShaderResources(1, 1, &nullSRV);
}

HRESULT RenderClass::ConvolveCubemapToIrradiance(
    ID3D11ShaderResourceView* environmentCubeSRV,
    UINT irradianceSize,
    ID3D11ShaderResourceView** outIrradianceSRV)
{
    if (!environmentCubeSRV || !outIrradianceSRV)
        return E_INVALIDARG;

    *outIrradianceSRV = nullptr;

    UINT oldViewportCount = 1;
    D3D11_VIEWPORT oldViewport = {};
    m_pDeviceContext->RSGetViewports(&oldViewportCount, &oldViewport);

    ID3D11RenderTargetView* oldRTV = nullptr;
    ID3D11DepthStencilView* oldDSV = nullptr;
    m_pDeviceContext->OMGetRenderTargets(1, &oldRTV, &oldDSV);

    ID3D11RasterizerState* pOldRS = nullptr;
    m_pDeviceContext->RSGetState(&pOldRS);

    D3D11_TEXTURE2D_DESC cubeDesc = {};
    cubeDesc.Width = irradianceSize;
    cubeDesc.Height = irradianceSize;
    cubeDesc.MipLevels = 1;
    cubeDesc.ArraySize = 6;
    cubeDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    cubeDesc.SampleDesc.Count = 1;
    cubeDesc.Usage = D3D11_USAGE_DEFAULT;
    cubeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    cubeDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    ID3D11Texture2D* irradianceTex = nullptr;
    HRESULT hr = m_pDevice->CreateTexture2D(&cubeDesc, nullptr, &irradianceTex);
    if (FAILED(hr))
        return hr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = cubeDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = 1;

    ID3D11ShaderResourceView* irradianceSRV = nullptr;
    hr = m_pDevice->CreateShaderResourceView(irradianceTex, &srvDesc, &irradianceSRV);
    if (FAILED(hr))
    {
        irradianceTex->Release();
        return hr;
    }

    ID3D11RenderTargetView* faceRTV[6] = {};
    for (UINT i = 0; i < 6; ++i)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = cubeDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        rtvDesc.Texture2DArray.ArraySize = 1;

        hr = m_pDevice->CreateRenderTargetView(irradianceTex, &rtvDesc, &faceRTV[i]);
        if (FAILED(hr))
        {
            for (UINT k = 0; k < i; ++k)
                if (faceRTV[k]) faceRTV[k]->Release();
            irradianceSRV->Release();
            irradianceTex->Release();
            return hr;
        }
    }

    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_FRONT;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable = TRUE;

    ID3D11RasterizerState* pCubeRS = nullptr;
    hr = m_pDevice->CreateRasterizerState(&rsDesc, &pCubeRS);
    if (FAILED(hr))
    {
        for (UINT i = 0; i < 6; ++i)
            if (faceRTV[i]) faceRTV[i]->Release();
        irradianceSRV->Release();
        irradianceTex->Release();
        if (pOldRS) pOldRS->Release();
        return hr;
    }

    m_pDeviceContext->RSSetState(pCubeRS);

    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<float>(irradianceSize);
    vp.Height = static_cast<float>(irradianceSize);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &vp);

    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pSkyLayout);

    m_pDeviceContext->VSSetShader(m_pSkyVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pIrradianceConvolutionPS, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &environmentCubeSRV);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);

    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 10.0f);
    XMVECTOR eye = XMVectorZero();

    const XMVECTOR targets[6] =
    {
        XMVectorSet(1,  0,  0, 0),
        XMVectorSet(-1,  0,  0, 0),
        XMVectorSet(0,  1,  0, 0),
        XMVectorSet(0, -1,  0, 0),
        XMVectorSet(0,  0,  1, 0),
        XMVectorSet(0,  0, -1, 0)
    };

    const XMVECTOR ups[6] =
    {
        XMVectorSet(0, 1, 0, 0),
        XMVectorSet(0, 1, 0, 0),
        XMVectorSet(0, 0,-1, 0),
        XMVectorSet(0, 0, 1, 0),
        XMVectorSet(0, 1, 0, 0),
        XMVectorSet(0, 1, 0, 0)
    };

    XMMATRIX model = XMMatrixIdentity();
    XMMATRIX modelT = XMMatrixTranspose(model);
    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &modelT, 0, 0);

    for (UINT face = 0; face < 6; ++face)
    {
        float clearColor[4] = { 0, 0, 0, 1 };
        m_pDeviceContext->OMSetRenderTargets(1, &faceRTV[face], nullptr);
        m_pDeviceContext->ClearRenderTargetView(faceRTV[face], clearColor);

        CameraBuffer cb = {};
        cb.vp = XMMatrixTranspose(XMMatrixLookToLH(eye, targets[face], ups[face]) * proj);
        cb.cameraPos = XMFLOAT3(0, 0, 0);

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        hr = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr))
            break;

        memcpy(mapped.pData, &cb, sizeof(cb));
        m_pDeviceContext->Unmap(m_pVPBuffer, 0);

        m_pDeviceContext->DrawIndexed(m_indexCount, 0, 0);
    }

    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

    m_pDeviceContext->RSSetState(pOldRS);
    if (pOldRS) pOldRS->Release();
    if (pCubeRS) pCubeRS->Release();

    m_pDeviceContext->OMSetRenderTargets(1, &oldRTV, oldDSV);
    m_pDeviceContext->RSSetViewports(1, &oldViewport);

    if (oldRTV) oldRTV->Release();
    if (oldDSV) oldDSV->Release();

    for (UINT i = 0; i < 6; ++i)
        if (faceRTV[i]) faceRTV[i]->Release();

    irradianceTex->Release();

    if (FAILED(hr))
    {
        irradianceSRV->Release();
        return hr;
    }

    *outIrradianceSRV = irradianceSRV;
    return S_OK;
}

HRESULT RenderClass::GenerateBRDFLUT(
    UINT lutWidth,
    UINT lutHeight,
    ID3D11ShaderResourceView** outBRDFLUTSRV)
{
    if (!outBRDFLUTSRV)
        return E_INVALIDARG;
    *outBRDFLUTSRV = nullptr;
    ID3D11RenderTargetView* oldRTV = nullptr;
    ID3D11DepthStencilView* oldDSV = nullptr;
    m_pDeviceContext->OMGetRenderTargets(1, &oldRTV, &oldDSV);
    UINT oldViewportCount = 1;
    D3D11_VIEWPORT oldViewport = {};
    m_pDeviceContext->RSGetViewports(&oldViewportCount, &oldViewport);
    ID3D11InputLayout* oldLayout = nullptr;
    ID3D11VertexShader* oldVS = nullptr;
    ID3D11PixelShader* oldPS = nullptr;
    m_pDeviceContext->IAGetInputLayout(&oldLayout);
    m_pDeviceContext->VSGetShader(&oldVS, nullptr, nullptr);
    m_pDeviceContext->PSGetShader(&oldPS, nullptr, nullptr);
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = lutWidth;
    texDesc.Height = lutHeight;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D* lutTex = nullptr;
    HRESULT hr = m_pDevice->CreateTexture2D(&texDesc, nullptr, &lutTex);
    if (FAILED(hr))
        return hr;
    ID3D11RenderTargetView* lutRTV = nullptr;
    hr = m_pDevice->CreateRenderTargetView(lutTex, nullptr, &lutRTV);
    if (FAILED(hr))
    {
        lutTex->Release();
        return hr;
    }
    ID3D11ShaderResourceView* lutSRV = nullptr;
    hr = m_pDevice->CreateShaderResourceView(lutTex, nullptr, &lutSRV);
    if (FAILED(hr))
    {
        lutRTV->Release();
        lutTex->Release();
        return hr;
    }
    float clearColor[4] = { 0, 0, 0, 0 };
    m_pDeviceContext->OMSetRenderTargets(1, &lutRTV, nullptr);
    m_pDeviceContext->ClearRenderTargetView(lutRTV, clearColor);
    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)lutWidth;
    vp.Height = (FLOAT)lutHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_pDeviceContext->RSSetViewports(1, &vp);
    UINT stride = sizeof(FullScreenVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pFullScreenQuadVB, &stride, &offset);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pDeviceContext->IASetInputLayout(m_pFullScreenLayout);
    m_pDeviceContext->VSSetShader(m_pFullScreenVS, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pBRDFIntegrationPS, nullptr, 0);
    m_pDeviceContext->Draw(4, 0);
    m_pDeviceContext->OMSetRenderTargets(1, &oldRTV, oldDSV);
    m_pDeviceContext->RSSetViewports(1, &oldViewport);
    m_pDeviceContext->IASetInputLayout(oldLayout);
    m_pDeviceContext->VSSetShader(oldVS, nullptr, 0);
    m_pDeviceContext->PSSetShader(oldPS, nullptr, 0);
    if (oldLayout) oldLayout->Release();
    if (oldVS) oldVS->Release();
    if (oldPS) oldPS->Release();
    if (oldRTV) oldRTV->Release();
    if (oldDSV) oldDSV->Release();
    lutRTV->Release();
    lutTex->Release();
    *outBRDFLUTSRV = lutSRV;
    return S_OK;
}

HRESULT RenderClass::PrefilterCubemapSpecular(
    ID3D11ShaderResourceView* environmentCubeSRV,
    UINT prefilterSize,
    UINT mipLevels,
    ID3D11ShaderResourceView** outPrefilterSRV)
{
    if (!environmentCubeSRV || !outPrefilterSRV)
        return E_INVALIDARG;

    *outPrefilterSRV = nullptr;

    UINT oldViewportCount = 1;
    D3D11_VIEWPORT oldViewport = {};
    m_pDeviceContext->RSGetViewports(&oldViewportCount, &oldViewport);

    ID3D11RenderTargetView* oldRTV = nullptr;
    ID3D11DepthStencilView* oldDSV = nullptr;
    m_pDeviceContext->OMGetRenderTargets(1, &oldRTV, &oldDSV);

    ID3D11RasterizerState* pOldRS = nullptr;
    m_pDeviceContext->RSGetState(&pOldRS);
    D3D11_TEXTURE2D_DESC cubeDesc = {};
    cubeDesc.Width = prefilterSize;
    cubeDesc.Height = prefilterSize;
    cubeDesc.MipLevels = mipLevels;
    cubeDesc.ArraySize = 6;
    cubeDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    cubeDesc.SampleDesc.Count = 1;
    cubeDesc.Usage = D3D11_USAGE_DEFAULT;
    cubeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    cubeDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    ID3D11Texture2D* prefilterTex = nullptr;
    HRESULT hr = m_pDevice->CreateTexture2D(&cubeDesc, nullptr, &prefilterTex);
    if (FAILED(hr))
        return hr;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = cubeDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = mipLevels;

    ID3D11ShaderResourceView* prefilterSRV = nullptr;
    hr = m_pDevice->CreateShaderResourceView(prefilterTex, &srvDesc, &prefilterSRV);
    if (FAILED(hr))
    {
        prefilterTex->Release();
        return hr;
    }
    std::vector<ID3D11RenderTargetView*> rtvs;
    rtvs.resize(mipLevels * 6, nullptr);

    for (UINT mip = 0; mip < mipLevels; ++mip)
    {
        for (UINT face = 0; face < 6; ++face)
        {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = cubeDesc.Format;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = mip;
            rtvDesc.Texture2DArray.FirstArraySlice = face;
            rtvDesc.Texture2DArray.ArraySize = 1;

            hr = m_pDevice->CreateRenderTargetView(
                prefilterTex,
                &rtvDesc,
                &rtvs[mip * 6 + face]
            );

            if (FAILED(hr))
            {
                for (auto* rtv : rtvs)
                    if (rtv) rtv->Release();
                prefilterSRV->Release();
                prefilterTex->Release();
                if (pOldRS) pOldRS->Release();
                return hr;
            }
        }
    }
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_FRONT;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable = TRUE;

    ID3D11RasterizerState* pCubeRS = nullptr;
    hr = m_pDevice->CreateRasterizerState(&rsDesc, &pCubeRS);
    if (FAILED(hr))
    {
        for (auto* rtv : rtvs)
            if (rtv) rtv->Release();
        prefilterSRV->Release();
        prefilterTex->Release();
        if (pOldRS) pOldRS->Release();
        return hr;
    }

    m_pDeviceContext->RSSetState(pCubeRS);
    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pSkyLayout);

    m_pDeviceContext->VSSetShader(m_pSkyVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pSpecularPrefilterPS, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &environmentCubeSRV);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);

    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);
    m_pDeviceContext->PSSetConstantBuffers(4, 1, &m_pSpecularPrefilterCB);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 10.0f);
    XMVECTOR eye = XMVectorZero();

    const XMVECTOR targets[6] =
    {
        XMVectorSet(1,  0,  0, 0),
        XMVectorSet(-1, 0,  0, 0),
        XMVectorSet(0,  1,  0, 0),
        XMVectorSet(0, -1,  0, 0),
        XMVectorSet(0,  0,  1, 0),
        XMVectorSet(0,  0, -1, 0)
    };

    const XMVECTOR ups[6] =
    {
        XMVectorSet(0, 1, 0, 0),
        XMVectorSet(0, 1, 0, 0),
        XMVectorSet(0, 0,-1, 0),
        XMVectorSet(0, 0, 1, 0),
        XMVectorSet(0, 1, 0, 0),
        XMVectorSet(0, 1, 0, 0)
    };

    XMMATRIX model = XMMatrixIdentity();
    XMMATRIX modelT = XMMatrixTranspose(model);
    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &modelT, 0, 0);
    for (UINT mip = 0; mip < mipLevels; ++mip)
    {
        UINT mipWidth = std::max(1u, prefilterSize >> mip);
        UINT mipHeight = std::max(1u, prefilterSize >> mip);

        D3D11_VIEWPORT vp = {};
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width = static_cast<float>(mipWidth);
        vp.Height = static_cast<float>(mipHeight);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        m_pDeviceContext->RSSetViewports(1, &vp);

        float roughness = (mipLevels > 1)
            ? (float)mip / (float)(mipLevels - 1)
            : 0.0f;

        SpecularPrefilterCB prefilterCB = {};
        prefilterCB.Params = XMFLOAT4(roughness, (float)mipLevels, 0.0f, 0.0f);
        m_pDeviceContext->UpdateSubresource(
            m_pSpecularPrefilterCB,
            0,
            nullptr,
            &prefilterCB,
            0,
            0
        );

        for (UINT face = 0; face < 6; ++face)
        {
            float clearColor[4] = { 0, 0, 0, 1 };
            ID3D11RenderTargetView* rtv = rtvs[mip * 6 + face];

            m_pDeviceContext->OMSetRenderTargets(1, &rtv, nullptr);
            m_pDeviceContext->ClearRenderTargetView(rtv, clearColor);

            CameraBuffer cb = {};
            cb.vp = XMMatrixTranspose(XMMatrixLookToLH(eye, targets[face], ups[face]) * proj);
            cb.cameraPos = XMFLOAT3(0, 0, 0);

            D3D11_MAPPED_SUBRESOURCE mapped = {};
            hr = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            if (FAILED(hr))
                break;

            memcpy(mapped.pData, &cb, sizeof(cb));
            m_pDeviceContext->Unmap(m_pVPBuffer, 0);

            m_pDeviceContext->DrawIndexed(m_indexCount, 0, 0);
        }

        if (FAILED(hr))
            break;
    }
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

    m_pDeviceContext->RSSetState(pOldRS);
    if (pOldRS) pOldRS->Release();
    if (pCubeRS) pCubeRS->Release();

    m_pDeviceContext->OMSetRenderTargets(1, &oldRTV, oldDSV);
    m_pDeviceContext->RSSetViewports(1, &oldViewport);

    if (oldRTV) oldRTV->Release();
    if (oldDSV) oldDSV->Release();

    for (auto* rtv : rtvs)
        if (rtv) rtv->Release();

    prefilterTex->Release();

    if (FAILED(hr))
    {
        prefilterSRV->Release();
        return hr;
    }

    *outPrefilterSRV = prefilterSRV;
    return S_OK;
}

// gltf

bool RenderClass::LoadModelResource(const std::wstring& path, int& outResourceIndex)
{
    outResourceIndex = -1;

    GltfModelResource resource = {};
    resource.FilePath = path;

    if (!LoadGltfScene(path, resource.Scene))
        return false;

    HRESULT hr = CreateGltfGpuResources(resource);
    if (FAILED(hr))
    {
        ReleaseGltfGpuResources(resource);
        return false;
    }

    m_ModelResources.push_back(std::move(resource));
    outResourceIndex = (int)m_ModelResources.size() - 1;
    return true;
}

void RenderClass::LoadSceneModels()
{
    m_ModelResources.clear();
    m_SceneModelInstances.clear();

    const std::vector<SceneModelDesc> descs = GetSceneModelDescs();

    for (const auto& desc : descs)
    {
        int resourceIndex = -1;
        if (!LoadModelResource(desc.FilePath, resourceIndex))
        {
            std::wstring msg = L"Failed to load model: " + desc.FilePath + L"\n";
            OutputDebugStringW(msg.c_str());
        }
    }

    BuildSceneLayout();
    PrecomputeSceneModelTransforms();
    CollectShadowCasters();
}

void RenderClass::ReleaseGltfGpuResources(GltfModelResource& model)
{
    for (auto& mesh : model.GpuMeshes)
    {
        for (auto& prim : mesh.Primitives)
        {
            if (prim.VertexBuffer)
            {
                prim.VertexBuffer->Release();
                prim.VertexBuffer = nullptr;
            }

            if (prim.IndexBuffer)
            {
                prim.IndexBuffer->Release();
                prim.IndexBuffer = nullptr;
            }
        }
    }

    model.GpuMeshes.clear();

    for (auto* srv : model.TextureSRVs)
    {
        if (srv)
            srv->Release();
    }

    model.TextureSRVs.clear();
}

void RenderClass::ReleaseAllGltfModelResources()
{
    for (auto& model : m_ModelResources)
    {
        ReleaseGltfGpuResources(model);
    }

    m_ModelResources.clear();
    m_SceneModelInstances.clear();
}

HRESULT RenderClass::CreateTextureSRVFromFile(const std::wstring& path, ID3D11ShaderResourceView** outSRV)
{
    if (!outSRV)
        return E_INVALIDARG;
    *outSRV = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string narrow(path.begin(), path.end());
    unsigned char* pixels = stbi_load(narrow.c_str(), &width, &height, &channels, 4);
    if (!pixels)
        return E_FAIL;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = (UINT)width;
    desc.Height = (UINT)height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels;
    initData.SysMemPitch = width * 4;
    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = m_pDevice->CreateTexture2D(&desc, &initData, &texture);
    stbi_image_free(pixels);

    if (FAILED(hr))
        return hr;

    hr = m_pDevice->CreateShaderResourceView(texture, nullptr, outSRV);
    texture->Release();
    return hr;
}

HRESULT RenderClass::CreateGltfGpuResources(GltfModelResource& model)
{
    ReleaseGltfGpuResources(model);

    HRESULT hr = S_OK;

    model.GpuMeshes.resize(model.Scene.Meshes.size());

    for (size_t meshIndex = 0; meshIndex < model.Scene.Meshes.size(); ++meshIndex)
    {
        const GltfMeshData& srcMesh = model.Scene.Meshes[meshIndex];
        GltfGpuMesh& dstMesh = model.GpuMeshes[meshIndex];
        dstMesh.Primitives.resize(srcMesh.Primitives.size());

        for (size_t primIndex = 0; primIndex < srcMesh.Primitives.size(); ++primIndex)
        {
            const GltfPrimitiveData& srcPrim = srcMesh.Primitives[primIndex];
            GltfGpuPrimitive& dstPrim = dstMesh.Primitives[primIndex];

            dstPrim.MaterialIndex = srcPrim.MaterialIndex;
            dstPrim.IndexCount = (UINT)srcPrim.Indices.size();
            dstPrim.IndexFormat = DXGI_FORMAT_R32_UINT;

            if (srcPrim.Vertices.empty() || srcPrim.Indices.empty())
                continue;

            D3D11_BUFFER_DESC vbDesc = {};
            vbDesc.Usage = D3D11_USAGE_DEFAULT;
            vbDesc.ByteWidth = (UINT)(sizeof(GltfVertex) * srcPrim.Vertices.size());
            vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            D3D11_SUBRESOURCE_DATA vbData = {};
            vbData.pSysMem = srcPrim.Vertices.data();

            hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, &dstPrim.VertexBuffer);
            if (FAILED(hr))
                return hr;

            D3D11_BUFFER_DESC ibDesc = {};
            ibDesc.Usage = D3D11_USAGE_DEFAULT;
            ibDesc.ByteWidth = (UINT)(sizeof(uint32_t) * srcPrim.Indices.size());
            ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

            D3D11_SUBRESOURCE_DATA ibData = {};
            ibData.pSysMem = srcPrim.Indices.data();

            hr = m_pDevice->CreateBuffer(&ibDesc, &ibData, &dstPrim.IndexBuffer);
            if (FAILED(hr))
                return hr;
        }
    }

    model.TextureSRVs.resize(model.Scene.Textures.size(), nullptr);

    for (size_t i = 0; i < model.Scene.Textures.size(); ++i)
    {
        const auto& tex = model.Scene.Textures[i];
        if (!tex.Uri.empty())
        {
            CreateTextureSRVFromFile(tex.Uri, &model.TextureSRVs[i]);
        }
    }

    return S_OK;
}

void RenderClass::DrawGltfPrimitive(
    const GltfModelResource& model,
    const GltfGpuPrimitive& primitive,
    const XMMATRIX& world,
    const XMMATRIX& viewProj,
    bool receiveShadow)
{
    if (!primitive.VertexBuffer || !primitive.IndexBuffer || primitive.IndexCount == 0)
        return;

    XMMATRIX worldT = XMMatrixTranspose(world);
    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &worldT, 0, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);

    MaterialParamsCB materialParams = {};
    materialParams.Surface = XMFLOAT4(
        m_MaterialMetalness,
        m_MaterialRoughness,
        m_MaterialAO,
        m_NormalStrength
    );
    materialParams.Albedo = XMFLOAT4(1, 1, 1, 0);
    materialParams.DebugView = XMFLOAT4(
        (float)m_DebugViewMode,
        m_EnableSpecularIBL ? 1.0f : 0.0f,
        m_DiffuseIBLIntensity,
        m_SpecularIBLIntensity
    );

    materialParams.Extra = XMFLOAT4(
        0.0f,
        receiveShadow ? 1.0f : 0.0f,
        m_EnableSSAO ? 1.0f : 0.0f,
        0.0f
    );

    ID3D11ShaderResourceView* albedoSRV = nullptr;
    ID3D11ShaderResourceView* normalSRV = nullptr;
    ID3D11ShaderResourceView* emissiveSRV = nullptr;

    if (primitive.MaterialIndex >= 0 &&
        primitive.MaterialIndex < (int)model.Scene.Materials.size())
    {
        const GltfMaterial& mat = model.Scene.Materials[primitive.MaterialIndex];

        materialParams.Emissive = XMFLOAT4(0, 0, 0, 0);
        materialParams.Surface.x = mat.MetallicFactor;
        materialParams.Surface.y = mat.RoughnessFactor;
        materialParams.Surface.z = 1.0f;
        materialParams.Surface.w = 1.0f;

        materialParams.Albedo = XMFLOAT4(
            mat.BaseColorFactor.x,
            mat.BaseColorFactor.y,
            mat.BaseColorFactor.z,
            (mat.BaseColorTexture >= 0) ? 1.0f : 0.0f
        );

        materialParams.Emissive = XMFLOAT4(
            mat.EmissiveFactor.x,
            mat.EmissiveFactor.y,
            mat.EmissiveFactor.z,
            (mat.EmissiveTexture >= 0) ? 1.0f : 0.0f
        );

        if (mat.BaseColorTexture >= 0 &&
            mat.BaseColorTexture < (int)model.TextureSRVs.size())
        {
            albedoSRV = model.TextureSRVs[mat.BaseColorTexture];
        }

        if (mat.NormalTexture >= 0 &&
            mat.NormalTexture < (int)model.TextureSRVs.size())
        {
            normalSRV = model.TextureSRVs[mat.NormalTexture];
        }

        if (mat.EmissiveTexture >= 0 &&
            mat.EmissiveTexture < (int)model.TextureSRVs.size())
        {
            emissiveSRV = model.TextureSRVs[mat.EmissiveTexture];
        }
    }

    m_pDeviceContext->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &materialParams, 0, 0);
    m_pDeviceContext->PSSetConstantBuffers(3, 1, &m_pMaterialBuffer);

    UINT stride = sizeof(GltfVertex);
    UINT offset = 0;

    m_pDeviceContext->IASetVertexBuffers(0, 1, &primitive.VertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(primitive.IndexBuffer, primitive.IndexFormat, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &albedoSRV);
    m_pDeviceContext->PSSetShaderResources(1, 1, &normalSRV);
    m_pDeviceContext->PSSetShaderResources(5, 1, &emissiveSRV);

    m_pDeviceContext->VSSetConstantBuffers(4, 1, &m_pShadowLightBuffer);
    m_pDeviceContext->PSSetConstantBuffers(4, 1, &m_pShadowParamsBuffer);
    m_pDeviceContext->PSSetConstantBuffers(5, 1, &m_pShadowLightBuffer);
    m_pDeviceContext->PSSetShaderResources(6, 1, &m_pShadowMapSRV);

    m_pDeviceContext->PSSetSamplers(2, 1, &m_pShadowSampler);

    m_pDeviceContext->DrawIndexed(primitive.IndexCount, 0, 0);
}

void RenderClass::RenderGltfNode(
    const GltfModelResource& model,
    int nodeIndex,
    const XMMATRIX& viewProj,
    const XMMATRIX& instanceWorld,
    bool receiveShadow)
{
    if (nodeIndex < 0 || nodeIndex >= (int)model.Scene.Nodes.size())
        return;

    const GltfNodeData& node = model.Scene.Nodes[nodeIndex];
    XMMATRIX nodeWorld = XMLoadFloat4x4(&node.WorldMatrix);

    XMMATRIX finalWorld = nodeWorld * instanceWorld;

    if (node.MeshIndex >= 0 && node.MeshIndex < (int)model.GpuMeshes.size())
    {
        const GltfGpuMesh& mesh = model.GpuMeshes[node.MeshIndex];
        for (const auto& prim : mesh.Primitives)
            DrawGltfPrimitive(model, prim, finalWorld, viewProj, receiveShadow);
    }

    for (int child : node.Children)
        RenderGltfNode(model, child, viewProj, finalWorld, receiveShadow);
}

void RenderClass::RenderAllSceneModels(const XMMATRIX& viewProj)
{
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pLayout);
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    m_pDeviceContext->PSSetSamplers(2, 1, &m_pShadowSampler);
    m_pDeviceContext->PSSetShaderResources(2, 1, &m_pIrradianceSRV);
    m_pDeviceContext->PSSetShaderResources(3, 1, &m_pPrefilteredEnvSRV);
    m_pDeviceContext->PSSetShaderResources(7, 1, &m_pSSAOSRV);
    m_pDeviceContext->PSSetShaderResources(4, 1, &m_pBRDFLUTSRV);
    m_pDeviceContext->RSSetState(m_pGltfRasterState);

    for (const SceneModelInstance& instance : m_SceneModelInstances)
    {
        RenderModelInstance(instance, viewProj);
    }

    m_pDeviceContext->RSSetState(nullptr);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);
    m_pDeviceContext->PSSetShaderResources(1, 1, &nullSRV);
    m_pDeviceContext->PSSetShaderResources(2, 1, &nullSRV);
    m_pDeviceContext->PSSetShaderResources(3, 1, &nullSRV);
    m_pDeviceContext->PSSetShaderResources(4, 1, &nullSRV);
    m_pDeviceContext->PSSetShaderResources(5, 1, &nullSRV);
    m_pDeviceContext->PSSetShaderResources(7, 1, &nullSRV);
    m_pDeviceContext->PSSetShaderResources(6, 1, &nullSRV);
}

void RenderClass::RenderModelInstance(const SceneModelInstance& instance, const XMMATRIX& viewProj)
{
    if (instance.ModelResourceIndex < 0 ||
        instance.ModelResourceIndex >= (int)m_ModelResources.size())
    {
        return;
    }

    const GltfModelResource& model = m_ModelResources[instance.ModelResourceIndex];

    for (int rootNode : model.Scene.RootNodes)
    {
        RenderGltfNode(
            model,
            rootNode,
            viewProj,
            instance.PrecomputedWorld,
            instance.ReceiveShadow
        );
    }
}

// bloom

HRESULT RenderClass::CreateBloomResources(UINT width, UINT height)
{
    ReleaseBloomResources();
    width = std::max(1u, width / 2);
    height = std::max(1u, height / 2);
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT result = m_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pBloomTextureA);
    if (FAILED(result))
        return result;

    result = m_pDevice->CreateRenderTargetView(m_pBloomTextureA, nullptr, &m_pBloomRTVA);
    if (FAILED(result))
        return result;

    result = m_pDevice->CreateShaderResourceView(m_pBloomTextureA, nullptr, &m_pBloomSRVA);
    if (FAILED(result))
        return result;

    result = m_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pBloomTextureB);
    if (FAILED(result))
        return result;

    result = m_pDevice->CreateRenderTargetView(m_pBloomTextureB, nullptr, &m_pBloomRTVB);
    if (FAILED(result))
        return result;

    result = m_pDevice->CreateShaderResourceView(m_pBloomTextureB, nullptr, &m_pBloomSRVB);
    if (FAILED(result))
        return result;

    return S_OK;
}

void RenderClass::ReleaseBloomResources()
{
    if (m_pBloomSRVA)
    {
        m_pBloomSRVA->Release();
        m_pBloomSRVA = nullptr;
    }

    if (m_pBloomRTVA)
    {
        m_pBloomRTVA->Release();
        m_pBloomRTVA = nullptr;
    }
    if (m_pBloomTextureA)
    {
        m_pBloomTextureA->Release();
        m_pBloomTextureA = nullptr;
    }

    if (m_pBloomSRVB)
    {
        m_pBloomSRVB->Release();
        m_pBloomSRVB = nullptr;
    }

    if (m_pBloomRTVB)
    {
        m_pBloomRTVB->Release();
        m_pBloomRTVB = nullptr;
    }

    if (m_pBloomTextureB)
    {
        m_pBloomTextureB->Release();
        m_pBloomTextureB = nullptr;
    }
}

void RenderClass::ApplyBloom()
{
    if (!m_EnableBloom || !m_pHDRSceneSRV || !m_pBloomRTVA || !m_pBloomRTVB)
        return;
    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);
    float bloomWidth = std::max(1.0f, (float)(rc.right - rc.left) / 2);
    float bloomHeight = std::max(1.0f, (float)(rc.bottom - rc.top) / 2);
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = bloomWidth;
    vp.Height = bloomHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &vp);
    UINT stride = sizeof(FullScreenVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetInputLayout(m_pFullScreenLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pFullScreenQuadVB, &stride, &offset);
    m_pDeviceContext->VSSetShader(m_pFullScreenVS, nullptr, 0);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_pDeviceContext->Map(m_pBloomCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        BloomParamsCB* cb = (BloomParamsCB*)mapped.pData;
        cb->Params0 = XMFLOAT4(
            m_BloomThreshold,
            m_BloomIntensity,
            (1.0f / bloomWidth) * m_BloomBlurScale,
            (1.0f / bloomHeight) * m_BloomBlurScale
        );
        cb->Params1 = XMFLOAT4(1, 0, 1, 0);
        m_pDeviceContext->Unmap(m_pBloomCB, 0);
    }
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pBloomCB);
    float clear[4] = { 0,0,0,0 };

    m_pDeviceContext->OMSetRenderTargets(1, &m_pBloomRTVA, nullptr);
    m_pDeviceContext->ClearRenderTargetView(m_pBloomRTVA, clear);
    m_pDeviceContext->PSSetShader(m_pBloomExtractPS, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pHDRSceneSRV);
    m_pDeviceContext->Draw(4, 0);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

    if (SUCCEEDED(m_pDeviceContext->Map(m_pBloomCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        BloomParamsCB* cb = (BloomParamsCB*)mapped.pData;
        cb->Params0 = XMFLOAT4(
            m_BloomThreshold,
            m_BloomIntensity,
            (1.0f / bloomWidth) * m_BloomBlurScale,
            (1.0f / bloomHeight) * m_BloomBlurScale
        );
        cb->Params1 = XMFLOAT4(1, 0, 1, 0);
        m_pDeviceContext->Unmap(m_pBloomCB, 0);
    }
    m_pDeviceContext->OMSetRenderTargets(1, &m_pBloomRTVB, nullptr);
    m_pDeviceContext->ClearRenderTargetView(m_pBloomRTVB, clear);
    m_pDeviceContext->PSSetShader(m_pBloomBlurPS, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pBloomSRVA);
    m_pDeviceContext->Draw(4, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

    if (SUCCEEDED(m_pDeviceContext->Map(m_pBloomCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        BloomParamsCB* cb = (BloomParamsCB*)mapped.pData;
        cb->Params0 = XMFLOAT4(
            m_BloomThreshold,
            m_BloomIntensity,
            (1.0f / bloomWidth) * m_BloomBlurScale,
            (1.0f / bloomHeight) * m_BloomBlurScale
        );
        cb->Params1 = XMFLOAT4(0, 1, 1, 0);
        m_pDeviceContext->Unmap(m_pBloomCB, 0);
    }
    m_pDeviceContext->OMSetRenderTargets(1, &m_pBloomRTVA, nullptr);
    m_pDeviceContext->ClearRenderTargetView(m_pBloomRTVA, clear);
    m_pDeviceContext->PSSetShader(m_pBloomBlurPS, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pBloomSRVB);
    m_pDeviceContext->Draw(4, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);
}

// shadows

static XMMATRIX MakeShadowUvMatrix()
{
    return XMMatrixScaling(0.5f, -0.5f, 1.0f) *
        XMMatrixTranslation(0.5f, 0.5f, 0.0f);
}

void RenderClass::CollectShadowCasters()
{
    m_ShadowCasters.clear();

    for (const SceneModelInstance& inst : m_SceneModelInstances)
    {
        if (!inst.CastShadow)
            continue;

        if (inst.ModelResourceIndex < 0 ||
            inst.ModelResourceIndex >= (int)m_ModelResources.size())
        {
            continue;
        }

        const GltfModelResource& model = m_ModelResources[inst.ModelResourceIndex];

        for (int rootNode : model.Scene.RootNodes)
        {
            std::vector<int> stack;
            stack.push_back(rootNode);

            while (!stack.empty())
            {
                int nodeIndex = stack.back();
                stack.pop_back();

                if (nodeIndex < 0 || nodeIndex >= (int)model.Scene.Nodes.size())
                    continue;

                const GltfNodeData& node = model.Scene.Nodes[nodeIndex];
                XMMATRIX nodeWorld = XMLoadFloat4x4(&node.WorldMatrix);
                XMMATRIX finalWorld = nodeWorld * inst.PrecomputedWorld;

                if (node.MeshIndex >= 0 &&
                    node.MeshIndex < (int)model.GpuMeshes.size() &&
                    node.MeshIndex < (int)model.Scene.Meshes.size())
                {
                    const GltfGpuMesh& gpuMesh = model.GpuMeshes[node.MeshIndex];
                    const GltfMeshData& cpuMesh = model.Scene.Meshes[node.MeshIndex];
                    const size_t primCount = std::min(gpuMesh.Primitives.size(), cpuMesh.Primitives.size());
                    for (size_t primIndex = 0; primIndex < primCount; ++primIndex)
                    {
                        const auto& gpuPrim = gpuMesh.Primitives[primIndex];
                        const auto& cpuPrim = cpuMesh.Primitives[primIndex];
                        XMFLOAT3 bmin(FLT_MAX, FLT_MAX, FLT_MAX);
                        XMFLOAT3 bmax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
                        for (const auto& v : cpuPrim.Vertices)
                        {
                            bmin.x = std::min(bmin.x, v.Position.x);
                            bmin.y = std::min(bmin.y, v.Position.y);
                            bmin.z = std::min(bmin.z, v.Position.z);
                            bmax.x = std::max(bmax.x, v.Position.x);
                            bmax.y = std::max(bmax.y, v.Position.y);
                            bmax.z = std::max(bmax.z, v.Position.z);
                        }
                        SceneShadowItem item = {};
                        item.World = finalWorld;
                        item.IndexCount = gpuPrim.IndexCount;
                        item.IsGround = false;
                        item.LocalBoundsMin = bmin;
                        item.LocalBoundsMax = bmax;
                        m_ShadowCasters.push_back(item);
                    }
                }

                for (int child : node.Children)
                    stack.push_back(child);
            }
        }
    }
}

HRESULT RenderClass::CreateShadowResources(UINT shadowMapSize)
{
    ReleaseShadowMapResources();

    m_ShadowMapSize = shadowMapSize;

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = shadowMapSize;
    texDesc.Height = shadowMapSize;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = kShadowCascadeCount;
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    texDesc.MiscFlags = 0;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = m_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pShadowMapTexture);
    if (FAILED(hr))
        return hr;

    for (UINT i = 0; i < kShadowCascadeCount; ++i)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = i;
        dsvDesc.Texture2DArray.ArraySize = 1;
        hr = m_pDevice->CreateDepthStencilView(m_pShadowMapTexture, &dsvDesc, &m_pShadowMapDSV[i]);
        if (FAILED(hr))
            return hr;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = kShadowCascadeCount;

    hr = m_pDevice->CreateShaderResourceView(m_pShadowMapTexture, &srvDesc, &m_pShadowMapSRV);
    if (FAILED(hr))
        return hr;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = sizeof(ShadowCameraBuffer);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pShadowCameraBuffer);
    if (FAILED(hr))
        return hr;

    cbDesc.ByteWidth = sizeof(CascadedShadowParamsCB);
    hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pShadowParamsBuffer);
    if (FAILED(hr))
        return hr;

    cbDesc.ByteWidth = sizeof(CascadedShadowBuffer);
    hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pShadowLightBuffer);
    if (FAILED(hr))
        return hr;
    return S_OK;
}

void RenderClass::ReleaseShadowMapResources()
{
    if (m_pShadowMapSRV)
    {
        m_pShadowMapSRV->Release();
        m_pShadowMapSRV = nullptr;
    }
    for (UINT i = 0; i < kShadowCascadeCount; ++i)
    {
        if (m_pShadowMapDSV[i])
        {
            m_pShadowMapDSV[i]->Release();
            m_pShadowMapDSV[i] = nullptr;
        }
    }
    if (m_pShadowMapTexture)
    {
        m_pShadowMapTexture->Release();
        m_pShadowMapTexture = nullptr;
    }
    if (m_pShadowCameraBuffer)
    {
        m_pShadowCameraBuffer->Release();
        m_pShadowCameraBuffer = nullptr;
    }
    if (m_pShadowParamsBuffer)
    {
        m_pShadowParamsBuffer->Release();
        m_pShadowParamsBuffer = nullptr;
    }
    if (m_pShadowLightBuffer)
    {
        m_pShadowLightBuffer->Release();
        m_pShadowLightBuffer = nullptr;
    }
}

void RenderClass::ReleaseShadowResources()
{
    ReleaseShadowMapResources();
    if (m_pShadowVertexShader)
    {
        m_pShadowVertexShader->Release();
        m_pShadowVertexShader = nullptr;
    }
    if (m_pShadowPixelShader)
    {
        m_pShadowPixelShader->Release();
        m_pShadowPixelShader = nullptr;
    }
    if (m_pShadowMaterialBuffer)
    {
        m_pShadowMaterialBuffer->Release();
        m_pShadowMaterialBuffer = nullptr;
    }
    if (m_pShadowRasterState)
    {
        m_pShadowRasterState->Release();
        m_pShadowRasterState = nullptr;
    }
    if (m_pShadowSampler)
    {
        m_pShadowSampler->Release();
        m_pShadowSampler = nullptr;
    }
    if (m_pShadowDepthState)
    {
        m_pShadowDepthState->Release();
        m_pShadowDepthState = nullptr;
    }

}

void RenderClass::ComputeCascadeSplits()
{
    if (m_ShadowMode == ShadowModeCSM)
    {
        for (UINT i = 0; i < kShadowCascadeCount; ++i)
        {
            m_ShadowSplitDists[i] = m_CascadeWorldHalfSize[i];
            m_CascadeData[i].SplitDepth = m_CascadeWorldHalfSize[i];
        }
        return;
    }

    for (UINT i = 0; i < kShadowCascadeCount; ++i)
    {
        float nearZ = m_CameraNearZ;
        float farZ = std::min(m_CameraFarZ, m_ShadowCascadeFarZ);
        float p = (i + 1) / (float)kShadowCascadeCount;

        float logSplit = nearZ * powf(farZ / nearZ, p);
        float uniSplit = nearZ + (farZ - nearZ) * p;
        float split = m_CascadeLambda * logSplit + (1.0f - m_CascadeLambda) * uniSplit;

        m_ShadowSplitDists[i] = split;
        m_CascadeData[i].SplitDepth = split;
    }
}

static void BuildAabbCorners(const XMFLOAT3& bmin, const XMFLOAT3& bmax, XMVECTOR outCorners[8])
{
    outCorners[0] = XMVectorSet(bmin.x, bmin.y, bmin.z, 1.0f);
    outCorners[1] = XMVectorSet(bmax.x, bmin.y, bmin.z, 1.0f);
    outCorners[2] = XMVectorSet(bmax.x, bmax.y, bmin.z, 1.0f);
    outCorners[3] = XMVectorSet(bmin.x, bmax.y, bmin.z, 1.0f);
    outCorners[4] = XMVectorSet(bmin.x, bmin.y, bmax.z, 1.0f);
    outCorners[5] = XMVectorSet(bmax.x, bmin.y, bmax.z, 1.0f);
    outCorners[6] = XMVectorSet(bmax.x, bmax.y, bmax.z, 1.0f);
    outCorners[7] = XMVectorSet(bmin.x, bmax.y, bmax.z, 1.0f);
}

void RenderClass::BuildCascadeMatrices(const XMMATRIX& cameraView, const XMMATRIX& cameraProj)
{
    const XMMATRIX invViewProj = XMMatrixInverse(nullptr, cameraView * cameraProj);
    const XMVECTOR lightDir = XMVector3Normalize(XMLoadFloat3(&m_ShadowLightDirection));
    XMVECTOR lightUp = XMVectorSet(0, 1, 0, 0);
    if (fabsf(XMVectorGetX(XMVector3Dot(lightDir, lightUp))) > 0.95f)
        lightUp = XMVectorSet(0, 0, 1, 0);

    const XMMATRIX uv = MakeShadowUvMatrix();
    const float nearZ = m_CameraNearZ;
    const float farZ = std::min(m_CameraFarZ, m_ShadowCascadeFarZ);
    const XMMATRIX invView = XMMatrixInverse(nullptr, cameraView);
    const XMVECTOR cameraPos = invView.r[3];

    UINT vpCount = 1;
    D3D11_VIEWPORT cameraViewport = {};
    m_pDeviceContext->RSGetViewports(&vpCount, &cameraViewport);
    const float aspect = (cameraViewport.Height > 1e-5f) ? (cameraViewport.Width / cameraViewport.Height) : 1.0f;
    const float tanHalfFovY = 1.0f / std::max(XMVectorGetY(cameraProj.r[1]), 1e-5f);

    XMVECTOR frustumCornersWS[8];
    const float corners[8][3] =
    {
        { -1, -1, 0 }, { 1, -1, 0 }, { 1, 1, 0 }, { -1, 1, 0 },
        { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 }
    };
    for (int i = 0; i < 8; ++i)
    {
        XMVECTOR corner = XMVectorSet(corners[i][0], corners[i][1], corners[i][2], 1.0f);
        frustumCornersWS[i] = XMVector3TransformCoord(corner, invViewProj);
    }

    float prevSplit = nearZ;

    XMVECTOR globalCasterMin = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
    XMVECTOR globalCasterMax = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);
    bool hasGlobalCasterBounds = false;

    for (const SceneShadowItem& caster : m_ShadowCasters)
    {
        XMVECTOR localCorners[8];
        BuildAabbCorners(caster.LocalBoundsMin, caster.LocalBoundsMax, localCorners);
        for (int i = 0; i < 8; ++i)
        {
            XMVECTOR worldCorner = XMVector3TransformCoord(localCorners[i], caster.World);
            globalCasterMin = XMVectorMin(globalCasterMin, worldCorner);
            globalCasterMax = XMVectorMax(globalCasterMax, worldCorner);
            hasGlobalCasterBounds = true;
        }
    }

    for (UINT cascade = 0; cascade < kShadowCascadeCount; ++cascade)
    {
        const float splitDist = std::min(m_ShadowSplitDists[cascade], farZ);
        float minX = 0.0f;
        float minY = 0.0f;
        float minZ = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        float maxZ = 0.0f;

        if (m_ShadowMode == ShadowModePSSM)
        {
            const float splitNearNdc = (prevSplit - nearZ) / (farZ - nearZ);
            const float splitFarNdc = (splitDist - nearZ) / (farZ - nearZ);

            XMVECTOR cornersWS[8];

            for (int i = 0; i < 4; ++i)
            {
                XMVECTOR nearCorner = frustumCornersWS[i];
                XMVECTOR farCorner = frustumCornersWS[i + 4];
                cornersWS[i] = XMVectorLerp(nearCorner, farCorner, splitNearNdc);
                cornersWS[i + 4] = XMVectorLerp(nearCorner, farCorner, splitFarNdc);
            }

            XMVECTOR frustumCenter = XMVectorZero();
            for (int i = 0; i < 8; ++i)
                frustumCenter += cornersWS[i];
            frustumCenter *= (1.0f / 8.0f);

            float radius = 0.0f;
            for (int i = 0; i < 8; ++i)
            {
                float dist = XMVectorGetX(XMVector3Length(cornersWS[i] - frustumCenter));
                radius = std::max(radius, dist);
            }
            radius = ceilf(radius * 16.0f) / 16.0f;

            const XMVECTOR lightPos = frustumCenter - lightDir * (radius * 4.0f + 20.0f);
            const XMMATRIX lightView = XMMatrixLookAtLH(lightPos, frustumCenter, lightUp);

            XMVECTOR mins = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
            XMVECTOR maxs = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);

            for (int i = 0; i < 8; ++i)
            {
                XMVECTOR ls = XMVector3TransformCoord(cornersWS[i], lightView);
                mins = XMVectorMin(mins, ls);
                maxs = XMVectorMax(maxs, ls);
            }

            minX = XMVectorGetX(mins);
            minY = XMVectorGetY(mins);
            minZ = XMVectorGetZ(mins);
            maxX = XMVectorGetX(maxs);
            maxY = XMVectorGetY(maxs);
            maxZ = XMVectorGetZ(maxs);

            float casterMinZ = minZ;
            float casterMaxZ = maxZ;
            for (const SceneShadowItem& caster : m_ShadowCasters)
            {
                XMVECTOR localCorners[8];
                BuildAabbCorners(caster.LocalBoundsMin, caster.LocalBoundsMax, localCorners);
                for (int i = 0; i < 8; ++i)
                {
                    XMVECTOR worldCorner = XMVector3TransformCoord(localCorners[i], caster.World);
                    XMVECTOR ls = XMVector3TransformCoord(worldCorner, lightView);
                    casterMinZ = std::min(casterMinZ, XMVectorGetZ(ls));
                    casterMaxZ = std::max(casterMaxZ, XMVectorGetZ(ls));
                }
            }

            minZ = casterMinZ - 35.0f;
            maxZ = casterMaxZ + 35.0f;

            float cascadeSize = std::max(maxX - minX, maxY - minY);
            float texelWorld = cascadeSize / float(m_ShadowMapSize);
            float centerX = 0.5f * (minX + maxX);
            float centerY = 0.5f * (minY + maxY);
            centerX = floorf(centerX / texelWorld) * texelWorld;
            centerY = floorf(centerY / texelWorld) * texelWorld;

            float halfSize = 0.5f * cascadeSize;
            minX = centerX - halfSize;
            maxX = centerX + halfSize;
            minY = centerY - halfSize;
            maxY = centerY + halfSize;

            const float xyPadding = std::max(texelWorld * 8.0f, 2.0f);
            minX -= xyPadding;
            maxX += xyPadding;
            minY -= xyPadding;
            maxY += xyPadding;

            XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);
            m_CascadeData[cascade].LightViewProj = lightView * lightProj;
            m_CascadeData[cascade].WorldToLightUV = lightView * lightProj * uv;
        }
        else
        {
            XMFLOAT3 cam;
            XMStoreFloat3(&cam, cameraPos);

            XMVECTOR cascadeCenter = XMVectorSet(cam.x, 0.0f, cam.z, 1.0f);

            const float cascadeHalfExtent = m_CascadeWorldHalfSize[cascade];
            const float lightDistance = m_ShadowCascadeFarZ + cascadeHalfExtent * 2.0f;

            const XMVECTOR lightPos = cascadeCenter - lightDir * lightDistance;
            const XMMATRIX lightView = XMMatrixLookAtLH(lightPos, cascadeCenter, lightUp);

            XMVECTOR centerLS = XMVector3TransformCoord(cascadeCenter, lightView);

            float texelWorld = (cascadeHalfExtent * 2.0f) / float(m_ShadowMapSize);
            float centerX = XMVectorGetX(centerLS);
            float centerY = XMVectorGetY(centerLS);

            centerX = floorf(centerX / texelWorld) * texelWorld;
            centerY = floorf(centerY / texelWorld) * texelWorld;

            float minX = centerX - cascadeHalfExtent;
            float maxX = centerX + cascadeHalfExtent;
            float minY = centerY - cascadeHalfExtent;
            float maxY = centerY + cascadeHalfExtent;
            float minZ = XMVectorGetZ(centerLS) - lightDistance;
            float maxZ = XMVectorGetZ(centerLS) + lightDistance;

            if (hasGlobalCasterBounds)
            {
                XMVECTOR casterCorners[8];

                XMFLOAT3 bmin, bmax;
                XMStoreFloat3(&bmin, globalCasterMin);
                XMStoreFloat3(&bmax, globalCasterMax);

                BuildAabbCorners(bmin, bmax, casterCorners);

                float casterMinZ = FLT_MAX;
                float casterMaxZ = -FLT_MAX;

                for (int i = 0; i < 8; ++i)
                {
                    XMVECTOR ls = XMVector3TransformCoord(casterCorners[i], lightView);
                    casterMinZ = std::min(casterMinZ, XMVectorGetZ(ls));
                    casterMaxZ = std::max(casterMaxZ, XMVectorGetZ(ls));
                }
                minZ = casterMinZ - 20.0f;
                maxZ = casterMaxZ + 20.0f;
            }

            XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(
                minX, maxX,
                minY, maxY,
                minZ, maxZ
            );

            m_CascadeData[cascade].LightViewProj = lightView * lightProj;
            m_CascadeData[cascade].WorldToLightUV = lightView * lightProj * uv;
        }
        m_CascadeData[cascade].SplitDepth = splitDist;

        prevSplit = splitDist;
    }
}

void RenderClass::UpdateCascadedShadowData(const XMMATRIX& cameraView, const XMMATRIX& cameraProj)
{
    ComputeCascadeSplits();
    BuildCascadeMatrices(cameraView, cameraProj);

    CascadedShadowBuffer lightCB = {};
    for (UINT i = 0; i < kShadowCascadeCount; ++i)
    {
        lightCB.WorldToLightUV[i] = XMMatrixTranspose(m_CascadeData[i].WorldToLightUV);
    }

    if (m_ShadowMode == ShadowModeCSM)
    {
        lightCB.CascadeSplits = XMFLOAT4(
            m_CascadeWorldHalfSize[0],
            m_CascadeWorldHalfSize[1],
            m_CascadeWorldHalfSize[2],
            m_CascadeWorldHalfSize[3]
        );
    }
    else
    {
        lightCB.CascadeSplits = XMFLOAT4(
            m_CascadeData[0].SplitDepth,
            m_CascadeData[1].SplitDepth,
            m_CascadeData[2].SplitDepth,
            m_CascadeData[3].SplitDepth
        );
    }


    XMVECTOR lightDirV = XMVector3Normalize(XMLoadFloat3(&m_ShadowLightDirection));
    XMFLOAT3 lightDir;
    XMStoreFloat3(&lightDir, lightDirV);

    lightCB.ShadowLightDirStrength = XMFLOAT4(
        lightDir.x,
        lightDir.y,
        lightDir.z,
        m_ShadowStrength
    );

    lightCB.CsmRatio = XMFLOAT4(
        m_CascadeData[0].SplitDepth / m_CascadeData[3].SplitDepth,
        m_CascadeData[1].SplitDepth / m_CascadeData[3].SplitDepth,
        m_CascadeData[2].SplitDepth / m_CascadeData[3].SplitDepth,
        1.0f
    );
    XMMATRIX invView = XMMatrixInverse(nullptr, cameraView);
    XMVECTOR cameraForwardV = XMVector3Normalize(invView.r[2]);
    XMFLOAT3 cameraForward;
    XMStoreFloat3(&cameraForward, cameraForwardV);
    lightCB.CameraForward = XMFLOAT4(
        cameraForward.x,
        cameraForward.y,
        cameraForward.z,
        0.0f
    );

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_pDeviceContext->Map(m_pShadowLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &lightCB, sizeof(lightCB));
        m_pDeviceContext->Unmap(m_pShadowLightBuffer, 0);
    }

    CascadedShadowParamsCB paramsCB = {};
    const float shadowTexel = 1.0f / (float)m_ShadowMapSize;

    const float cascadeBlendUv =
        (m_ShadowMode == ShadowModeCSM)
        ? 8.0f * shadowTexel
        : m_CascadeBlendBand;

    paramsCB.ShadowBiasTexelSizeBlend = XMFLOAT4(
        m_ShadowReceiverConstBias,
        m_ShadowReceiverSlopeBias,
        shadowTexel,
        cascadeBlendUv
    );

    paramsCB.ShadowOptions = XMFLOAT4(
        (float)m_ShadowMode,
        m_TintSplits ? 1.0f : 0.0f,
        m_ShadowPcfMinRadius,
        m_ShadowPcfMaxRadius
    );

    if (SUCCEEDED(m_pDeviceContext->Map(m_pShadowParamsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &paramsCB, sizeof(paramsCB));
        m_pDeviceContext->Unmap(m_pShadowParamsBuffer, 0);
    }
}

void RenderClass::RenderCascadedShadowPass()
{
    if (!m_pShadowVertexShader)
        return;

    ID3D11RenderTargetView* nullRTV = nullptr;
    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)m_ShadowMapSize;
    vp.Height = (FLOAT)m_ShadowMapSize;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;

    m_pDeviceContext->RSSetViewports(1, &vp);
    m_pDeviceContext->RSSetState(m_pShadowRasterState);
    ID3D11ShaderResourceView* nullSRVs[8] = {};
    m_pDeviceContext->PSSetShaderResources(0, 8, nullSRVs);
    m_pDeviceContext->VSSetShader(m_pShadowVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pShadowPixelShader, nullptr, 0);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    m_pDeviceContext->IASetInputLayout(m_pLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (UINT cascade = 0; cascade < kShadowCascadeCount; ++cascade)
    {
        ShadowCameraBuffer cb = {};
        cb.LightViewProj = XMMatrixTranspose(m_CascadeData[cascade].LightViewProj);
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        HRESULT hr = m_pDeviceContext->Map(
            m_pShadowCameraBuffer,
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mapped
        );
        if (SUCCEEDED(hr))
        {
            memcpy(mapped.pData, &cb, sizeof(cb));
            m_pDeviceContext->Unmap(m_pShadowCameraBuffer, 0);
        }
        m_pDeviceContext->VSSetConstantBuffers(4, 1, &m_pShadowCameraBuffer);
        m_pDeviceContext->OMSetRenderTargets(1, &nullRTV, m_pShadowMapDSV[cascade]);

        m_pDeviceContext->ClearDepthStencilView(
            m_pShadowMapDSV[cascade],
            D3D11_CLEAR_DEPTH,
            1.0f,
            0
        );
        RenderAllSceneModelsShadow();
    }

    m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    m_pDeviceContext->RSSetState(nullptr);
}

void RenderClass::RenderGroundPlaneShadow()
{
    if (!m_pGroundVB || !m_pGroundIB)
        return;

    UINT stride = sizeof(GroundVertex);
    UINT offset = 0;

    XMMATRIX world = XMMatrixIdentity();
    XMMATRIX worldT = XMMatrixTranspose(world);

    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &worldT, 0, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);

    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pGroundVB, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pGroundIB, DXGI_FORMAT_R32_UINT, 0);
    m_pDeviceContext->DrawIndexed(m_GroundIndexCount, 0, 0);
}


// utils 

void RenderClass::Resize(HWND hWnd)
{
    if (!m_pSwapChain || !m_pDeviceContext)
        return;

    m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* nullSRVs[8] = {};
    m_pDeviceContext->PSSetShaderResources(0, 8, nullSRVs);

    if (m_pRenderTargetView)
    {
        m_pRenderTargetView->Release();
        m_pRenderTargetView = nullptr;
    }

    if (m_pDepthSRV)
    {
        m_pDepthSRV->Release();
        m_pDepthSRV = nullptr;
    }

    if (m_pDepthView)
    {
        m_pDepthView->Release();
        m_pDepthView = nullptr;
    }

    if (m_pDepthReadOnlyView)
    {
        m_pDepthReadOnlyView->Release();
        m_pDepthReadOnlyView = nullptr;
    }

    if (m_pDepthTexture)
    {
        m_pDepthTexture->Release();
        m_pDepthTexture = nullptr;
    }

    if (m_pHDRSceneSRV)
    {
        m_pHDRSceneSRV->Release();
        m_pHDRSceneSRV = nullptr;
    }

    if (m_pHDRSceneRTV)
    {
        m_pHDRSceneRTV->Release();
        m_pHDRSceneRTV = nullptr;
    }

    if (m_pHDRSceneTexture)
    {
        m_pHDRSceneTexture->Release();
        m_pHDRSceneTexture = nullptr;
    }

    ReleaseBloomResources();
    ReleaseSSAOResources();
    ReleaseGBufferResources();

    HRESULT hrShadow = CreateShadowResources(m_ShadowMapSize);
    if (FAILED(hrShadow))
    {
        OutputDebugString(_T("CreateShadowResources failed.\n"));
        return;
    }

    HRESULT hr;

    RECT rc;
    GetClientRect(hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    hr = m_pSwapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"ResizeBuffers failed.", L"Error", MB_OK);
        return;
    }

    HRESULT resultBack = ConfigureBackBuffer(width, height);
    if (FAILED(resultBack))
    {
        MessageBox(nullptr, L"Configure back buffer failed.", L"Error", MB_OK);
        return;
    }

    hr = CreateGBufferResources(width, height);
    if (FAILED(hr))
    {
        OutputDebugString(_T("CreateGBufferResources failed.\n"));
        return;
    }

    hr = CreateSSAOResources(width, height);
    if (FAILED(hr))
    {
        OutputDebugString(_T("CreateSSAOResources failed.\n"));
        return;
    }

    hr = CreateHDRSceneTexture(width, height);
    if (FAILED(hr))
    {
        OutputDebugString(_T("CreateHDRSceneTexture failed.\n"));
        return;
    }

    hr = CreateBloomResources(width, height);
    if (FAILED(hr))
    {
        OutputDebugString(_T("CreateBloomResources failed.\n"));
        return;
    }

    hr = InitLuminanceResources(width, height);
    if (FAILED(hr))
    {
        OutputDebugString(_T("InitLuminanceResources failed.\n"));
        return;
    }

    m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthView);

    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_pDeviceContext->RSSetViewports(1, &vp);
}

void RenderClass::MoveCamera(float dx, float dy, float dz)
{
    m_CameraPosition.x += dx * m_CameraSpeed;
    m_CameraPosition.y += dy * m_CameraSpeed;
    m_CameraPosition.z += dz * m_CameraSpeed;
}

void RenderClass::RotateCamera(float lrAngle, float udAngle)
{
    m_LRAngle += lrAngle;
    m_UDAngle += udAngle;

    if (m_LRAngle > XM_2PI) m_LRAngle -= XM_2PI;
    if (m_LRAngle < -XM_2PI) m_LRAngle += XM_2PI;

    if (m_UDAngle > XM_PIDIV2) m_UDAngle = XM_PIDIV2;
    if (m_UDAngle < -XM_PIDIV2) m_UDAngle = -XM_PIDIV2;
}

void RenderClass::MouseRBPressed(bool pressed, int x, int y)
{
    m_rbPressed = pressed;
    if (m_rbPressed)
    {
        m_prevMouseX = x;
        m_prevMouseY = y;
    }
}

void RenderClass::MouseMoved(int x, int y, HWND hWnd)
{
    if (m_rbPressed)
    {
        int dx = x - m_prevMouseX;
        int dy = y - m_prevMouseY;

        const float sens = 0.0015f;
        m_LRAngle += dx * sens;
        m_UDAngle -= dy * sens;

        m_UDAngle = std::min(std::max(m_UDAngle, -XM_PIDIV2 + 0.01f), XM_PIDIV2 - 0.01f);

        m_prevMouseX = x;
        m_prevMouseY = y;
    }
}

void RenderClass::MouseWheel(int delta)
{
    float steps = delta * 0.005f;
    XMVECTOR forward = XMVectorSet(
        cosf(m_UDAngle) * sinf(m_LRAngle),
        sinf(m_UDAngle),
        cosf(m_UDAngle) * cosf(m_LRAngle),
        0.0f
    );

    XMVECTOR camPos = XMLoadFloat3(&m_CameraPosition);
    camPos += forward * steps;

    XMStoreFloat3(&m_CameraPosition, camPos);
}

void RenderClass::MoveCube(float dx, float dy, float dz)
{
    m_CubePosition.x += dx * m_CubeMoveSpeed;
    m_CubePosition.y += dy * m_CubeMoveSpeed;
    m_CubePosition.z += dz * m_CubeMoveSpeed;
}

void RenderClass::RenderAllSceneModelsShadow()
{
    for (const SceneModelInstance& instance : m_SceneModelInstances)
    {
        if (!instance.CastShadow)
            continue;

        RenderModelInstanceShadow(instance);
    }
}


void RenderClass::RenderModelInstanceShadow(const SceneModelInstance& instance)
{
    if (instance.ModelResourceIndex < 0 ||
        instance.ModelResourceIndex >= (int)m_ModelResources.size())
        return;

    const GltfModelResource& model = m_ModelResources[instance.ModelResourceIndex];
    for (int rootNode : model.Scene.RootNodes)
        RenderGltfNodeShadow(model, rootNode, instance.PrecomputedWorld);
}

void RenderClass::RenderGltfNodeShadow(
    const GltfModelResource& model,
    int nodeIndex,
    const XMMATRIX& instanceWorld)
{
    if (nodeIndex < 0 || nodeIndex >= (int)model.Scene.Nodes.size())
        return;

    const GltfNodeData& node = model.Scene.Nodes[nodeIndex];
    XMMATRIX nodeWorld = XMLoadFloat4x4(&node.WorldMatrix);
    XMMATRIX finalWorld = nodeWorld * instanceWorld;

    if (node.MeshIndex >= 0 && node.MeshIndex < (int)model.GpuMeshes.size())
    {
        const GltfGpuMesh& mesh = model.GpuMeshes[node.MeshIndex];
        for (const auto& prim : mesh.Primitives)
            DrawGltfPrimitiveShadow(model, prim, finalWorld);
    }

    for (int child : node.Children)
        RenderGltfNodeShadow(model, child, finalWorld);
}

void RenderClass::DrawGltfPrimitiveShadow(
    const GltfModelResource& model,
    const GltfGpuPrimitive& primitive,
    const XMMATRIX& world)
{
    if (!primitive.VertexBuffer || !primitive.IndexBuffer || primitive.IndexCount == 0)
        return;
    UINT stride = sizeof(GltfVertex);
    UINT offset = 0;
    XMMATRIX worldT = XMMatrixTranspose(world);
    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &worldT, 0, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    ShadowMaterialCB shadowMat = {};
    shadowMat.AlphaParams = XMFLOAT4(
        0.0f,
        1.0f,
        0.5f,
        0.0f
    );
    ID3D11ShaderResourceView* baseColorSRV = nullptr;
    if (primitive.MaterialIndex >= 0 &&
        primitive.MaterialIndex < (int)model.Scene.Materials.size())
    {
        const GltfMaterial& mat = model.Scene.Materials[primitive.MaterialIndex];
        shadowMat.AlphaParams.y = mat.BaseColorFactor.w;
        shadowMat.AlphaParams.z = mat.AlphaCutoff;
        shadowMat.AlphaParams.w = mat.AlphaMask ? 1.0f : 0.0f;
        if (mat.BaseColorTexture >= 0 &&
            mat.BaseColorTexture < (int)model.TextureSRVs.size() &&
            model.TextureSRVs[mat.BaseColorTexture] != nullptr)
        {
            baseColorSRV = model.TextureSRVs[mat.BaseColorTexture];
            shadowMat.AlphaParams.x = 1.0f;
        }
    }
    m_pDeviceContext->UpdateSubresource(
        m_pShadowMaterialBuffer,
        0,
        nullptr,
        &shadowMat,
        0,
        0
    );
    m_pDeviceContext->PSSetConstantBuffers(1, 1, &m_pShadowMaterialBuffer);
    m_pDeviceContext->PSSetShaderResources(0, 1, &baseColorSRV);
    m_pDeviceContext->IASetVertexBuffers(0, 1, &primitive.VertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(primitive.IndexBuffer, primitive.IndexFormat, 0);
    m_pDeviceContext->DrawIndexed(primitive.IndexCount, 0, 0);
}


// imgui

void RenderClass::InitImGui(HWND hWnd)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);
}

void RenderClass::ShutdownImGui()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void RenderClass::RenderImGui()
{
    m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);

    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);

    D3D11_VIEWPORT vp = {};
    vp.Width = float(rc.right - rc.left);
    vp.Height = float(rc.bottom - rc.top);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    m_pDeviceContext->RSSetViewports(1, &vp);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Renderer Controls");

    ImGui::TextUnformatted("Material and IBL");
    ImGui::Checkbox("Enable specular IBL", &m_EnableSpecularIBL);
    ImGui::SliderFloat("Diffuse IBL intensity", &m_DiffuseIBLIntensity, 0.0f, 3.0f);
    ImGui::SliderFloat("Specular IBL intensity", &m_SpecularIBLIntensity, 0.0f, 3.0f);

    static const char* debugModes[] =
    {
        "Final",
        "Normal Distribution Function",
        "Geometry Function",
        "Fresnel Function",
        "Diffuse IBL",
        "Specular IBL",
        "Ambient IBL",
        "Reflection only",
        "SSAO mask",
        "Normal buffer",
        "Depth buffer",
        "Ground normal map markers",
        "GBuffer albedo",
        "GBuffer material",
        "GBuffer emissive",
        "Deferred lighting"
    };
 
    ImGui::Combo("View mode", &m_DebugViewMode, debugModes, IM_ARRAYSIZE(debugModes));
    ImGui::Separator();

    ImGui::TextUnformatted("Environment");
    if (!m_environmentFileNames.empty())
    {
        std::vector<const char*> items;
        for (const auto& name : m_environmentFileNames)
            items.push_back(name.c_str());

        if (ImGui::Combo("HDRI map", &m_currentEnvIndex, items.data(), static_cast<int>(items.size())))
        {
            if (m_currentEnvIndex >= 0 &&
                m_currentEnvIndex < static_cast<int>(m_environmentFiles.size()) &&
                m_currentEnvIndex != m_prevEnvIndex)
            {
                LoadEnvironmentMap(m_environmentFiles[m_currentEnvIndex].c_str());
                m_prevEnvIndex = m_currentEnvIndex;
            }
        }
    }
    ImGui::Separator();

    //ImGui::TextUnformatted("Eye adaptation");
    //ImGui::Text("Current luminance: %.4f", m_CurrentLuminance);
    //ImGui::Text("Adapted luminance: %.4f", m_AdaptedLuminance);
    //ImGui::SliderFloat("Adaptation time", &m_EyeAdaptationTime, 0.05f, 5.0f);
    //ImGui::Separator();

    ImGui::TextUnformatted("Bloom");
    ImGui::Checkbox("Enable bloom", &m_EnableBloom);
    ImGui::SliderFloat("Threshold", &m_BloomThreshold, 0.1f, 10.0f);
    ImGui::SliderFloat("Blur intensity", &m_BloomIntensity, 0.0f, 3.0f);
    ImGui::SliderFloat("Blur scale", &m_BloomBlurScale, 0.5f, 3.0f);
    ImGui::Separator();

    ImGui::TextUnformatted("Directional light");
    ImGui::Checkbox("Show cascade split colors", &m_ShowCascadeSplitColors);

    bool lightChanged = false;
    lightChanged |= ImGui::SliderFloat("Horizontal (phi)", &m_ShadowLightYawDeg, -180.0f, 180.0f, "%.1f deg");
    lightChanged |= ImGui::SliderFloat("Vertical (theta)", &m_ShadowLightPitchDeg, -89.0f, 89.0f, " % .1f deg");
    if (lightChanged)
    {
        UpdateShadowLightDirectionFromAngles();
    }
    ImGui::SliderFloat("Intensity", &m_LightBrightness[0], 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Shadow strength", &m_ShadowStrength, 0.0f, 1.0f, "%.2f");
    ImGui::Separator();

    ImGui::TextUnformatted("Deferred point lights");
    ImGui::SliderInt("Active point lights", &m_DeferredPointLightCount, 0, static_cast<int>(DEFERRED_MAX_POINT_LIGHTS));
    ImGui::SliderFloat("Point light radius", &m_DeferredLightRadius, 1.0f, 40.0f, "%.1f");
    ImGui::SliderFloat("Point light intensity", &m_DeferredLightIntensityScale, 0.0f, 5.0f, "%.2f");
    ImGui::Separator();

    ImGui::TextUnformatted("SSAO");
    ImGui::Checkbox("Enable SSAO", &m_EnableSSAO);
    static const char* ssaoModes[] =
    {
        "Basic",
        "Half sphere",
        "Half sphere + noise"
    };
    ImGui::Combo("SSAO mode", &m_SSAOMode, ssaoModes, IM_ARRAYSIZE(ssaoModes));
    ImGui::SliderFloat("SSAO kernel radius", &m_SSAORadius, 0.05f, 5.0f, "%.2f");
    ImGui::SliderFloat("SSAO bias", &m_SSAOBias, 0.0f, 0.20f, "%.3f");
    ImGui::SliderFloat("SSAO strength", &m_SSAOStrength, 0.1f, 4.0f, "%.2f");
    int ssaoSampleCount = static_cast<int>(m_SSAOSampleCount);
    if (ImGui::SliderInt("SSAO kernel samples", &ssaoSampleCount, 1, static_cast<int>(SSAO_MAX_SAMPLE_COUNT)))
    {
        m_SSAOSampleCount = static_cast<UINT>(std::clamp(ssaoSampleCount, 1, static_cast<int>(SSAO_MAX_SAMPLE_COUNT)));
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
