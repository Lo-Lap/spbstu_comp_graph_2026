#include "GltfLoader.h"

#include <filesystem>
#include <iostream>
#include <windows.h>
#include <string>
#include <sstream>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include "src/tiny_gltf.h"

using namespace DirectX;
namespace fs = std::filesystem;

static bool ReadFloat3Attribute(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const std::string& attributeName,
    std::vector<XMFLOAT3>& outData)
{
    auto it = primitive.attributes.find(attributeName);
    if (it == primitive.attributes.end())
        return false;

    const tinygltf::Accessor& accessor = model.accessors[it->second];
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];

    const unsigned char* dataPtr =
        buffer.data.data() + view.byteOffset + accessor.byteOffset;

    outData.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; ++i)
    {
        const float* src = reinterpret_cast<const float*>(dataPtr + i * 3 * sizeof(float));
        outData[i] = XMFLOAT3(src[0], src[1], src[2]);
    }

    return true;
}

static bool ReadFloat2Attribute(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const std::string& attributeName,
    std::vector<XMFLOAT2>& outData)
{
    auto it = primitive.attributes.find(attributeName);
    if (it == primitive.attributes.end())
        return false;

    const tinygltf::Accessor& accessor = model.accessors[it->second];
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];

    const unsigned char* dataPtr =
        buffer.data.data() + view.byteOffset + accessor.byteOffset;

    outData.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; ++i)
    {
        const float* src = reinterpret_cast<const float*>(dataPtr + i * 2 * sizeof(float));
        outData[i] = XMFLOAT2(src[0], src[1]);
    }

    return true;
}

static bool ReadIndices(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    std::vector<uint32_t>& outIndices)
{
    if (primitive.indices < 0)
        return false;

    const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];

    const unsigned char* dataPtr =
        buffer.data.data() + view.byteOffset + accessor.byteOffset;

    outIndices.resize(accessor.count);

    for (size_t i = 0; i < accessor.count; ++i)
    {
        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            outIndices[i] = reinterpret_cast<const uint8_t*>(dataPtr)[i];
            break;

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            outIndices[i] = reinterpret_cast<const uint16_t*>(dataPtr)[i];
            break;

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            outIndices[i] = reinterpret_cast<const uint32_t*>(dataPtr)[i];
            break;

        default:
            return false;
        }
    }

    return true;
}

static bool LoadPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    GltfPrimitiveData& outPrimitive)
{
    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<XMFLOAT2> uvs;

    if (!ReadFloat3Attribute(model, primitive, "POSITION", positions))
        return false;

    ReadFloat3Attribute(model, primitive, "NORMAL", normals);
    ReadFloat2Attribute(model, primitive, "TEXCOORD_0", uvs);
    ReadIndices(model, primitive, outPrimitive.Indices);

    outPrimitive.Vertices.resize(positions.size());

    for (size_t i = 0; i < positions.size(); ++i)
    {
        outPrimitive.Vertices[i].Position = positions[i];

        if (i < normals.size())
            outPrimitive.Vertices[i].Normal = normals[i];
        else
            outPrimitive.Vertices[i].Normal = XMFLOAT3(0, 1, 0);

        if (i < uvs.size())
            outPrimitive.Vertices[i].TexCoord = uvs[i];
        else
            outPrimitive.Vertices[i].TexCoord = XMFLOAT2(0, 0);
    }

    outPrimitive.MaterialIndex = primitive.material;
    return true;
}

static void LoadMaterials(
    const tinygltf::Model& model,
    LoadedGltfScene& outScene)
{
    outScene.Materials.clear();
    outScene.Materials.reserve(model.materials.size());

    for (const tinygltf::Material& srcMat : model.materials)
    {
        GltfMaterial dstMat;

        if (srcMat.values.count("baseColorFactor"))
        {
            const auto& c = srcMat.values.at("baseColorFactor").ColorFactor();
            dstMat.BaseColorFactor = XMFLOAT4(
                (float)c[0], (float)c[1], (float)c[2], (float)c[3]);
        }

        if (srcMat.values.count("metallicFactor"))
            dstMat.MetallicFactor = (float)srcMat.values.at("metallicFactor").Factor();

        if (srcMat.values.count("roughnessFactor"))
            dstMat.RoughnessFactor = (float)srcMat.values.at("roughnessFactor").Factor();

        if (srcMat.additionalValues.count("emissiveFactor"))
        {
            const auto& e = srcMat.additionalValues.at("emissiveFactor").ColorFactor();
            dstMat.EmissiveFactor = XMFLOAT3((float)e[0], (float)e[1], (float)e[2]);
        }

        if (srcMat.values.count("baseColorTexture"))
            dstMat.BaseColorTexture = srcMat.values.at("baseColorTexture").TextureIndex();

        if (srcMat.values.count("metallicRoughnessTexture"))
            dstMat.MetallicRoughnessTexture = srcMat.values.at("metallicRoughnessTexture").TextureIndex();

        if (srcMat.additionalValues.count("normalTexture"))
            dstMat.NormalTexture = srcMat.additionalValues.at("normalTexture").TextureIndex();

        if (srcMat.additionalValues.count("emissiveTexture"))
            dstMat.EmissiveTexture = srcMat.additionalValues.at("emissiveTexture").TextureIndex();

        outScene.Materials.push_back(dstMat);
    }
}

static void LoadTextures(
    const tinygltf::Model& model,
    const std::wstring& filePath,
    LoadedGltfScene& outScene)
{
    outScene.Textures.clear();
    outScene.Textures.reserve(model.textures.size());

    fs::path baseDir = fs::path(filePath).parent_path();

    for (const tinygltf::Texture& tex : model.textures)
    {
        GltfTextureInfo info;

        if (tex.source >= 0 && tex.source < (int)model.images.size())
        {
            const tinygltf::Image& image = model.images[tex.source];

            info.Width = image.width;
            info.Height = image.height;

            if (!image.uri.empty())
            {
                fs::path fullPath = baseDir / image.uri;
                info.Uri = fullPath.wstring();
            }
        }

        outScene.Textures.push_back(info);
    }
}

static void LoadMeshes(
    const tinygltf::Model& model,
    LoadedGltfScene& outScene)
{
    outScene.Meshes.clear();
    outScene.Meshes.reserve(model.meshes.size());

    for (const tinygltf::Mesh& srcMesh : model.meshes)
    {
        GltfMeshData dstMesh;
        dstMesh.Name = srcMesh.name;

        for (const tinygltf::Primitive& primitive : srcMesh.primitives)
        {
            GltfPrimitiveData dstPrimitive;
            if (LoadPrimitive(model, primitive, dstPrimitive))
            {
                dstMesh.Primitives.push_back(std::move(dstPrimitive));
            }
        }

        outScene.Meshes.push_back(std::move(dstMesh));
    }
}

static XMFLOAT4X4 GetNodeLocalMatrix(const tinygltf::Node& node)
{
    XMMATRIX M = XMMatrixIdentity();

    if (node.matrix.size() == 16)
    {
        M = XMMATRIX(
            (float)node.matrix[0], (float)node.matrix[1], (float)node.matrix[2], (float)node.matrix[3],
            (float)node.matrix[4], (float)node.matrix[5], (float)node.matrix[6], (float)node.matrix[7],
            (float)node.matrix[8], (float)node.matrix[9], (float)node.matrix[10], (float)node.matrix[11],
            (float)node.matrix[12], (float)node.matrix[13], (float)node.matrix[14], (float)node.matrix[15]
        );
    }
    else
    {
        XMVECTOR T = XMVectorZero();
        XMVECTOR R = XMQuaternionIdentity();
        XMVECTOR S = XMVectorSet(1, 1, 1, 0);

        if (node.translation.size() == 3)
            T = XMVectorSet((float)node.translation[0], (float)node.translation[1], (float)node.translation[2], 1.0f);

        if (node.rotation.size() == 4)
            R = XMVectorSet((float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3]);

        if (node.scale.size() == 3)
            S = XMVectorSet((float)node.scale[0], (float)node.scale[1], (float)node.scale[2], 0.0f);

        M = XMMatrixScalingFromVector(S) *
            XMMatrixRotationQuaternion(R) *
            XMMatrixTranslationFromVector(T);
    }

    XMFLOAT4X4 result;
    XMStoreFloat4x4(&result, M);
    return result;
}

static void LoadNodes(
    const tinygltf::Model& model,
    LoadedGltfScene& outScene)
{
    outScene.Nodes.clear();
    outScene.Nodes.reserve(model.nodes.size());

    for (const tinygltf::Node& srcNode : model.nodes)
    {
        GltfNodeData dstNode;
        dstNode.Name = srcNode.name;
        dstNode.MeshIndex = srcNode.mesh;
        dstNode.LocalMatrix = GetNodeLocalMatrix(srcNode);
        dstNode.WorldMatrix = dstNode.LocalMatrix;

        for (int child : srcNode.children)
            dstNode.Children.push_back(child);

        outScene.Nodes.push_back(dstNode);
    }
}

static void ComputeWorldRecursive(
    LoadedGltfScene& scene,
    int nodeIndex,
    const XMMATRIX& parentMatrix)
{
    GltfNodeData& node = scene.Nodes[nodeIndex];

    XMMATRIX local = XMLoadFloat4x4(&node.LocalMatrix);
    XMMATRIX world = local * parentMatrix;
    XMStoreFloat4x4(&node.WorldMatrix, world);

    for (int child : node.Children)
        ComputeWorldRecursive(scene, child, world);
}

static void DebugPrint(const std::string& s)
{
    OutputDebugStringA(s.c_str());
}

static void DebugPrintW(const std::wstring& s)
{
    OutputDebugStringW(s.c_str());
}

bool LoadGltfScene(const std::wstring& filePath, LoadedGltfScene& outScene)
{
    outScene = {};

    DebugPrint("=== LoadGltfScene BEGIN ===\n");
    DebugPrintW(L"Requested path: ");
    DebugPrintW(filePath);
    DebugPrintW(L"\n");

    std::filesystem::path path(filePath);

    DebugPrintW(L"Absolute path: ");
    DebugPrintW(std::filesystem::absolute(path).wstring());
    DebugPrintW(L"\n");

    if (!std::filesystem::exists(path))
    {
        DebugPrint("GLTF ERROR: file does not exist\n");
        return false;
    }

    if (!std::filesystem::is_regular_file(path))
    {
        DebugPrint("GLTF ERROR: path is not a regular file\n");
        return false;
    }

    std::string utf8Path = path.string();

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;

    bool ok = false;

    std::string narrowPath = path.string();

    DebugPrint("Trying to load as: ");
    DebugPrint(narrowPath);
    DebugPrint("\n");

    ok = loader.LoadASCIIFromFile(&model, &err, &warn, narrowPath);

    if (!warn.empty())
    {
        DebugPrint("tinygltf WARN:\n");
        DebugPrint(warn);
        DebugPrint("\n");
    }

    if (!err.empty())
    {
        DebugPrint("tinygltf ERROR:\n");
        DebugPrint(err);
        DebugPrint("\n");
    }

    DebugPrint(ok ? "tinygltf returned: SUCCESS\n" : "tinygltf returned: FAIL\n");

    if (!ok)
    {
        DebugPrint("=== LoadGltfScene FAILED AT FILE PARSE ===\n");
        return false;
    }

    LoadTextures(model, filePath, outScene);
    LoadMaterials(model, outScene);
    LoadMeshes(model, outScene);
    LoadNodes(model, outScene);

    if (model.defaultScene >= 0 && model.defaultScene < (int)model.scenes.size())
    {
        const tinygltf::Scene& scene = model.scenes[model.defaultScene];

        for (int rootNode : scene.nodes)
        {
            ComputeWorldRecursive(outScene, rootNode, XMMatrixIdentity());
            outScene.SceneRoot = rootNode;
        }
    }
    else
    {
        for (size_t i = 0; i < outScene.Nodes.size(); ++i)
            ComputeWorldRecursive(outScene, (int)i, XMMatrixIdentity());
    }

    return true;
}