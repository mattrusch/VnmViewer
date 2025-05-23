// D3d12Context.cpp

#include "D3d12Context.h"
#include <DirectXMath.h>
#include "Window.h"
#include <cassert>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#ifdef max
#undef max
#endif//def max
#include "tiny_gltf.h"

constexpr size_t ALIGN_256(size_t in)
{
    return (in + 0xff) & ~0xff;
}

//D3dContext gContext;
constexpr int gX = 100;
constexpr int gY = 100;
constexpr int gWidth = 2560;
constexpr int gHeight = 1600;

constexpr uint32_t gTexWidth = 64;
constexpr uint32_t gTexHeight = 64;
constexpr uint32_t gTexBpp = 4;
char gTexData[gTexWidth][gTexHeight][gTexBpp];

// TODO: Move these
float scales[D3dContext::kTreePosCount];
float rotations[D3dContext::kTreePosCount];


void InitAssets(D3dContext& context);
void InitTexture(char* dst, uint32_t width, uint32_t height, uint32_t bpp);

void D3dContext::Init(HWND hwnd)
{
    InitTexture(&gTexData[0][0][0], gTexWidth, gTexHeight, gTexBpp);
    InitDevice(hwnd);
    InitAssets(*this);
}

using Microsoft::WRL::ComPtr;

inline void D3D_CHECK(HRESULT hr)
{
    assert(SUCCEEDED(hr));
}

// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
_Use_decl_annotations_
static void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
{
    ComPtr<IDXGIAdapter1> adapter;
    *ppAdapter = nullptr;

    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select the Basic Render Driver adapter.
            // If you want a software adapter, pass in "/warp" on the command line.
            continue;
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the
        // actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
    }

    *ppAdapter = adapter.Detach();
}

static void WaitForPreviousFrame(D3dContext& d3dContext)
{
    // Signal and increment fence value
    const UINT64 fence = d3dContext.mFenceValue;
    D3D_CHECK(d3dContext.mCommandQueue->Signal(d3dContext.mFence.Get(), fence));
    d3dContext.mFenceValue++;

    // Wait until the previous frame is finished
    if (d3dContext.mFence->GetCompletedValue() < fence)
    {
        D3D_CHECK(d3dContext.mFence->SetEventOnCompletion(fence, d3dContext.mFenceEvent));
        WaitForSingleObject(d3dContext.mFenceEvent, INFINITE);
    }

    d3dContext.mFrameIndex = d3dContext.mSwapChain->GetCurrentBackBufferIndex();
}

void D3dContext::InitDevice(HWND hwnd)
{
    mViewport.Width = static_cast<float>(gWidth);
    mViewport.Height = static_cast<float>(gHeight);
    mViewport.MaxDepth = 1.0f;
    mViewport.MinDepth = 0.0f;
    mViewport.TopLeftX = 0.0f;
    mViewport.TopLeftY = 0.0f;

    mScissorRect.top = 0;
    mScissorRect.left = 0;
    mScissorRect.bottom = gHeight;
    mScissorRect.right = gWidth;

    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    {
        // Enable debug layer
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    D3D_CHECK(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    bool useWarpDevice = false;
    if (useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        D3D_CHECK(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
        D3D_CHECK(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice)));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);
        D3D_CHECK(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice)));
    }

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    D3D_CHECK(mDevice->CreateCommandQueue(
        &commandQueueDesc,
        IID_PPV_ARGS(&mCommandQueue)));

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = kFrameCount;
    swapChainDesc.Width = gWidth;
    swapChainDesc.Height = gHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    D3D_CHECK(factory->CreateSwapChainForHwnd(
        mCommandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain));

    D3D_CHECK(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    D3D_CHECK(swapChain.As(&mSwapChain));
    mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps

    // RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = kFrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    D3D_CHECK(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));
    mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // DSV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    D3D_CHECK(mDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));

    // CBVSRV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 2; // TODO: Make this bigger
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    D3D_CHECK(mDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvSrvHeap)));

    // Create frame resources

    // Render targets
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (int i = 0; i < kFrameCount; ++i)
    {
        D3D_CHECK(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mRenderTargets[i])));
        mDevice->CreateRenderTargetView(mRenderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, mRtvDescriptorSize);
    }

    // Depth stencil buffer
    D3D12_CLEAR_VALUE depthOptClearValue = {};
    depthOptClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptClearValue.DepthStencil.Depth = 1.0f;
    depthOptClearValue.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, gWidth, gHeight, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    D3D_CHECK(mDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptClearValue,
        IID_PPV_ARGS(&mDepthStencil)));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(mDsvHeap->GetCPUDescriptorHandleForHeapStart());

    mDevice->CreateDepthStencilView(mDepthStencil.Get(), &dsvDesc, dsvHandle);

    D3D_CHECK(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator)));
}

 // TODO: Move gltf helpers to util file
static void InterleaveArrays(size_t numArrays, uint8_t** srcArrays, size_t* attributeByteSize, size_t numAttributes, uint8_t* dest, size_t* outStride)
{
    size_t stride = 0;
    for (uint32_t i = 0; i < numArrays; i++)
    {
        stride += attributeByteSize[i];
    }

    size_t totalSize = stride * numAttributes;
    uint8_t* scratchBuf = new uint8_t[totalSize];
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

// Interleaves vertex data in place, destructively
static void LoadGltf(const char* filename, GltfModel* dstModel)
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
            const size_t texcoordAttribByteSize = sizeof(float) * 3;
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

static void InitAssets(D3dContext& context)
{
    // Create root signature
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(context.mDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
    //CD3DX12_ROOT_PARAMETER1 rootParameters[1];
    CD3DX12_ROOT_PARAMETER1 rootParameters[2];

    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
    rootParameters[0].InitAsDescriptorTable(2, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
    //rootParameters[1].InitAsConstants(1, 1, 0);
    rootParameters[1].InitAsConstantBufferView(1);

    CD3DX12_STATIC_SAMPLER_DESC samplers[1];
    samplers[0].Init(0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    D3D_CHECK(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
    D3D_CHECK(context.mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&context.mRootSignature)));

    // Create pipeline state
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif//_DEBUG

    D3D_CHECK(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VsMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
    D3D_CHECK(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PsMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

    // Input layout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Create pipeline state object
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = context.mRootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    D3D_CHECK(context.mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&context.mPipelineState)));

    // Create command list
    D3D_CHECK(context.mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, context.mCommandAllocator.Get(), context.mPipelineState.Get(), IID_PPV_ARGS(&context.mCommandList)));

    // Load geometry
    constexpr uint32_t numGltfModels = 2;
    constexpr uint32_t terrainModelIndex = 0;
    constexpr uint32_t treeModelIndex = 1;
    GltfModel gltfInstancedModel[numGltfModels];

    LoadGltf("terrain.glb", &gltfInstancedModel[terrainModelIndex]);
    assert(gltfInstancedModel[terrainModelIndex].meshes.size() < D3dContext::kMaxMeshes && "Increase D3dContext::kMaxMeshes");
    context.mNumTerrainMeshes = gltfInstancedModel[terrainModelIndex].meshes.size();
    InitMeshesFromGltf(gltfInstancedModel[terrainModelIndex], context, context.mTerrainMesh, context.kMaxMeshes);

    // TODO: Make better
    // Extract tree positions
    srand(time(NULL));
    for (int i = 0; i < D3dContext::kTreePosCount; ++i)
    {
        int index = ((float)rand() / (float)RAND_MAX) * (float)gltfInstancedModel[terrainModelIndex].meshes[0].numVertices;// context.kTreePosCount;
        size_t vertexStride = gltfInstancedModel[terrainModelIndex].meshes[0].vertexStride;
        const float* position = (const float*)(gltfInstancedModel[terrainModelIndex].meshes[0].vertices + index * vertexStride);
        context.mTreePosArray[i] = DirectX::XMVectorSet(position[0], position[1], position[2], 1.0f);
    }

    // TODO: Make better
    // Fill in random scale and rotation arrays
    for (int i = 0; i < D3dContext::kTreePosCount; ++i)
    {
        scales[i] = (float)rand() / (float)RAND_MAX;
        rotations[i]  = (float)rand() / (float)RAND_MAX;
    }

    //LoadGltf("sapling_with_texcoords_and_leaves.glb", &gltfInstancedModel[treeModelIndex]);
    LoadGltf("white_oak.glb", &gltfInstancedModel[treeModelIndex]);
    assert(gltfInstancedModel[treeModelIndex].meshes.size() < D3dContext::kMaxMeshes && "Increase D3dContext::kMaxMeshes");
    context.mNumTreeMeshes = gltfInstancedModel[treeModelIndex].meshes.size();
    InitMeshesFromGltf(gltfInstancedModel[treeModelIndex], context, context.mTreeMesh, context.kMaxMeshes);

    // Create the constant buffer
    CD3DX12_HEAP_PROPERTIES cbHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC cbResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(D3dContext::kConstBufferSize);
    D3D_CHECK(context.mDevice->CreateCommittedResource(
        &cbHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &cbResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&context.mConstantBuffer)));

    // Create constant buffer view
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = context.mConstantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = (UINT)ALIGN_256(sizeof(SceneConstantBuffer));
    context.mDevice->CreateConstantBufferView(&cbvDesc, context.mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart());

    // Map and initialize constant buffer
    CD3DX12_RANGE readRangeCb(0, 0);
    D3D_CHECK(context.mConstantBuffer->Map(0, &readRangeCb, reinterpret_cast<void**>(&context.mpCbvDataBegin)));
    memcpy(context.mpCbvDataBegin, &context.mConstantBufferData, sizeof(context.mConstantBufferData));

    // Create texture
    CD3DX12_HEAP_PROPERTIES texHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    const auto texResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, gTexWidth, gTexHeight, 1, 1);
    D3D_CHECK(context.mDevice->CreateCommittedResource(
        &texHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &texResourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&context.mTexture)));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(context.mTexture.Get(), 0, 1);

    // Create GPU upload buffer
    CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ComPtr<ID3D12Resource> textureUploadHeap;
    D3D_CHECK(context.mDevice->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&textureUploadHeap)));

    // Copy data to the upload heap and schedule a copy from the upload heap to the texture
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = &gTexData[0][0][0];
    textureData.RowPitch = gTexWidth * gTexBpp;
    textureData.SlicePitch = textureData.RowPitch * gTexHeight;

    UpdateSubresources(context.mCommandList.Get(), context.mTexture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);

    CD3DX12_RESOURCE_BARRIER resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        context.mTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    context.mCommandList->ResourceBarrier(1, &resourceBarrier);

    // Create SRV for the texture
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = texResourceDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    context.mDevice->CreateShaderResourceView(
        context.mTexture.Get(),
        &srvDesc,
        CD3DX12_CPU_DESCRIPTOR_HANDLE(context.mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), 1, context.mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));

    // Close command list and execute to begin initial GPU setup
    D3D_CHECK(context.mCommandList->Close());
    ID3D12CommandList* ppCommandLists[] = { context.mCommandList.Get() };
    context.mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Create synchroniztion objects
    D3D_CHECK(context.mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&context.mFence)));
    context.mFenceValue = 1;
    context.mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (context.mFenceEvent == nullptr)
    {
        D3D_CHECK(HRESULT_FROM_WIN32(GetLastError()));
    }

    WaitForPreviousFrame(context);
}

void D3dContext::Update(const DirectX::XMMATRIX& lookAt, float elapsedSeconds)
{
    static float totalRotation = 0.0f;
    //totalRotation += elapsedSeconds * 0.5f;

    DirectX::XMMATRIX matRotation = DirectX::XMMatrixRotationY(totalRotation);
    DirectX::XMMATRIX matLookAt = lookAt;
    DirectX::XMMATRIX matPerspective = DirectX::XMMatrixPerspectiveFovLH(1.0f, static_cast<float>(gWidth) / static_cast<float>(gHeight), 0.1f, 100.0f);

    // CB for terrain
    DirectX::XMMATRIX worldViewProj = matRotation * matLookAt * matPerspective;
    memcpy(mpCbvDataBegin, &worldViewProj, sizeof(worldViewProj));

    // CBs for Tree instances
    for (int i = 1; i < kTreePosCount; ++i)
    {
        size_t offset = i * ALIGN_256(sizeof(worldViewProj));
        float scaleFactor = scales[i] + 1.0;
        float scale = 0.0015f * scaleFactor;
        float rotation = 0.0f;// rotations[i] * DirectX::XM_2PI;
        worldViewProj = DirectX::XMMatrixRotationY(rotation) * DirectX::XMMatrixScaling(scale , scale, scale) * matRotation * DirectX::XMMatrixTranslationFromVector(mTreePosArray[i]) * matLookAt * matPerspective;
        memcpy(mpCbvDataBegin + offset, &worldViewProj, sizeof(worldViewProj));
    }
}

static void PopulateCommandList(D3dContext& context)
{
    // Command list allocators can only be reset when the associated command lists have finished execution on the GPU; use fences to determine GPU execution progress
    D3D_CHECK(context.mCommandAllocator->Reset());

    // When ExecuteCommandList() is called on a particular command list, that command list can then be reset at any time and must be before re-recording
    D3D_CHECK(context.mCommandList->Reset(context.mCommandAllocator.Get(), context.mPipelineState.Get()));

    // Set necessary state
    context.mCommandList->SetGraphicsRootSignature(context.mRootSignature.Get());

    // Set descriptor heaps
    ID3D12DescriptorHeap* ppHeaps[] = { context.mCbvSrvHeap.Get() };
    context.mCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Set root descriptor table
    context.mCommandList->SetGraphicsRootDescriptorTable(0, context.mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart());

    context.mCommandList->RSSetViewports(1, &context.mViewport);
    context.mCommandList->RSSetScissorRects(1, &context.mScissorRect);

    // Indicate that the back buffer will be used as a render target
    CD3DX12_RESOURCE_BARRIER rtResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        context.mRenderTargets[context.mFrameIndex].Get(), 
        D3D12_RESOURCE_STATE_PRESENT, 
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    context.mCommandList->ResourceBarrier(1, &rtResourceBarrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(context.mRtvHeap->GetCPUDescriptorHandleForHeapStart(), context.mFrameIndex, context.mRtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(context.mDsvHeap->GetCPUDescriptorHandleForHeapStart());
    context.mCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Record commands
    const float clearColor[] = { 0.65f, 0.65f, 0.85f, 1.0f };
    context.mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    context.mCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    context.mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (size_t i = 0; i < context.mNumTerrainMeshes; ++i)
    {
        context.mCommandList->IASetVertexBuffers(0, 1, &context.mTerrainMesh[i].mVertexBufferView);
        context.mCommandList->IASetIndexBuffer(&context.mTerrainMesh[i].mIndexBufferView);

        // Set root command buffer view for first instance
        context.mCommandList->SetGraphicsRootConstantBufferView(1, context.mConstantBuffer->GetGPUVirtualAddress());
        context.mCommandList->DrawIndexedInstanced(context.mTerrainMesh[i].mNumIndices, 1, 0, 0, 0);
    }

    for (size_t i = 0; i < context.mNumTreeMeshes; ++i)
    {
        context.mCommandList->IASetVertexBuffers(0, 1, &context.mTreeMesh[i].mVertexBufferView);
        context.mCommandList->IASetIndexBuffer(&context.mTreeMesh[i].mIndexBufferView);

        for (int iInstance = 1; iInstance < context.kTreePosCount; ++iInstance)
        {
            // Set root command buffer view for first instance
            context.mCommandList->SetGraphicsRootConstantBufferView(1, context.mConstantBuffer->GetGPUVirtualAddress() + ALIGN_256(sizeof(SceneConstantBuffer)) * iInstance);
            context.mCommandList->DrawIndexedInstanced(context.mTreeMesh[i].mNumIndices, 1, 0, 0, 0);
        }
    }

    CD3DX12_RESOURCE_BARRIER presentResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(context.mRenderTargets[context.mFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    // Indicate that the back buffer will now be used to present
    context.mCommandList->ResourceBarrier(1, &presentResourceBarrier);

    D3D_CHECK(context.mCommandList->Close());
}

void D3dContext::Render()
{
    PopulateCommandList(*this);

    ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    D3D_CHECK(mSwapChain->Present(1, 0));

    WaitForPreviousFrame(*this);
}

void D3dContext::Destroy()
{
    WaitForPreviousFrame(*this);

    CloseHandle(mFenceEvent);
}

static void InitTexture(char* dst, uint32_t width, uint32_t height, uint32_t bpp)
{
    uint8_t range = 0x40;

    for (uint32_t j = 0; j < height; ++j)
    {
        for (uint32_t i = 0; i < width; ++i)
        {
            dst[j * (width * bpp) + (i * bpp) + 0] = i % 16 < 8 && j % 8 < 4 ? 0xff : (abs(rand()) % range) + (0xff - range);
            dst[j * (width * bpp) + (i * bpp) + 1] = i % 16 < 8 && j % 8 < 4 ? 0xff : (abs(rand()) % range) + (0xff - range);
            dst[j * (width * bpp) + (i * bpp) + 2] = i % 16 < 8 && j % 8 < 4 ? 0xff : (abs(rand()) % range) + (0xff - range);
            dst[j * (width * bpp) + (i * bpp) + 3] = 0xffu;
        }
    }
}
