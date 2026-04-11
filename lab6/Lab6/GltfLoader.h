#pragma once

#include <string>
#include <vector>
#include <DirectXMath.h>

using namespace DirectX;

struct GltfVertex
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT2 TexCoord;
};

struct GltfTextureInfo
{
    std::wstring Uri; 
    int Width = 0;
    int Height = 0;
};

struct GltfMaterial
{
    XMFLOAT4 BaseColorFactor = XMFLOAT4(1, 1, 1, 1);
    float MetallicFactor = 1.0f;
    float RoughnessFactor = 1.0f;

    XMFLOAT3 EmissiveFactor = XMFLOAT3(0, 0, 0);

    int BaseColorTexture = -1;
    int MetallicRoughnessTexture = -1;
    int NormalTexture = -1;
    int EmissiveTexture = -1;
};

struct GltfPrimitiveData
{
    std::vector<GltfVertex> Vertices;
    std::vector<uint32_t> Indices;
    int MaterialIndex = -1;
};

struct GltfMeshData
{
    std::string Name;
    std::vector<GltfPrimitiveData> Primitives;
};

struct GltfNodeData
{
    std::string Name;
    int MeshIndex = -1;
    DirectX::XMFLOAT4X4 LocalMatrix{};
    DirectX::XMFLOAT4X4 WorldMatrix{};
    std::vector<int> Children;
};

struct LoadedGltfScene
{
    std::vector<GltfMeshData> Meshes;
    std::vector<GltfMaterial> Materials;
    std::vector<GltfTextureInfo> Textures;
    std::vector<GltfNodeData> Nodes;

    int SceneRoot = -1;
};

bool LoadGltfScene(const std::wstring& filePath, LoadedGltfScene& outScene);