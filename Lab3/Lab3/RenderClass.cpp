#include "framework.h"
#include "RenderClass.h"
#include "DDSTextureLoader11.h"

#include <filesystem>

#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")


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
    XMFLOAT3 cameraPos;
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
};


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
    {
        m_pDeviceContext->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&m_pAnnotation);
    }

    if (SUCCEEDED(result))
    {
        DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        //swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
        {
            result = CreateHDRSceneTexture(width, height);
        }

        if (SUCCEEDED(result))
        {
            result = InitLuminanceResources(width, height);
        }

        D3D11_VIEWPORT vp = {};
        vp.Width = (FLOAT)width;
        vp.Height = (FLOAT)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        m_pDeviceContext->RSSetViewports(1, &vp);
    }

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    const int x = 20;
    int y = 20;
    const int w = 220;
    const int h = 30;
    const int gap = 10;
    for (int i = 0; i < 3; i++)
    {
        m_hLightSlider[i] = CreateWindowExW(
            0, TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            x + 35, y, w, h,
            hWnd, (HMENU)(1000 + i), GetModuleHandleW(nullptr), nullptr
        );
        m_hLightSwatch[i] = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            x, y + 6, 16, 16,
            hWnd, (HMENU)(1200 + i), GetModuleHandleW(nullptr), nullptr
        );
        SendMessageW(m_hLightSlider[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        int pos = (int)(m_LightBrightness[i] * 100.0f);
        SendMessageW(m_hLightSlider[i], TBM_SETPOS, TRUE, pos);
        y += h + gap;
    }

    if (SUCCEEDED(result))
    {
        result = InitBufferShader();
    }

    if (SUCCEEDED(result))
    {
        m_CameraR = 5.0f;
        m_CameraPosition = XMFLOAT3(0.0f, 0.0f, -m_CameraR);
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
    {
        Terminate();
    }

    InitImGui(hWnd);

    return result;
}

HRESULT RenderClass::InitBufferShader()
{
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(CubeVertex, xyz),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(CubeVertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(CubeVertex, uv),     D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    HRESULT result = S_OK;

    ID3DBlob* pVertexCode = nullptr;
    if (SUCCEEDED(result))
    {
        result = CompileShader(L"ColorVertex.vs", &m_pVertexShader, nullptr, &pVertexCode);
    }
    if (SUCCEEDED(result))
    {
        result = CompileShader(L"ColorPixel.ps", nullptr, &m_pPixelShader);
    }

    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateInputLayout(layout, 3, pVertexCode->GetBufferPointer(), pVertexCode->GetBufferSize(), &m_pLayout);
    }

    if (pVertexCode)
        pVertexCode->Release();

    if (SUCCEEDED(result))
    {
        result = CompileShader(L"LightPixel.ps", nullptr, &m_pLightPixelShader);
    }

    result = CompileShader(L"ToneMapPixel.ps", nullptr, &m_pToneMapPS);

    if (SUCCEEDED(result))
    {
        result = CompileShader(L"SkyVertex.vs", &m_pSkyVertexShader, nullptr);
    }
    if (SUCCEEDED(result))
    {
        result = CompileShader(L"SkyPixel.ps", nullptr, &m_pSkyPixelShader);
    }

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

    //static const CubeVertex vertices[] =
    //{
    //    { {-1.0f, -1.0f,  1.0f}, { 0.0f,  -1.0f,  0.0f}, {0.0f, 1.0f} },
    //    { { 1.0f, -1.0f,  1.0f}, { 0.0f,  -1.0f,  0.0f}, {1.0f, 1.0f} },
    //    { { 1.0f, -1.0f, -1.0f}, { 0.0f,  -1.0f,  0.0f}, {1.0f, 0.0f} },
    //    { {-1.0f, -1.0f, -1.0f}, { 0.0f,  -1.0f,  0.0f}, {0.0f, 0.0f} },
    //
    //    { {-1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f, 0.0f}, {0.0f, 1.0f} },
    //    { { 1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f, 0.0f}, {1.0f, 1.0f} },
    //    { { 1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
    //    { {-1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },
    //
    //    { { 1.0f, -1.0f, -1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f} },
    //    { { 1.0f, -1.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f} },
    //    { { 1.0f,  1.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
    //    { { 1.0f,  1.0f, -1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },
    //
    //    { {-1.0f, -1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f} },
    //    { {-1.0f, -1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f} },
    //    { {-1.0f,  1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
    //    { {-1.0f,  1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },
    //
    //    { { 1.0f, -1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f} },
    //    { {-1.0f, -1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f} },
    //    { {-1.0f,  1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f} },
    //    { { 1.0f,  1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f} },
    //
    //    { {-1.0f, -1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {0.0f, 1.0f} },
    //    { { 1.0f, -1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {1.0f, 1.0f} },
    //    { { 1.0f,  1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {1.0f, 0.0f} },
    //    { {-1.0f,  1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {0.0f, 0.0f} },
    //};
    //
    //WORD indices[] =
    //{
    //    0, 2, 1,
    //    0, 3, 2,
    //
    //    4, 6, 5,
    //    4, 7, 6,
    //
    //    8, 10, 9,
    //    8, 11, 10,
    //
    //    12, 14, 13,
    //    12, 15, 14,
    //
    //    16, 18, 17,
    //    16, 19, 18,
    //
    //    20, 22, 21,
    //    20, 23, 22
    //};

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
    //bd.ByteWidth = sizeof(CubeVertex) * ARRAYSIZE(vertices);
    bd.ByteWidth = (UINT)(sizeof(CubeVertex) * vertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    /*initData.pSysMem = vertices;
    result = m_pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer);*/
    initData.pSysMem = vertices.data();
    result = m_pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer);
    if (FAILED(result))
        return result;

    bd.Usage = D3D11_USAGE_DEFAULT;
    //bd.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
    bd.ByteWidth = (UINT)(sizeof(WORD) * indices.size());
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    //initData.pSysMem = indices;
    //result = m_pDevice->CreateBuffer(&bd, &initData, &m_pIndexBuffer);
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

    //result = DirectX::CreateDDSTextureFromFile(m_pDevice, L"cat.dds", nullptr, &m_pTextureView);
    //if (FAILED(result))
    //    return result;

    //result = DirectX::CreateDDSTextureFromFile(m_pDevice, L"cube_normal.dds", nullptr, &m_pNormalMapView);
    //if (FAILED(result))
    //    return result;

    //result = DirectX::CreateDDSTextureFromFile(m_pDevice, L"cubemaps/shanghai_2048_box.dds", nullptr, &m_pEnvironmentSRV);
    result = DirectX::CreateDDSTextureFromFileEx(
        m_pDevice,
        L"cubemaps/shanghai_2048_box.dds",
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        DirectX::DDS_LOADER_FORCE_SRGB, 
        nullptr,
        &m_pEnvironmentSRV
    );
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
        if (m_pDeviceContext->GetData(m_pLuminanceQuery, nullptr, 0, 0) != S_FALSE)
        {
            m_pDeviceContext->End(m_pLuminanceQuery);
        }
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
    if (ImGui::GetCurrentContext() != nullptr)
    {
        ShutdownImGui();
    }

    TerminateBufferShader();

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

    if (m_pRenderTargetView)
    {
        m_pRenderTargetView->Release();
        m_pRenderTargetView = nullptr;
    }

    if (m_pDepthView)
    {
        m_pDepthView->Release();
        m_pDepthView = nullptr;
    }

    if (m_pSwapChain)
    {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }

    if (m_pDeviceContext)
    {
        m_pDeviceContext->ClearState();
        m_pDeviceContext->Flush();
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

    if (m_pLayout)
    {
        m_pLayout->Release();
        m_pLayout = nullptr;
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

    if (m_pSkyDepthState) 
    {
        m_pSkyDepthState->Release();
        m_pSkyDepthState = nullptr;
    }

    if (m_pTextureView)
    {
        m_pTextureView->Release();
        m_pTextureView = nullptr;
    }

    if (m_pNormalMapView)
    {
        m_pNormalMapView->Release();
        m_pNormalMapView = nullptr;
    }

    if (m_pSamplerState)
    {
        m_pSamplerState->Release();
        m_pSamplerState = nullptr;
    }
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

    {
        ScopedEvent evt(m_pAnnotation, L"Clear");
       /* ID3D11RenderTargetView* rtvs[2] = { m_pRenderTargetView, m_pHDRSceneRTV };
        m_pDeviceContext->OMSetRenderTargets(2, rtvs, m_pDepthView);*/

        m_pDeviceContext->OMSetRenderTargets(1, &m_pHDRSceneRTV, m_pDepthView);


        float BackColor[4] = { 0.48f, 0.57f, 0.48f, 1.0f };
        m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, BackColor);

        float hdrClear[4] = { 0,0,0,0 };
        if (m_pHDRSceneRTV)
            m_pDeviceContext->ClearRenderTargetView(m_pHDRSceneRTV, hdrClear);

        m_pDeviceContext->ClearDepthStencilView(m_pDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    }

    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;

    {
        ScopedEvent evt(m_pAnnotation, L"Bind pipeline");
        m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
        m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

        m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pDeviceContext->IASetInputLayout(m_pLayout);

        m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
        m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
    }

    {
        ScopedEvent evt(m_pAnnotation, L"Update MVP");
        SetMVPBuffer();
    }

    {
        ScopedEvent evt(m_pAnnotation, L"DrawIndexed");
        //m_pDeviceContext->DrawIndexed(36, 0, 0);
        m_pDeviceContext->DrawIndexed(m_indexCount, 0, 0);
    }

    {
        ScopedEvent evt(m_pAnnotation, L"Luminance Calculation");
        CalculateAverageLuminance();

        m_CurrentLuminance = ReadLuminanceFromGPU();

        static int frameCount = 0;
        frameCount++;
        if (frameCount % 20 == 0)
        {
            char buf[256];
            //sprintf_s(buf, "Average Luminance: %f\n", m_CurrentLuminance);
            sprintf_s(buf, "CurrLum=%.4f AdaptLum=%.4f\n", m_CurrentLuminance, m_AdaptedLuminance);

            OutputDebugStringA(buf);
        }

        ULONGLONG now = GetTickCount64();
        float dt = (m_LastFrameTime == 0) ? (1.0f / 60.0f)
            : float(now - m_LastFrameTime) / 10.0f;
        m_LastFrameTime = now;
        if (dt > 0.1f) dt = 0.1f;

        float tauUp = 1.5f; 
        float tauDown = 1.5f; 
        float tau = (m_CurrentLuminance > m_AdaptedLuminance) ? tauUp : tauDown;
        float k = 1.0f - expf(-dt / tau);
        m_AdaptedLuminance += (m_CurrentLuminance - m_AdaptedLuminance) * k;
    }

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

void RenderClass::SetMVPBuffer()
{
    //m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthView); 
    m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);

    //m_CubeAngle += 0.01f;
    //if (m_CubeAngle > XM_2PI) m_CubeAngle -= XM_2PI;

    //XMMATRIX rotation = XMMatrixRotationY(m_CubeAngle);

    //XMMATRIX translation = XMMatrixTranslation(m_CubePosition.x, m_CubePosition.y, m_CubePosition.z);

    //XMMATRIX model = rotation * translation;

    XMMATRIX model = XMMatrixTranslation(m_CubePosition.x, m_CubePosition.y, m_CubePosition.z);

    XMVECTOR direction = XMVectorSet(
        cosf(m_UDAngle) * sinf(m_LRAngle),
        sinf(m_UDAngle),
        cosf(m_UDAngle) * cosf(m_LRAngle),
        0.0f
    );

    XMVECTOR eyePos = XMVectorSet(m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z, 0.0f);
    XMVECTOR focusPoint = XMVectorAdd(eyePos, direction);
    XMVECTOR upDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eyePos, focusPoint, upDir);

    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);
    float aspect = static_cast<float>(rc.right - rc.left) / (rc.bottom - rc.top);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);

    XMMATRIX vp = view * proj;

    XMMATRIX mT = XMMatrixTranspose(model);
    XMMATRIX vpT = XMMatrixTranspose(vp);

    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &mT, 0, 0);

    //D3D11_MAPPED_SUBRESOURCE mappedResource;
    //HRESULT hr = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    //if (SUCCEEDED(hr))
    //{
    //    memcpy(mappedResource.pData, &vpT, sizeof(XMMATRIX));
    //    m_pDeviceContext->Unmap(m_pVPBuffer, 0);
    //}

    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);

    CameraBuffer cameraBuffer;
    cameraBuffer.vp = XMMatrixTranspose(vp);
    cameraBuffer.cameraPos = m_CameraPosition;

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResource.pData, &cameraBuffer, sizeof(CameraBuffer));
        m_pDeviceContext->Unmap(m_pVPBuffer, 0);
    }

    {
        XMMATRIX skyModel =
            XMMatrixScaling(40.0f, 40.0f, 40.0f) *
            XMMatrixTranslation(m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z);
        XMMATRIX skyModelT = XMMatrixTranspose(skyModel);
        m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &skyModelT, 0, 0);
        m_pDeviceContext->RSSetState(m_pSkyRasterState);
        m_pDeviceContext->OMSetDepthStencilState(m_pSkyDepthState, 0);
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

    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &mT, 0, 0);

    static float orbitLight = XM_PI / 2;
    orbitLight += 0.01f;
    if (orbitLight > XM_2PI)
        orbitLight -= XM_2PI;

    PointLight lights[3];

    //const float range = 1.75f;
    const float range = 6.0f;
    const float baseIntensity = 80.0f;
    float r = 1.5f;

    // red (pink)
    lights[0].Position = XMFLOAT3(0.0f, 0.5f, -r);
    lights[0].Range = range;
    //lights[0].Color = XMFLOAT3(1.0f, 0.35f, 0.65f);
    //lights[0].Color = XMFLOAT3(1.0f, 0.0f, 0.0f);
    lights[0].Color = XMFLOAT3(1.0f, 1.0f, 1.0f);

    // green (cyan)
    lights[1].Position = XMFLOAT3(-0.43f, -0.25f, -r);
    lights[1].Range = range;
    //lights[1].Color = XMFLOAT3(0.20f, 0.95f, 0.85f);
    lights[1].Color = XMFLOAT3(0.0f, 1.0f, 0.0f);
    //lights[1].Color = XMFLOAT3(1.0f, 0.0f, 0.0f);

    // blue (purple)
    lights[2].Position = XMFLOAT3(0.43f, -0.25f, -r);
    lights[2].Range = range;
    //lights[2].Color = XMFLOAT3(0.55f, 0.35f, 1.0f);
    lights[2].Color = XMFLOAT3(0.0f, 0.0f, 1.0f);
    //lights[2].Color = XMFLOAT3(1.0f, 0.0f, 0.0f);

    lights[0].Intensity = m_LightBrightness[0] * baseIntensity;
    lights[1].Intensity = m_LightBrightness[1] * baseIntensity;
    lights[2].Intensity = m_LightBrightness[2] * baseIntensity;

    D3D11_MAPPED_SUBRESOURCE mappedResourceLight;
    hr = m_pDeviceContext->Map(m_pLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceLight);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResourceLight.pData, lights, sizeof(PointLight) * 3);
        m_pDeviceContext->Unmap(m_pLightBuffer, 0);
    }
    m_pDeviceContext->PSSetConstantBuffers(2, 1, &m_pLightBuffer);

    MaterialParamsCB materialParams = {};
    // воздушный шарик (хехе)
    //materialParams.Surface = XMFLOAT4(0.05f, 0.15f, 1.0f, 1.0f);
    // пластик
    //materialParams.Surface = XMFLOAT4(1.0f, 0.6f, 1.0f, 1.0f);
    // металл
    //materialParams.Surface = XMFLOAT4(1.0f, 0.25f, 1.0f, 1.0f);
    
    materialParams.Surface = XMFLOAT4(
        m_MaterialMetalness,
        m_MaterialRoughness,
        m_MaterialAO,
        m_NormalStrength
    );

    materialParams.Albedo = XMFLOAT4(
        m_MaterialColor.x,
        m_MaterialColor.y,
        m_MaterialColor.z,
        1.0f
    );

    m_pDeviceContext->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &materialParams, 0, 0);
    m_pDeviceContext->PSSetConstantBuffers(3, 1, &m_pMaterialBuffer);

    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;

    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pLayout);

    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pTextureView);
    m_pDeviceContext->PSSetShaderResources(1, 1, &m_pNormalMapView);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    //m_pDeviceContext->DrawIndexed(36, 0, 0);
    m_pDeviceContext->DrawIndexed(m_indexCount, 0, 0);

    //static float orbit = 0.0f;
    //orbit += 0.01f;
    //XMMATRIX model2 = XMMatrixRotationZ(orbit) * XMMatrixTranslation(4.0f, 0.0f, 0.0f) * XMMatrixRotationZ(-orbit);
    //XMMATRIX mT2 = XMMatrixTranspose(model2);
    //m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &mT2, 0, 0);
    //m_pDeviceContext->DrawIndexed(36, 0, 0);

    m_pDeviceContext->PSSetShader(m_pLightPixelShader, nullptr, 0);
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pColorBuffer);

    for (int i = 0; i < 3; i++)
    {
        XMMATRIX lightModel =
            XMMatrixScaling(0.1f, 0.1f, 0.1f) *
            XMMatrixTranslation(lights[i].Position.x, lights[i].Position.y, lights[i].Position.z);

        XMMATRIX lightModelT = XMMatrixTranspose(lightModel);
        m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &lightModelT, 0, 0);

        ColorBuffer lightColorCB;
        float k = lights[i].Intensity;
        XMFLOAT3 c = lights[i].Color;
        auto clamp = [](float v) { return (v < 0.f) ? 0.f : (v > 1.f ? 1.f : v); };
        lightColorCB.color = XMFLOAT4(
            clamp(c.x * k),
            clamp(c.y * k),
            clamp(c.z * k),
            1.0f
        );

        m_pDeviceContext->UpdateSubresource(m_pColorBuffer, 0, nullptr, &lightColorCB, 0, 0);

        //m_pDeviceContext->DrawIndexed(36, 0, 0);
        m_pDeviceContext->DrawIndexed(m_indexCount, 0, 0);
    }
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
    descDepth.Format = DXGI_FORMAT_D32_FLOAT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ID3D11Texture2D* pDepthStencil = nullptr;
    hr = m_pDevice->CreateTexture2D(&descDepth, nullptr, &pDepthStencil);
    if (FAILED(hr))
        return hr;

    hr = m_pDevice->CreateDepthStencilView(pDepthStencil, nullptr, &m_pDepthView);
    pDepthStencil->Release();
    if (FAILED(hr))
        return hr;

    return hr;
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
    //texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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

void RenderClass::Resize(HWND hWnd)
{
    if (!m_pSwapChain || !m_pDeviceContext)
        return;

    m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    if (m_pRenderTargetView)
    {
        m_pRenderTargetView->Release();
        m_pRenderTargetView = nullptr;
    }

    if (m_pDepthView)
    {
        m_pDepthView->Release();
        m_pDepthView = nullptr;
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


    HRESULT hr;

    RECT rc;
    GetClientRect(hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    //hr = m_pSwapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 0);
    hr = m_pSwapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0);
    //hr = m_pSwapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
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

    hr = CreateHDRSceneTexture(width, height);
    if (FAILED(hr))
    {
        OutputDebugString(_T("CreateHDRSceneTexture failed.\n"));
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
    //m_CameraPosition.z -= steps * 0.005f;
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

void RenderClass::SetLightBrightness(int index, float value)
{
    if (index < 0 || index >= 3) return;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0) value = 1.0;
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
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_pDeviceContext->Map(m_pToneMapCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        ToneMapParamsCB* cb = (ToneMapParamsCB*)mapped.pData;
        cb->Params = XMFLOAT4(m_AdaptedLuminance, 0, 0, 0);
        m_pDeviceContext->Unmap(m_pToneMapCB, 0);
    }
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pToneMapCB);
    m_pDeviceContext->Draw(4, 0);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);
}

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
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Material");
    ImGui::SliderFloat("Metalness", &m_MaterialMetalness, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", &m_MaterialRoughness, 0.045f, 1.0f);
    ImGui::ColorEdit3("Color", &m_MaterialColor.x);
    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}


