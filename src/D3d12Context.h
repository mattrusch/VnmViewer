// D3d12Context.h

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h>
#include "d3dx12.h"

class SceneConstantBuffer
{
public:
    DirectX::XMMATRIX mWorldViewProj;
};

class D3dMesh
{
public:
    Microsoft::WRL::ComPtr<ID3D12Resource>            mVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW                          mVertexBufferView;

    Microsoft::WRL::ComPtr<ID3D12Resource>            mIndexBuffer;
    D3D12_INDEX_BUFFER_VIEW                           mIndexBufferView;
    size_t                                            mNumIndices = 0;
};

class D3dContext
{
public:
    void Init(HWND hwnd);
    void Update(const DirectX::XMMATRIX& lookAt, float elapsedSeconds);
    void Render();
    void Destroy();

    static const UINT   kFrameCount = 2;
    static const size_t kConstBufferSize = 1024 * 64;

    Microsoft::WRL::ComPtr<IDXGISwapChain3>           mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Device>              mDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    mCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>        mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12RootSignature>       mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>       mPipelineState;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      mCbvSrvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource>            mConstantBuffer;
    uint8_t*                                          mpCbvDataBegin;
    SceneConstantBuffer                               mConstantBufferData;
    Microsoft::WRL::ComPtr<ID3D12Resource>            mTexture;

    Microsoft::WRL::ComPtr<ID3D12Fence>               mFence;
    HANDLE                                            mFenceEvent;
    UINT64                                            mFenceValue;
    unsigned int                                      mFrameIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource>            mRenderTargets[kFrameCount];
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      mRtvHeap;
    unsigned int                                      mRtvDescriptorSize = 0;
    CD3DX12_VIEWPORT                                  mViewport;
    CD3DX12_RECT                                      mScissorRect;

    Microsoft::WRL::ComPtr<ID3D12Resource>            mDepthStencil;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      mDsvHeap;

    static const size_t                               kMaxMeshes = 100;
    size_t                                            mNumMeshes;
    D3dMesh                                           mMesh[kMaxMeshes];

private:
    void InitDevice(HWND hwnd);
};
