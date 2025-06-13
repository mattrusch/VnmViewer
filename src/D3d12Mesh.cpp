// D3d12Mesh.cpp

#include "D3d12Mesh.h"
#include "D3d12Context.h"
#include <cassert>

void InitMeshesFromGltf(const GltfModel& gltfInstancedModel, D3dContext& context, D3dMesh* destMeshes, size_t maxDestMeshCount)
{
    size_t numMeshes = gltfInstancedModel.meshes.size();
    assert(numMeshes <= maxDestMeshCount);

    for (size_t iMesh = 0; iMesh < numMeshes; ++iMesh)
    {
        // Create vertex buffer
        const uint8_t* triangleVerts = gltfInstancedModel.meshes[iMesh].vertices;
        const size_t vertexBufferSize = gltfInstancedModel.meshes[iMesh].verticesSize;

        CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
        D3D_CHECK(context.mDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &buffer,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&destMeshes[iMesh].mVertexBuffer)));

        // Copy triangle data to vertex buffer
        UINT8* pVertexData;
        CD3DX12_RANGE readRangeVb(0, 0);
        D3D_CHECK(destMeshes[iMesh].mVertexBuffer->Map(0, &readRangeVb, reinterpret_cast<void**>(&pVertexData)));
        memcpy(pVertexData, triangleVerts, vertexBufferSize);
        destMeshes[iMesh].mVertexBuffer->Unmap(0, nullptr);

        // Initialize VB view
        destMeshes[iMesh].mVertexBufferView.BufferLocation = destMeshes[iMesh].mVertexBuffer->GetGPUVirtualAddress();
        destMeshes[iMesh].mVertexBufferView.StrideInBytes = static_cast<UINT>(gltfInstancedModel.meshes[iMesh].vertexStride);
        destMeshes[iMesh].mVertexBufferView.SizeInBytes = static_cast<UINT>(vertexBufferSize);

        // Create index buffer
        const uint8_t* indices = gltfInstancedModel.meshes[iMesh].indices;
        const size_t indexBufferSize = gltfInstancedModel.meshes[iMesh].indicesSize;

        CD3DX12_HEAP_PROPERTIES ibHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
        D3D_CHECK(context.mDevice->CreateCommittedResource(
            &ibHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&destMeshes[iMesh].mIndexBuffer)));

        // Copy data into index buffer
        UINT8* pIndexData;
        CD3DX12_RANGE readRangeIb(0, 0);
        D3D_CHECK(destMeshes[iMesh].mIndexBuffer->Map(0, &readRangeIb, reinterpret_cast<void**>(&pIndexData)));
        memcpy(pIndexData, indices, indexBufferSize);
        destMeshes[iMesh].mIndexBuffer->Unmap(0, nullptr);

        // Initialize IB view
        destMeshes[iMesh].mIndexBufferView.BufferLocation = destMeshes[iMesh].mIndexBuffer->GetGPUVirtualAddress();
        destMeshes[iMesh].mIndexBufferView.SizeInBytes = static_cast<UINT>(indexBufferSize);
        destMeshes[iMesh].mNumIndices = gltfInstancedModel.meshes[iMesh].numIndices;

        size_t indexSize = gltfInstancedModel.meshes[iMesh].indicesSize / gltfInstancedModel.meshes[iMesh].numIndices;
        switch (indexSize)
        {
        case 2:
            destMeshes[iMesh].mIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
            break;
        case 4:
            destMeshes[iMesh].mIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
            break;
        default:
            destMeshes[iMesh].mIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
            assert(0);
            break;
        }
    }
}

static void InterleaveArrays(size_t numArrays, uint8_t** srcArrays, size_t* attributeByteSize, size_t numAttributes, uint8_t* dest, size_t* outStride)
{
    size_t stride = 0;
    for (uint32_t i = 0; i < numArrays; i++)
    {
        stride += attributeByteSize[i];
    }

    size_t totalSize = stride * numAttributes;
    uint8_t* scratchBuf = new uint8_t[totalSize]; // TODO: Find a way around heap allocation
    uint8_t* curScratch = scratchBuf;

    for (uint32_t iAttrib = 0; iAttrib < numAttributes; iAttrib++)
    {
        for (uint32_t iArray = 0; iArray < numArrays; iArray++)
        {
            memcpy(curScratch, srcArrays[iArray] + iAttrib * attributeByteSize[iArray], attributeByteSize[iArray]);
            curScratch += attributeByteSize[iArray];
        }
    }

    memcpy(dest, scratchBuf, totalSize);

    delete[] scratchBuf;

    if (outStride != nullptr)
    {
        *outStride = stride;
    }
}

// Interleaves vertex data in place, destructively
void LoadGltf(const char* filename, GltfModel* dstModel)
{
    tinygltf::Model& model = dstModel->model;
    tinygltf::TinyGLTF loader;
    std::string error;
    std::string warning;

    loader.LoadBinaryFromFile(&model, &error, &warning, filename);

    size_t numMeshes = model.meshes.size();
    for (size_t i = 0; i < numMeshes; ++i)
    {
        auto& mesh = model.meshes[i];

        size_t numPrimitives = mesh.primitives.size();
        for (size_t iPrimitive = 0; iPrimitive < numPrimitives; ++iPrimitive)
        {
            auto& primitive = mesh.primitives[iPrimitive];

            int posAccessorIndex = -1;
            int normalAccessorIndex = -1;
            int tangentAccessorIndex = -1;
            int texcoordAccessorIndex = -1;
            for (const auto& attribute : primitive.attributes)
            {
                if (attribute.first == "POSITION")
                {
                    posAccessorIndex = attribute.second;
                }
                else if (attribute.first == "NORMAL")
                {
                    normalAccessorIndex = attribute.second;
                }
                else if (attribute.first == "TANGENT")
                {
                    tangentAccessorIndex = attribute.second;
                }
                else if (attribute.first == "TEXCOORD_0")
                {
                    texcoordAccessorIndex = attribute.second;
                }
            }

            assert(posAccessorIndex > -1);
            auto& posAccessor = model.accessors[posAccessorIndex];
            size_t vertexOffset = posAccessor.byteOffset;
            auto& posBufferView = model.bufferViews[posAccessor.bufferView];
            uint8_t* gltfVertices = model.buffers[posBufferView.buffer].data.data() + vertexOffset + posBufferView.byteOffset;

            assert(normalAccessorIndex > -1);
            auto& normalAccessor = model.accessors[normalAccessorIndex];
            size_t vertexNormalOffset = normalAccessor.byteOffset;
            auto& normalBufferView = model.bufferViews[normalAccessor.bufferView];
            uint8_t* gltfNormals = model.buffers[normalBufferView.buffer].data.data() + vertexNormalOffset + normalBufferView.byteOffset;

            assert(tangentAccessorIndex > -1);
            auto& tangentAccessor = model.accessors[tangentAccessorIndex];
            size_t vertexTangentOffset = tangentAccessor.byteOffset;
            auto& tangentBufferView = model.bufferViews[tangentAccessor.bufferView];
            uint8_t* gltfTangents = model.buffers[tangentBufferView.buffer].data.data() + vertexTangentOffset + tangentBufferView.byteOffset;

            assert(texcoordAccessorIndex > -1);
            auto& texcoordAccessor = model.accessors[texcoordAccessorIndex];
            size_t vertexTexcoordOffset = texcoordAccessor.byteOffset;
            auto& texcoordBufferView = model.bufferViews[texcoordAccessor.bufferView];
            uint8_t* gltfTexcoords = model.buffers[texcoordBufferView.buffer].data.data() + vertexTexcoordOffset + texcoordBufferView.byteOffset;

            uint8_t* streams[] = { gltfVertices, gltfNormals, gltfTangents, gltfTexcoords };
            const size_t attribByteSize = sizeof(float) * 3;
            const size_t texcoordAttribByteSize = sizeof(float) * 2;
            size_t attribByteSizes[] = { attribByteSize, attribByteSize, attribByteSize, texcoordAttribByteSize };

            size_t gltfVertexStride = 0;
            InterleaveArrays(4, streams, attribByteSizes, posAccessor.count, gltfVertices, &gltfVertexStride);
            size_t gltfVerticesSize = posAccessor.count * gltfVertexStride;

            auto& indexAccessorIndex = primitive.indices;
            auto& indexAccessor = model.accessors[indexAccessorIndex];
            size_t indexOffset = indexAccessor.byteOffset;

            //assert(indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
            assert(indexAccessor.type == TINYGLTF_TYPE_SCALAR);

            auto& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            uint8_t* gltfIndices = model.buffers[indexBufferView.buffer].data.data() + indexOffset + indexBufferView.byteOffset;
            size_t gltfIndicesSize = indexBufferView.byteLength;
            size_t gltfIndicesCount = indexAccessor.count;

            dstModel->meshes.emplace_back();
            auto& curMesh = dstModel->meshes.back();
            curMesh.numVertices = posAccessor.count;
            curMesh.vertexStride = gltfVertexStride;
            curMesh.verticesSize = gltfVerticesSize;
            curMesh.vertices = gltfVertices;
            curMesh.numIndices = gltfIndicesCount;
            curMesh.indicesSize = gltfIndicesSize;
            curMesh.indices = gltfIndices;
        }
    }
}
