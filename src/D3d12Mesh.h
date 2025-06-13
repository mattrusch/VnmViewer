// D3d12Mesh.h

#pragma once

#include <d3d12.h>
#include <wrl.h>
#include "tiny_gltf.h"

class D3dMesh
{
public:
    Microsoft::WRL::ComPtr<ID3D12Resource>            mVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW                          mVertexBufferView;

    Microsoft::WRL::ComPtr<ID3D12Resource>            mIndexBuffer;
    D3D12_INDEX_BUFFER_VIEW                           mIndexBufferView;
    size_t                                            mNumIndices = 0;
};

class GltfMesh
{
public:
    size_t numVertices;
    size_t vertexStride;
    size_t verticesSize;
    const uint8_t* vertices;
    size_t numIndices;
    size_t indicesSize;
    const uint8_t* indices;
};

class GltfModel
{
public:
    tinygltf::Model       model;
    std::vector<GltfMesh> meshes;
};

class D3dContext;
void InitMeshesFromGltf(const GltfModel& gltfInstancedModel, D3dContext& context, D3dMesh* destMeshes, size_t maxDestMeshCount);
void LoadGltf(const char* filename, GltfModel* dstModel);
