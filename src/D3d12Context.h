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
#include "D3d12Mesh.h"

// TODO: Move this out of context
class SceneConstantBuffer
{
public:
    DirectX::XMMATRIX mWorldViewProj;
};

inline void D3D_CHECK(HRESULT hr)
{
    assert(SUCCEEDED(hr));
}

class D3dContext
{
public:
    void Init(HWND hwnd);
    void Update(const DirectX::XMMATRIX& lookAt, float elapsedSeconds);
    void Render();
    void Destroy();

    static const UINT   kFrameCount = 2;
    static const size_t kConstBufferSize = 4096 * 256;
    static const size_t kTreePosCount = 2048;

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

    static const size_t kMaxTextures = 100;
    Microsoft::WRL::ComPtr<ID3D12Resource>            mTexture[kMaxTextures];

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

    // TODO: Move this out of context
    static const size_t                               kMaxMeshes = 100;
    size_t                                            mNumTerrainMeshes;
    D3dMesh                                           mTerrainMesh[kMaxMeshes];
    size_t                                            mNumTreeMeshes;
    D3dMesh                                           mTreeMesh[kMaxMeshes];
    size_t                                            mNumConiferMeshes;
    D3dMesh                                           mConiferMesh[kMaxMeshes];
    DirectX::XMVECTOR                                 mTreePosArray[kTreePosCount];

private:
    void InitDevice(HWND hwnd);
};
