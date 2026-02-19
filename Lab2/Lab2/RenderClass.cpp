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
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
            x , y + 6, 16, 16,
            hWnd, (HMENU)(1200 + i), GetModuleHandleW(nullptr), nullptr
        );
        SendMessageW(m_hLightSlider[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
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

    static const CubeVertex vertices[] =
    {
        { {-1.0f, -1.0f,  1.0f}, { 0.0f,  -1.0f,  0.0f}, {0.0f, 1.0f} },
        { { 1.0f, -1.0f,  1.0f}, { 0.0f,  -1.0f,  0.0f}, {1.0f, 1.0f} },
        { { 1.0f, -1.0f, -1.0f}, { 0.0f,  -1.0f,  0.0f}, {1.0f, 0.0f} },
        { {-1.0f, -1.0f, -1.0f}, { 0.0f,  -1.0f,  0.0f}, {0.0f, 0.0f} },

        { {-1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f, 0.0f}, {0.0f, 1.0f} },
        { { 1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f, 0.0f}, {1.0f, 1.0f} },
        { { 1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
        { {-1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },

        { { 1.0f, -1.0f, -1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f} },
        { { 1.0f, -1.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f} },
        { { 1.0f,  1.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
        { { 1.0f,  1.0f, -1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },

        { {-1.0f, -1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f} },
        { {-1.0f, -1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f} },
        { {-1.0f,  1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
        { {-1.0f,  1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },

        { { 1.0f, -1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f} },
        { {-1.0f, -1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f} },
        { {-1.0f,  1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f} },
        { { 1.0f,  1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f} },

        { {-1.0f, -1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {0.0f, 1.0f} },
        { { 1.0f, -1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {1.0f, 1.0f} },
        { { 1.0f,  1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {1.0f, 0.0f} },
        { {-1.0f,  1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {0.0f, 0.0f} },
    };

    WORD indices[] =
    {
        0, 2, 1,
        0, 3, 2,

        4, 6, 5,
        4, 7, 6,

        8, 10, 9,
        8, 11, 10,

        12, 14, 13,
        12, 15, 14,

        16, 18, 17,
        16, 19, 18,

        20, 22, 21,
        20, 23, 22
    };

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
    bd.ByteWidth = sizeof(CubeVertex) * ARRAYSIZE(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    result = m_pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer);
    if (FAILED(result))
        return result;

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = indices;
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
    vpBufferDesc.ByteWidth = sizeof(XMMATRIX);
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

    result = DirectX::CreateDDSTextureFromFile(m_pDevice, L"cat.dds", nullptr, &m_pTextureView);
    if (FAILED(result))
        return result;

    result = DirectX::CreateDDSTextureFromFile(m_pDevice, L"cube_normal.dds", nullptr, &m_pNormalMapView);
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

    return result;
}

void RenderClass::Terminate()
{
    if (m_pDeviceContext)
    {
        m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        m_pDeviceContext->ClearState();
        m_pDeviceContext->Flush();
    }

    if (m_pAnnotation)
    {
        m_pAnnotation->Release();
        m_pAnnotation = nullptr;
    }

    TerminateBufferShader();

    if (m_pRenderTargetView)
        m_pRenderTargetView->Release();

    if (m_pDepthView)
    {
        m_pDepthView->Release();
        m_pDepthView = nullptr;
    }

    if (m_pSwapChain)
        m_pSwapChain->Release();

    if (m_pDeviceContext)
        m_pDeviceContext->Release();

    if (m_pDevice) {
#ifdef _DEBUG
        ID3D11Debug* pDebug = nullptr;
        m_pDevice->QueryInterface(IID_PPV_ARGS(&pDebug));
        if (pDebug) {
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
    if (m_pLayout)
        m_pLayout->Release();

    if (m_pPixelShader)
        m_pPixelShader->Release();

    if (m_pVertexShader)
        m_pVertexShader->Release();

    if (m_pLightPixelShader)
        m_pLightPixelShader->Release();

    if (m_pIndexBuffer)
        m_pIndexBuffer->Release();

    if (m_pVertexBuffer)
        m_pVertexBuffer->Release();

    if (m_pModelBuffer)
        m_pModelBuffer->Release();

    if (m_pVPBuffer)
        m_pVPBuffer->Release();

    if (m_pTextureView)
        m_pTextureView->Release();

    if (m_pNormalMapView)
        m_pNormalMapView->Release();

    if (m_pSamplerState)
        m_pSamplerState->Release();

    if (m_pLightBuffer)
        m_pLightBuffer->Release();

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
        m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthView);
        float BackColor[4] = { 0.48f, 0.57f, 0.48f, 1.0f };
        m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, BackColor);
        m_pDeviceContext->ClearDepthStencilView(m_pDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    }

    {
        ScopedEvent evt(m_pAnnotation, L"Update MVP");
        SetMVPBuffer();
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
        ScopedEvent evt(m_pAnnotation, L"DrawIndexed");
        m_pDeviceContext->DrawIndexed(36, 0, 0);
    }

    {
        ScopedEvent evt(m_pAnnotation, L"Present");
        m_pSwapChain->Present(1, 0);
    }
}

void RenderClass::SetMVPBuffer()
{
    m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthView); 
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

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResource.pData, &vpT, sizeof(XMMATRIX));
        m_pDeviceContext->Unmap(m_pVPBuffer, 0);
    }

    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);

    CameraBuffer cameraBuffer;
    cameraBuffer.vp = XMMatrixTranspose(view * proj);
    cameraBuffer.cameraPos = m_CameraPosition;

    static float orbitLight = XM_PI / 2;
    orbitLight += 0.01f;
    if (orbitLight > XM_2PI) orbitLight -= XM_2PI;

    PointLight lights[3];

    const float range = 4.5f;
    const float intensity = 1.0f;

    static float t = 0.0f;
    t += 0.01f;
    if (t > XM_2PI) t -= XM_2PI;

    float r = 2.5f;

    // pink
    lights[0].Position = XMFLOAT3(0.2f, 0.0f, -1.2 * r);
    lights[0].Range = range;
    lights[0].Color = XMFLOAT3(1.0f, 0.35f, 0.65f);

    // cyan
    lights[1].Position = XMFLOAT3(0.0f, -0.4f, -1.2 * r);
    lights[1].Range = range;
    lights[1].Color = XMFLOAT3(0.20f, 0.95f, 0.85f);

    // purple
    lights[2].Position = XMFLOAT3(0.4f, 0.4f, -1.2 * r);
    lights[2].Range = range;
    lights[2].Color = XMFLOAT3(0.55f, 0.35f, 1.0f);

    lights[0].Intensity = m_LightBrightness[0];
    lights[1].Intensity = m_LightBrightness[1];
    lights[2].Intensity = m_LightBrightness[2];

    D3D11_MAPPED_SUBRESOURCE mappedResourceLight;
    hr = m_pDeviceContext->Map(m_pLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceLight);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResourceLight.pData, lights, sizeof(PointLight) * 3);
        m_pDeviceContext->Unmap(m_pLightBuffer, 0);
    }
    m_pDeviceContext->PSSetConstantBuffers(2, 1, &m_pLightBuffer);


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
    m_pDeviceContext->DrawIndexed(36, 0, 0);

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

        m_pDeviceContext->DrawIndexed(36, 0, 0);
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

    HRESULT hr;

    RECT rc;
    GetClientRect(hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    hr = m_pSwapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
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
    float steps = delta;
    m_CameraPosition.z -= steps * 0.005f;
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
    if (value > 5.0f) value = 5.0f;
    m_LightBrightness[index] = value;
}
float RenderClass::GetLightBrightness(int index) const
{
    if (index < 0 || index >= 3) return 0.0f;
    return m_LightBrightness[index];
}