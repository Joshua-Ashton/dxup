#include "d3d9_device.h"
#include "d3d9_interface.h"
#include "d3d9_swapchain.h"
#include "d3d9_surface.h"
#include "d3d9_texture.h"
#include "d3d9_buffer.h"
#include "d3d9_shaders.h"
#include "../dx9asm/dx9asm_translator.h"
#include "../dx9asm/dx9asm_util.h"
#include "../util/config.h"
#include "../util/d3dcompiler_helpers.h"
#include "d3d9_vertexdeclaration.h"

namespace dxapex {

  namespace dirtyFlags {
    const uint32_t vertexShader = 1 << 0;
    const uint32_t vertexDecl = 1 << 1;
    const uint32_t pixelShader = 1 << 2;
    const uint32_t renderTargets = 1 << 3;
    const uint32_t depthStencil = 1 << 4;
  }

  struct InternalRenderState{
    std::array<Com<IDirect3DBaseTexture9>, 20> textures;
    Com<Direct3DVertexShader9> vertexShader;
    Com<Direct3DPixelShader9> pixelShader;
    Com<Direct3DVertexDeclaration9> vertexDecl;
    Com<Direct3DSurface9> depthStencil;
    std::array<Com<Direct3DSurface9>, 4> renderTargets;
    uint32_t dirtyFlags = 0;
  };

  Direct3DDevice9Ex::Direct3DDevice9Ex(
    UINT adapterNum,
    IDXGIAdapter1* adapter,
    HWND window,
    ID3D11Device1* device,
    ID3D11DeviceContext1* context,
    Direct3D9Ex* parent,
    D3DDEVTYPE deviceType,
    DWORD behaviourFlags,
    uint8_t flags
  )
    : m_adapterNum{ adapterNum }
    , m_adapter(adapter)
    , m_window{ window }
    , m_device(device)
    , m_context(context)
    , m_parent(parent)
    , m_behaviourFlags{ behaviourFlags }
    , m_flags(flags)
    , m_deviceType(deviceType)
    , m_state{ new InternalRenderState }
    , m_constants{ device, context } { }

  HRESULT Direct3DDevice9Ex::CreateD3D11Device(UINT adapter, Direct3D9Ex* parent, ID3D11Device1** device, ID3D11DeviceContext1** context, IDXGIDevice1** dxgiDevice, IDXGIAdapter1** dxgiAdapter) {
    HRESULT result = parent->GetDXGIFactory()->EnumAdapters1(adapter, dxgiAdapter);

    UINT Flags = D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT | D3D11_CREATE_DEVICE_BGRA_SUPPORT; // Why isn't this a default?! ~ Josh

    if (config::getBool(config::Debug))
      Flags |= D3D11_CREATE_DEVICE_DEBUG;

    D3D_FEATURE_LEVEL FeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL Level = D3D_FEATURE_LEVEL_11_1;

    Com<ID3D11Device> initialDevice;
    Com<ID3D11DeviceContext> initialContext;

    result = D3D11CreateDevice(
      *dxgiAdapter,
      D3D_DRIVER_TYPE_UNKNOWN,
      nullptr,
      Flags,
      FeatureLevels,
      ARRAYSIZE(FeatureLevels),
      D3D11_SDK_VERSION,
      &initialDevice,
      &Level,
      &initialContext
    );

    if (FAILED(result)) {
      log::fail("Unable to create d3d11 device.");
      return D3DERR_DEVICELOST;
    }

    result = initialDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)device);
    HRESULT contextResult = initialContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)context);

    if (FAILED(result) || FAILED(contextResult)) {
      log::fail("Unable to upgrade to d3d11_1.");
      return D3DERR_DEVICELOST;
    }

    result = initialDevice->QueryInterface(__uuidof(IDXGIDevice1), (void**)dxgiDevice);

    if (FAILED(result)) {
      log::fail("Couldn't get IDXGIDevice1!");
      return D3DERR_INVALIDCALL;
    }

    return D3D_OK;
  }

  HRESULT Direct3DDevice9Ex::Create(
    UINT adapter,
    HWND window,
    Direct3D9Ex* parent,
    D3DPRESENT_PARAMETERS* presentParameters,
    D3DDEVTYPE deviceType,
    bool isEx,
    DWORD behaviourFlags,
    IDirect3DDevice9Ex** outDevice
    ) {
    InitReturnPtr(outDevice);

    if (!outDevice || !presentParameters)
      return D3DERR_INVALIDCALL;

    Com<ID3D11Device1> device;
    Com<ID3D11DeviceContext1> context;
    Com<IDXGIAdapter1> dxgiAdapter;
    Com<IDXGIDevice1> dxgiDevice;

    HRESULT result = CreateD3D11Device(adapter, parent, &device, &context, &dxgiDevice, &dxgiAdapter);
    SetupD3D11Debug(device.ptr());

    if (FAILED(result))
      return D3DERR_DEVICELOST;

    uint8_t flags = 0;

    if (isEx)
      flags |= DeviceFlag_Ex;

    RECT rect;
    GetWindowRect(window, &rect);

    if (!presentParameters->BackBufferWidth)
      presentParameters->BackBufferWidth = rect.right;

    if (!presentParameters->BackBufferHeight)
      presentParameters->BackBufferHeight = rect.bottom;

    if (!presentParameters->BackBufferCount)
      presentParameters->BackBufferCount = 1;

    if (presentParameters->BackBufferFormat == D3DFMT_UNKNOWN)
      presentParameters->BackBufferFormat = D3DFMT_A8B8G8R8;

    Direct3DDevice9Ex* d3d9Device = new Direct3DDevice9Ex(
      adapter,
      dxgiAdapter.ptr(),
      window,
      device.ptr(),
      context.ptr(),
      parent,
      deviceType,
      behaviourFlags,
      flags);

    result = d3d9Device->Reset(presentParameters);

    if (FAILED(result)) {
      log::fail("Failed to create d3d9 device as Reset to default state failed.");
      delete d3d9Device;
      return D3DERR_INVALIDCALL;
    }

    *outDevice = ref(d3d9Device);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    if (pPresentationParameters == nullptr)
      return D3DERR_INVALIDCALL;

    // Unbind current state...

    SetVertexShader(nullptr);
    SetPixelShader(nullptr);
    SetDepthStencilSurface(nullptr);

    for (uint32_t i = 0; i < 4; i++)
      SetRenderTarget(0, nullptr);

    // Setup new state...

    HRESULT result = D3D_OK;

    if (GetInternalSwapchain(0) == nullptr) {
      result = CreateAdditionalSwapChain(pPresentationParameters, (IDirect3DSwapChain9**)&m_swapchains[0]);

      if (FAILED(result)) {
        log::fail("Couldn't create implicit swapchain.");
        return D3DERR_INVALIDCALL;
      }
    }
    else {
      result = GetInternalSwapchain(0)->Reset(pPresentationParameters);

      if (FAILED(result))
        return D3DERR_INVALIDCALL;
    }

    Com<IDirect3DSurface9> backbuffer;
    result = m_swapchains[0]->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);

    if (FAILED(result)) {
      log::fail("Couldn't get implicit backbuffer.");
      return D3DERR_INVALIDCALL;
    }

    SetRenderTarget(0, backbuffer.ptr());

    D3DVIEWPORT9 implicitViewport;
    implicitViewport.X = 0;
    implicitViewport.Y = 0;
    implicitViewport.Height = pPresentationParameters->BackBufferHeight;
    implicitViewport.Width = pPresentationParameters->BackBufferWidth;
    implicitViewport.MinZ = 0.0f;
    implicitViewport.MaxZ = 1.0f;
    SetViewport(&implicitViewport);

    for (uint32_t i = 0; i < 4; i++) {
      ID3D11SamplerState* state;
      D3D11_SAMPLER_DESC sampDesc;
      ZeroMemory(&sampDesc, sizeof(sampDesc));
      sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
      sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
      sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
      sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
      sampDesc.MinLOD = 0;
      sampDesc.MaxLOD = 0;

      m_device->CreateSamplerState(&sampDesc, &state);
      m_context->PSSetSamplers(i, 1, &state);
    }

    for (uint32_t i = 0; i < 6; i++) {
      float plane[4] = { 0, 0, 0, 0 };
      SetClipPlane(i, plane);
    }

    if (pPresentationParameters->EnableAutoDepthStencil) {
      Com<IDirect3DSurface9> autoDepthStencil;
      CreateDepthStencilSurface(pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight, pPresentationParameters->AutoDepthStencilFormat, pPresentationParameters->MultiSampleType, pPresentationParameters->MultiSampleQuality, false, &autoDepthStencil, nullptr);
      SetDepthStencilSurface(autoDepthStencil.ptr());
    }

    if (config::getBool(config::InitialHideCursor))
      ShowCursor(false);

    return D3D_OK;
  }

  void Direct3DDevice9Ex::SetupD3D11Debug(ID3D11Device* device) {
    if (config::getBool(config::Debug)) {

      Com<ID3D11Debug> d3dDebug;
      if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug))) {

        Com<ID3D11InfoQueue> d3dInfoQueue;
        if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue))) {

          d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
          d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);

          std::array<D3D11_MESSAGE_ID, 1> messagesToHide = {
            D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
          };

          D3D11_INFO_QUEUE_FILTER filter;
          memset(&filter, 0, sizeof(filter));
          filter.DenyList.NumIDs = messagesToHide.size();
          filter.DenyList.pIDList = &messagesToHide[0];
          d3dInfoQueue->AddStorageFilterEntries(&filter);
        }
      }

    }
  }

  Direct3DDevice9Ex::~Direct3DDevice9Ex() {
    delete m_state;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::QueryInterface(REFIID riid, LPVOID* ppv) {
    InitReturnPtr(ppv);

    if (ppv == nullptr)
      return E_POINTER;

    if (riid == __uuidof(IDirect3DDevice9Ex) || riid == __uuidof(IDirect3DDevice9) || riid == __uuidof(IUnknown))
      *ppv = ref(this);

    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::TestCooperativeLevel() {
    if (m_flags & DeviceFlag_Ex)
      return D3D_OK;

    Direct3DSwapChain9Ex* swapchain = GetInternalSwapchain(0);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->TestSwapchain(nullptr, 0);
  }

  UINT    STDMETHODCALLTYPE Direct3DDevice9Ex::GetAvailableTextureMem() {
    DXGI_ADAPTER_DESC adapterDesc; 
    m_adapter->GetDesc(&adapterDesc);
    return UINT(adapterDesc.DedicatedVideoMemory / 1024ul);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::EvictManagedResources() {
    log::stub("Direct3DDevice9Ex::EvictManagedResources");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDirect3D(IDirect3D9** ppD3D9) {
    if (!ppD3D9)
      return D3DERR_INVALIDCALL;
    *ppD3D9 = ref(m_parent);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDeviceCaps(D3DCAPS9* pCaps) {
    return m_parent->GetDeviceCaps(0, D3DDEVTYPE_HAL, pCaps);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) {
    log::stub("Direct3DDevice9Ex::GetDisplayMode");

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) {
    if (!pParameters)
      return D3DERR_INVALIDCALL;

    pParameters->AdapterOrdinal = m_adapterNum;
    pParameters->BehaviorFlags = m_behaviourFlags;
    pParameters->hFocusWindow = m_window;
    pParameters->DeviceType = m_deviceType;

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) {
    log::stub("Direct3DDevice9Ex::SetCursorProperties");
    return D3D_OK;
  }
  void    STDMETHODCALLTYPE Direct3DDevice9Ex::SetCursorPosition(int X, int Y, DWORD Flags) {
    if (Flags & D3DCURSOR_IMMEDIATE_UPDATE) {
      ::SetCursorPos(X, Y);
      m_pendingCursorUpdate = { 0 };
      return;
    }

    m_pendingCursorUpdate.update = true;
    m_pendingCursorUpdate.x = X;
    m_pendingCursorUpdate.y = Y;
  }
  BOOL    STDMETHODCALLTYPE Direct3DDevice9Ex::ShowCursor(BOOL bShow) {

    ::ShowCursor(bShow);

    return TRUE;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** ppSwapChain) {
    InitReturnPtr(ppSwapChain);

    if (!ppSwapChain)
      return D3DERR_INVALIDCALL;

    DXGI_SWAP_CHAIN_DESC SwapChainDesc;
    memset(&SwapChainDesc, 0, sizeof(SwapChainDesc));

    UINT BackBufferCount = pPresentationParameters->BackBufferCount;
    if (BackBufferCount == 0)
      BackBufferCount = 1;

    SwapChainDesc.BufferCount = BackBufferCount;
    SwapChainDesc.BufferDesc.Width = pPresentationParameters->BackBufferWidth;
    SwapChainDesc.BufferDesc.Height = pPresentationParameters->BackBufferHeight;
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
    SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    SwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.OutputWindow = m_window;
    SwapChainDesc.Windowed = true;
    SwapChainDesc.SampleDesc.Count = (UINT)pPresentationParameters->MultiSampleType;

    if (SwapChainDesc.SampleDesc.Count == 0)
      SwapChainDesc.SampleDesc.Count = 1;

    SwapChainDesc.SampleDesc.Quality = pPresentationParameters->MultiSampleQuality;

    Com<Direct3D9Ex> parent;
    GetParent(&parent);

    Com<IDXGISwapChain> dxgiSwapChain;
    HRESULT result = parent->GetDXGIFactory()->CreateSwapChain(m_device.ptr(), &SwapChainDesc, &dxgiSwapChain);

    if (FAILED(result)) {
      log::fail("Failed to make swapchain!");
      return result;
    }

    Com<IDXGISwapChain1> upgradedSwapchain;
    result = dxgiSwapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&upgradedSwapchain);

    if (FAILED(result)) {
      log::fail("Failed to upgrade swapchain to IDXGISwapChain1!");
      return result;
    }

    parent->GetDXGIFactory()->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER);

    for (size_t i = 0; i < m_swapchains.size(); i++)
    {
      if (m_swapchains[i] == nullptr) {
        m_swapchains[i] = ref( new Direct3DSwapChain9Ex(this, pPresentationParameters, upgradedSwapchain.ptr()) );

        *ppSwapChain = ref(m_swapchains[i]);
        return D3D_OK;
      }
    }

    return D3DERR_INVALIDCALL;
  }

  Direct3DSwapChain9Ex* Direct3DDevice9Ex::GetInternalSwapchain(UINT i) {
    if (i >= m_swapchains.size())
      return nullptr;

    return reinterpret_cast<Direct3DSwapChain9Ex*>(m_swapchains[i].ptr());
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) {
    InitReturnPtr(pSwapChain);
    if (!pSwapChain || iSwapChain >= m_swapchains.size() || GetInternalSwapchain(iSwapChain) == nullptr)
      return D3DERR_INVALIDCALL;

    *pSwapChain = ref(GetInternalSwapchain(iSwapChain));

    return D3D_OK;
  }
  UINT    STDMETHODCALLTYPE Direct3DDevice9Ex::GetNumberOfSwapChains() {
    UINT swapchainCount = 0;

    for (size_t i = 0; i < m_swapchains.size(); i++) {
      if (m_swapchains[i] == nullptr)
        swapchainCount++;
    }
    return swapchainCount;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Present(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion) {
    return PresentEx(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, 0);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) {
    Direct3DSwapChain9Ex* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) {
    Direct3DSwapChain9Ex* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetRasterStatus(pRasterStatus);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetDialogBoxMode(BOOL bEnableDialogs) {
    log::stub("Direct3DDevice9Ex::SetDialogBoxMode");
    return D3D_OK;
  }
  void    STDMETHODCALLTYPE Direct3DDevice9Ex::SetGammaRamp(UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp) {
    log::stub("Direct3DDevice9Ex::SetGammaRamp");
  }
  void    STDMETHODCALLTYPE Direct3DDevice9Ex::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) {
    log::stub("Direct3DDevice9Ex::GetGammaRamp");
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) {
    return CreateTextureInternal(Width, Height, Levels, Usage, Format, Pool, D3DMULTISAMPLE_NONMASKABLE, 0, false, ppTexture, pSharedHandle);
  }

  HRESULT Direct3DDevice9Ex::CreateTextureInternal(
    UINT Width, 
    UINT Height,
    UINT Levels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool, 
    D3DMULTISAMPLE_TYPE MultiSample,
    DWORD MultisampleQuality,
    BOOL Discard,
    IDirect3DTexture9** ppTexture,
    HANDLE* pSharedHandle) {
    InitReturnPtr(ppTexture);
    InitReturnPtr(pSharedHandle);

    if (!ppTexture)
      return D3DERR_INVALIDCALL;

    D3D11_USAGE d3d11Usage = convert::usage(Pool, Usage);

    D3D11_TEXTURE2D_DESC desc;
    desc.Width = Width;
    desc.Height = Height;
    desc.Format = convert::format(Format);
    desc.Usage = d3d11Usage;
    desc.CPUAccessFlags = convert::cpuFlags(Pool, Usage);
    desc.MipLevels = d3d11Usage == D3D11_USAGE_DYNAMIC ? 1 : Levels;
    desc.ArraySize = 1;

    if (MultiSample == D3DMULTISAMPLE_NONE) // 0 samples -> 1 sample.
      MultiSample = D3DMULTISAMPLE_NONMASKABLE;

    desc.SampleDesc.Count = (UINT)MultiSample;
    desc.SampleDesc.Quality = MultisampleQuality;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;

    bool isDepthStencil = Usage & D3DUSAGE_DEPTHSTENCIL;

    if (d3d11Usage == D3D11_USAGE_DEFAULT) {
      if (!isDepthStencil)
        desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

      desc.BindFlags |= Usage & D3DUSAGE_RENDERTARGET ? D3D11_BIND_RENDER_TARGET : 0;
      desc.BindFlags |= Usage & D3DUSAGE_DEPTHSTENCIL ? D3D11_BIND_DEPTH_STENCIL : 0;

      desc.MiscFlags |= Usage & D3DUSAGE_AUTOGENMIPMAP ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
    }

    Com<ID3D11Texture2D> texture;
    HRESULT result = m_device->CreateTexture2D(&desc, nullptr, &texture);

    if (FAILED(result)) {
      log::fail("Failed to create texture.");
      return D3DERR_INVALIDCALL;
    }

    Com<ID3D11ShaderResourceView> srv;
    if (!isDepthStencil)
      m_device->CreateShaderResourceView(texture.ptr(), nullptr, &srv);

    *ppTexture = ref(new Direct3DTexture9(this, texture.ptr(), srv.ptr(), Pool, Usage, Discard));

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) {
    log::stub("Direct3DDevice9Ex::CreateVolumeTexture");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) {
    log::stub("Direct3DDevice9Ex::CreateCubeTexture");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) {
    InitReturnPtr(ppVertexBuffer);
    InitReturnPtr(pSharedHandle);

    if (!ppVertexBuffer)
      return D3DERR_INVALIDCALL;

    D3D11_BUFFER_DESC desc;
    desc.ByteWidth = Length;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = convert::cpuFlags(Pool, Usage);
    desc.Usage = convert::usage(Pool, Usage);
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    Com<ID3D11Buffer> buffer;
    HRESULT result = m_device->CreateBuffer(&desc, nullptr, &buffer);
    if (FAILED(result))
      return D3DERR_INVALIDCALL;

    *ppVertexBuffer = ref(new Direct3DVertexBuffer9(this, buffer.ptr(), Pool, FVF, Usage));

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) {
    InitReturnPtr(ppIndexBuffer);
    InitReturnPtr(pSharedHandle);

    if (!ppIndexBuffer)
      return D3DERR_INVALIDCALL;

    D3D11_BUFFER_DESC desc;
    desc.ByteWidth = Length;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = convert::cpuFlags(Pool, Usage);
    desc.Usage = convert::usage(Pool, Usage);
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    Com<ID3D11Buffer> buffer;
    HRESULT result = m_device->CreateBuffer(&desc, nullptr, &buffer);
    if (FAILED(result))
      return D3DERR_INVALIDCALL;

    *ppIndexBuffer = ref(new Direct3DIndexBuffer9(this, buffer.ptr(), Pool, Format, Usage));

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    InitReturnPtr(ppSurface);
    if (ppSurface == nullptr)
      return D3DERR_INVALIDCALL;

    Com<IDirect3DTexture9> d3d9Texture;

    // NOTE(Josh): May need to handle Lockable in future.
    HRESULT result = CreateTextureInternal(Width, Height, 1, D3DUSAGE_RENDERTARGET, Format, D3DPOOL_DEFAULT, MultiSample, MultisampleQuality, false, &d3d9Texture, pSharedHandle);

    if (FAILED(result)) {
      log::fail("Failed to create render target.");
      return result;
    }

    return d3d9Texture->GetSurfaceLevel(0, ppSurface);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    InitReturnPtr(ppSurface);
    if (ppSurface == nullptr)
      return D3DERR_INVALIDCALL;

    Com<IDirect3DTexture9> d3d9Texture;
    HRESULT result = CreateTextureInternal(Width, Height, 1, D3DUSAGE_DEPTHSTENCIL, Format, D3DPOOL_DEFAULT, MultiSample, MultisampleQuality, Discard, &d3d9Texture, pSharedHandle);

    if (FAILED(result)) {
      log::fail("Failed to create depth stencil.");
      return result;
    }

    return d3d9Texture->GetSurfaceLevel(0, ppSurface);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::UpdateSurface(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint) {

    RECT destRect;
    destRect.left = pDestPoint ? pDestPoint->x : 0;
    destRect.top = pDestPoint ? pDestPoint->y : 0;

    StretchRect(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint ? &destRect : nullptr, D3DTEXF_NONE);
    
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) {
    if (pSourceTexture == nullptr || pDestinationTexture == nullptr)
      return D3DERR_INVALIDCALL;

    switch (pSourceTexture->GetType()) {

    case D3DRTYPE_TEXTURE: {

      IDirect3DTexture9* srcTex = (Direct3DTexture9*)pSourceTexture;
      IDirect3DTexture9* dstTex = (Direct3DTexture9*)pDestinationTexture;

      if (srcTex == nullptr || dstTex == nullptr)
        return D3DERR_INVALIDCALL;

      Com<IDirect3DSurface9> srcSurface;
      Com<IDirect3DSurface9> dstSurface;

      srcTex->GetSurfaceLevel(0, &srcSurface);
      dstTex->GetSurfaceLevel(0, &dstSurface);

      StretchRect(srcSurface.ptr(), NULL, dstSurface.ptr(), NULL, D3DTEXF_NONE);
      dstTex->GenerateMipSubLevels();

      return D3D_OK;
    }

    default: return D3DERR_INVALIDCALL;
    }
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) {
    StretchRect(pRenderTarget, NULL, pDestSurface, NULL, D3DTEXF_NONE);
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) {
    Direct3DSwapChain9Ex* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    swapchain->GetFrontBufferData(pDestSurface);  
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::StretchRect(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) {
    Direct3DSurface9* src = reinterpret_cast<Direct3DSurface9*>(pSourceSurface);
    Direct3DSurface9* dst = reinterpret_cast<Direct3DSurface9*>(pDestSurface);

    if (dst->GetD3D11Texture2D() == nullptr || src->GetD3D11Texture2D() == nullptr) {
      log::fail("Can't StretchRect of non-d3d11 based surface.");
      return D3DERR_INVALIDCALL;
    }

    if (pSourceRect != nullptr && pDestRect != nullptr) {
      UINT x = pDestRect->left;
      UINT y = pDestRect->top;

      // This doesn't properly handle 'stretching'.
      D3D11_BOX box;
      box.left = pSourceRect->left;
      box.right = pSourceRect->right;
      box.top = pSourceRect->top;
      box.bottom = pSourceRect->bottom;
      box.top = 0;
      box.back = 0;
      m_context->CopySubresourceRegion(dst->GetD3D11Texture2D(), dst->GetSubresource(), x, y, 0, src->GetD3D11Texture2D(), src->GetSubresource(), &box);

      return D3D_OK;
    }

    m_context->CopySubresourceRegion(dst->GetD3D11Texture2D(), dst->GetSubresource(), 0, 0, 0, src->GetD3D11Texture2D(), src->GetSubresource(), nullptr);
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ColorFill(IDirect3DSurface9* pSurface, CONST RECT* pRect, D3DCOLOR color) {
    /*if (pSurface == nullptr)
      return D3DERR_INVALIDCALL;

    D3DLOCKED_RECT lockedRect;
    HRESULT result = pSurface->LockRect(&lockedRect, pRect, 0);
    if (FAILED(result))
      return D3DERR_INVALIDCALL;

    D3DSURFACE_DESC desc;
    pSurface->GetDesc(&desc);

    UINT rows = pRect ? pRect->bottom - pRect->top : desc.Height;
    UINT width = pRect ? pRect->right - pRect->left : desc.Width;

    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < width; x++) {
        lockedRect.pBits[(y * width) + x] = 
      }
    }*/

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    log::stub("Direct3DDevice9Ex::CreateOffscreenPlainSurface");
    return D3D_OK;
  }
  void Direct3DDevice9Ex::UpdateRenderTargets() {

    std::array<ID3D11RenderTargetView*, 4> rtvs = { nullptr, nullptr, nullptr, nullptr };
    for (uint32_t i = 0; i < 4; i++)
    {
      if (m_state->renderTargets[i] != nullptr) {
        rtvs[i] = m_state->renderTargets[i]->GetD3D11RenderTarget();
        if (rtvs[i] == nullptr)
          log::warn("No render target view for bound render target surface.");
      }
    }

    ID3D11DepthStencilView* dsv = nullptr;
    if (m_state->depthStencil != nullptr) {
      dsv = m_state->depthStencil->GetD3D11DepthStencil();
      if (dsv == nullptr)
        log::warn("No depth stencil view for bound depth stencil surface.");
    }

    m_context->OMSetRenderTargets(4, &rtvs[0], dsv);

    m_state->dirtyFlags &= ~dirtyFlags::renderTargets;
    m_state->dirtyFlags &= ~dirtyFlags::depthStencil;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) {
    if (RenderTargetIndex >= 4)
      return D3DERR_INVALIDCALL;

    m_state->renderTargets[RenderTargetIndex] = reinterpret_cast<Direct3DSurface9*>(pRenderTarget);
    m_state->dirtyFlags |= dirtyFlags::renderTargets;

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) {
    InitReturnPtr(ppRenderTarget);

    if (ppRenderTarget == nullptr)
      return D3DERR_INVALIDCALL;
    
    if (RenderTargetIndex > m_state->renderTargets.size())
      return D3DERR_INVALIDCALL;

    if (m_state->renderTargets[RenderTargetIndex] == nullptr)
      return D3DERR_NOTFOUND;

    *ppRenderTarget = ref(m_state->renderTargets[RenderTargetIndex]);

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
    DoDepthDiscardCheck();

    m_state->depthStencil = reinterpret_cast<Direct3DSurface9*>(pNewZStencil);
    m_state->dirtyFlags |= dirtyFlags::depthStencil;

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    InitReturnPtr(ppZStencilSurface);

    if (ppZStencilSurface == nullptr)
      return D3DERR_INVALIDCALL;

    if (m_state->depthStencil == nullptr)
      return D3DERR_NOTFOUND;

    *ppZStencilSurface = ref(m_state->depthStencil);

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::BeginScene() {
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::EndScene() {
    m_context->Flush();
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Clear(DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) {
    FLOAT color[4];
    convert::color(Color, color);

    if (config::getBool(config::RandomClearColour)) {
      for (uint32_t i = 0; i < 4; i++)
        color[i] = ((float)(rand() % 255)) / 255.0f;
    }
    
    if (Flags & D3DCLEAR_TARGET) {
      for (uint32_t i = 0; i < 4; i++)
      {
        if (m_state->renderTargets[i] == nullptr)
          continue;

        ID3D11RenderTargetView* rtv = m_state->renderTargets[i]->GetD3D11RenderTarget();
        if (rtv)
          m_context->ClearRenderTargetView(rtv, color);
      }
    }

    ID3D11DepthStencilView* dsv = nullptr;
    if (m_state->depthStencil != nullptr)
      dsv = m_state->depthStencil->GetD3D11DepthStencil();

    if ((Flags & D3DCLEAR_STENCIL || Flags & D3DCLEAR_ZBUFFER) && dsv != nullptr) {
      uint32_t clearFlags = Flags & D3DCLEAR_STENCIL ? D3D11_CLEAR_STENCIL : 0;
      clearFlags |= Flags & D3DCLEAR_ZBUFFER ? D3D11_CLEAR_DEPTH : 0;

      m_context->ClearDepthStencilView(dsv, clearFlags, Z, Stencil);
    }

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) {
    log::stub("Direct3DDevice9Ex::SetTransform");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
    log::stub("Direct3DDevice9Ex::GetTransform");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::MultiplyTransform(D3DTRANSFORMSTATETYPE TransformState, CONST D3DMATRIX* pMatrix) {
    log::stub("Direct3DDevice9Ex::MultiplyTransform");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetViewport(CONST D3DVIEWPORT9* pViewport) {
    if (!pViewport)
      return D3DERR_INVALIDCALL;

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = (FLOAT) pViewport->X;
    viewport.TopLeftY = (FLOAT) pViewport->Y;
    viewport.MinDepth = pViewport->MinZ;
    viewport.MaxDepth = pViewport->MaxZ;
    viewport.Width = (FLOAT)pViewport->Width;
    viewport.Height = (FLOAT)pViewport->Height;
    m_context->RSSetViewports(1, &viewport);

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetViewport(D3DVIEWPORT9* pViewport) {
    if (!pViewport)
      return D3DERR_INVALIDCALL;

    D3D11_VIEWPORT viewport;
    UINT numViewports = 1;
    m_context->RSGetViewports(&numViewports, &viewport);

    pViewport->MaxZ = viewport.MaxDepth;
    pViewport->MinZ = viewport.MinDepth;
    pViewport->Width = (DWORD) viewport.Width;
    pViewport->Height = (DWORD) viewport.Height;
    pViewport->X = (DWORD) viewport.TopLeftX;
    pViewport->Y = (DWORD) viewport.TopLeftY;

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetMaterial(CONST D3DMATERIAL9* pMaterial) {
    log::stub("Direct3DDevice9Ex::SetMaterial");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetMaterial(D3DMATERIAL9* pMaterial) {
    log::stub("Direct3DDevice9Ex::GetMaterial");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetLight(DWORD Index, CONST D3DLIGHT9* pLight) {
    log::stub("Direct3DDevice9Ex::SetLight");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetLight(DWORD Index, D3DLIGHT9* pLight) {
    log::stub("Direct3DDevice9Ex::GetLight");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::LightEnable(DWORD Index, BOOL Enable) {
    log::stub("Direct3DDevice9Ex::LightEnable");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetLightEnable(DWORD Index, BOOL* pEnable) {
    log::stub("Direct3DDevice9Ex::GetLightEnable");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetClipPlane(DWORD Index, CONST float* pPlane) {
    log::stub("Direct3DDevice9Ex::SetClipPlane");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetClipPlane(DWORD Index, float* pPlane) {
    log::stub("Direct3DDevice9Ex::GetClipPlane");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    log::stub("Direct3DDevice9Ex::SetRenderState");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
    log::stub("Direct3DDevice9Ex::GetRenderState");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) {
    log::stub("Direct3DDevice9Ex::CreateStateBlock");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::BeginStateBlock() {
    log::stub("Direct3DDevice9Ex::BeginStateBlock");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::EndStateBlock(IDirect3DStateBlock9** ppSB) {
    log::stub("Direct3DDevice9Ex::EndStateBlock");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetClipStatus(CONST D3DCLIPSTATUS9* pClipStatus) {
    log::stub("Direct3DDevice9Ex::SetClipStatus");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetClipStatus(D3DCLIPSTATUS9* pClipStatus) {
    log::stub("Direct3DDevice9Ex::GetClipStatus");
    return D3D_OK;
  }

  HRESULT Direct3DDevice9Ex::MapStageToSampler(DWORD Stage, DWORD* Sampler){
    if ((Stage >= 16 && Stage <= D3DDMAPSAMPLER) || Stage > D3DVERTEXTEXTURESAMPLER3)
      return D3DERR_INVALIDCALL;

    // For vertex samplers.
    if (Stage >= D3DVERTEXTEXTURESAMPLER0)
      Stage = 16 + (Stage - D3DVERTEXTEXTURESAMPLER0);

    *Sampler = Stage;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) {
    InitReturnPtr(ppTexture);

    if (ppTexture == nullptr)
      return D3DERR_INVALIDCALL;

    if (FAILED(MapStageToSampler(Stage, &Stage)))
      return D3DERR_INVALIDCALL;

    *ppTexture = ref(m_state->textures[Stage]);

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) {
    if (FAILED(MapStageToSampler(Stage, &Stage)))
      return D3DERR_INVALIDCALL;

    if (m_state->textures[Stage] == pTexture)
      return D3D_OK;

    Direct3DTexture9* texture2D = dynamic_cast<Direct3DTexture9*>(pTexture);

    ID3D11ShaderResourceView* srv = nullptr;
    if (texture2D != nullptr)
      srv = texture2D->GetSRV();
    else
      log::warn("Request to bind a texture that I don't know how!");

    if (srv != nullptr)
      m_context->PSSetShaderResources(Stage, 1, &srv);
    else
      log::fail("Failed to bind texture, null SRV.");

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) {
    log::stub("Direct3DDevice9Ex::GetTextureStageState");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) {
    log::stub("Direct3DDevice9Ex::SetTextureStageState");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) {
    log::stub("Direct3DDevice9Ex::GetSamplerState");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) {
    log::stub("Direct3DDevice9Ex::SetSamplerState");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ValidateDevice(DWORD* pNumPasses) {
    if (!pNumPasses)
      return D3DERR_INVALIDCALL;

    *pNumPasses = 1;
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY* pEntries) {
    log::stub("Direct3DDevice9Ex::SetPaletteEntries");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) {
    log::stub("Direct3DDevice9Ex::GetPaletteEntries");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetCurrentTexturePalette(UINT PaletteNumber) {
    log::stub("Direct3DDevice9Ex::SetCurrentTexturePalette");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetCurrentTexturePalette(UINT *PaletteNumber) {
    log::stub("Direct3DDevice9Ex::GetCurrentTexturePalette");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetScissorRect(CONST RECT* pRect) {
    if (pRect == nullptr)
      return D3DERR_INVALIDCALL;

    D3D11_RECT rect;
    rect.bottom = pRect->bottom;
    rect.left = pRect->left;
    rect.right = pRect->right;
    rect.top = pRect->top;

    m_context->RSSetScissorRects(1, &rect);
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetScissorRect(RECT* pRect) {
    if (pRect == nullptr)
      return D3DERR_INVALIDCALL;

    D3D11_RECT rect;
    UINT rects = 1;
    m_context->RSGetScissorRects(&rects, &rect);

    pRect->bottom = rect.bottom;
    pRect->left = rect.left;
    pRect->right = rect.right;
    pRect->top = rect.top;

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetSoftwareVertexProcessing(BOOL bSoftware) {
    m_softwareVertexProcessing = bSoftware;
    return D3D_OK;
  }
  BOOL    STDMETHODCALLTYPE Direct3DDevice9Ex::GetSoftwareVertexProcessing() {
    return m_softwareVertexProcessing;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetNPatchMode(float nSegments) {
    log::stub("Direct3DDevice9Ex::SetNPatchMode");
    return D3D_OK;
  }
  float   STDMETHODCALLTYPE Direct3DDevice9Ex::GetNPatchMode() {
    log::stub("Direct3DDevice9Ex::GetNPatchMode");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) {
    if (!PrepareDraw()) {
      log::warn("Invalid internal render state achieved.");
      return D3D_OK; // Lies!
    }

    D3D_PRIMITIVE_TOPOLOGY topology;
    UINT drawCount = convert::primitiveData(PrimitiveType, PrimitiveCount, topology);

    m_context->IASetPrimitiveTopology(topology);
    m_context->Draw(drawCount, StartVertex);

    FinishDraw();

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) {
    if (!PrepareDraw()) {
      log::warn("Invalid internal render state achieved.");
      return D3D_OK; // Lies!
    }

    log::warn("DrawIndexedPrimitive, partial support.");

    D3D_PRIMITIVE_TOPOLOGY topology;
    UINT drawCount = convert::primitiveData(PrimitiveType, primCount, topology);

    m_context->IASetPrimitiveTopology(topology);
    m_context->DrawIndexed(drawCount, startIndex, BaseVertexIndex + MinVertexIndex);

    FinishDraw();

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    log::stub("Direct3DDevice9Ex::DrawPrimitiveUP");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    log::stub("Direct3DDevice9Ex::DrawIndexedPrimitiveUP");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) {
    log::stub("Direct3DDevice9Ex::ProcessVertices");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) {
    InitReturnPtr(ppDecl);

    if (ppDecl == nullptr || pVertexElements == nullptr)
      return D3DERR_INVALIDCALL;

    D3DVERTEXELEMENT9 lastElement = D3DDECL_END();

    std::vector<D3D11_INPUT_ELEMENT_DESC> inputElements;
    std::vector<D3DVERTEXELEMENT9> d3d9Elements;

    auto vertexElementEqual = [] (const D3DVERTEXELEMENT9& a, const D3DVERTEXELEMENT9& b) {
      return  a.Method == b.Method &&
              a.Offset == b.Offset &&
              a.Stream == b.Stream &&
              a.Type == b.Type &&
              a.Usage == b.Usage &&
              a.UsageIndex == b.UsageIndex;
    };

    size_t count;
    {
      const D3DVERTEXELEMENT9* counter = pVertexElements;
      while (!vertexElementEqual(*counter, lastElement))
        counter++;

      count = counter - pVertexElements;
    }

    d3d9Elements.resize(count);
    inputElements.reserve(count);

    std::memcpy(&d3d9Elements[0], pVertexElements, sizeof(D3DVERTEXELEMENT9) * count);

    for (size_t i = 0; i < count; i++) {
      D3D11_INPUT_ELEMENT_DESC desc;
      
      desc.SemanticName = convert::declUsage(true, false, (D3DDECLUSAGE)pVertexElements[i].Usage).c_str();
      desc.SemanticIndex = 0;
      desc.Format = convert::declType((D3DDECLTYPE)pVertexElements[i].Type);
      desc.InputSlot = pVertexElements[i].UsageIndex;
      desc.AlignedByteOffset = pVertexElements[i].Offset;
      desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      desc.InstanceDataStepRate = 0;
      
      inputElements.push_back(desc);
    }

    *ppDecl = ref(new Direct3DVertexDeclaration9(this, inputElements, d3d9Elements));

    return D3D_OK;
  }

  bool Direct3DDevice9Ex::CanDraw() {
    return !(m_state->dirtyFlags & dirtyFlags::vertexDecl &&
             m_state->dirtyFlags & dirtyFlags::vertexShader);
  }

  void Direct3DDevice9Ex::UpdatePixelShader() {
    if (m_state->pixelShader != nullptr)
      m_context->PSSetShader(m_state->pixelShader->GetD3D11Shader(), nullptr, 0);
    else
      m_context->PSSetShader(nullptr, nullptr, 0);

    m_state->dirtyFlags &= ~dirtyFlags::pixelShader;
  }

  bool Direct3DDevice9Ex::PrepareDraw() {
    if (m_state->dirtyFlags & dirtyFlags::vertexDecl || m_state->dirtyFlags & dirtyFlags::vertexShader)
      UpdateVertexShaderAndInputLayout();

    if (m_state->dirtyFlags & dirtyFlags::renderTargets || m_state->dirtyFlags & dirtyFlags::depthStencil)
      UpdateRenderTargets();

    if (m_state->dirtyFlags & dirtyFlags::pixelShader)
      UpdatePixelShader();

    m_constants.prepareDraw();

    return CanDraw();
  }

  void Direct3DDevice9Ex::UpdateVertexShaderAndInputLayout() {
    auto& elements = m_state->vertexDecl->GetD3D11Descs();
    auto* vertexShdrBytecode = m_state->vertexShader->GetTranslation();

    ID3D11InputLayout* layout = m_state->vertexShader->GetLinkedInput(m_state->vertexDecl.ptr());

    bool created = false;
    if (layout == nullptr) {
      HRESULT result = m_device->CreateInputLayout(&elements[0], elements.size(), vertexShdrBytecode->getBytecode(), vertexShdrBytecode->getByteSize(), &layout);

      if (!FAILED(result)) {
        m_state->vertexShader->LinkInput(layout, m_state->vertexDecl.ptr());

        layout->Release();
      }
    }

    if (layout == nullptr)
      return;

    m_state->dirtyFlags &= ~dirtyFlags::vertexDecl;
    m_state->dirtyFlags &= ~dirtyFlags::vertexShader;

    m_context->IASetInputLayout(layout);

    m_context->VSSetShader(m_state->vertexShader->GetD3D11Shader(), nullptr, 0);
  }

  void Direct3DDevice9Ex::FinishDraw() {
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) {
    m_state->vertexDecl = reinterpret_cast<Direct3DVertexDeclaration9*>(pDecl);
    m_state->dirtyFlags |= dirtyFlags::vertexDecl;

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) {
    InitReturnPtr(ppDecl);

    if (ppDecl == nullptr)
      return D3DERR_INVALIDCALL;

    if (m_state->vertexDecl == nullptr)
      return D3DERR_NOTFOUND;

    *ppDecl = ref(m_state->vertexDecl);

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetFVF(DWORD FVF) {
    m_fvf = FVF;
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetFVF(DWORD* pFVF) {
    if (pFVF == nullptr)
      return D3DERR_INVALIDCALL;

    *pFVF = m_fvf;
    return D3D_OK;
  }

    static int32_t shaderNum = 0;

  template <bool Vertex, typename ID3D9, typename D3D9, typename D3D11>
  HRESULT CreateShader(CONST DWORD* pFunction, ID3D9** ppShader, ID3D11Device* device, Direct3DDevice9Ex* wrapDevice) {
    InitReturnPtr(ppShader);

    if (pFunction == nullptr || ppShader == nullptr)
      return D3DERR_INVALIDCALL;

    dx9asm::ShaderBytecode* bytecode = nullptr;
    dx9asm::toDXBC((const uint32_t*)pFunction, &bytecode);

    shaderNum++;

    if (config::getBool(config::ShaderDump)) {

      char name[64];
      snprintf(name, 64, "shader_%d.bin", shaderNum);

      FILE* file = fopen(name, "wb");
      fwrite(bytecode->getBytecode(), 1, bytecode->getByteSize(), file);
      fclose(file);
      
      // Disassemble.

      char comments[2048];
      Com<ID3DBlob> blob;

      HRESULT result = S_OK;

      if (!d3dcompiler::disassemble(&result, bytecode->getBytecode(), bytecode->getByteSize(), D3D_DISASM_ENABLE_COLOR_CODE, comments, &blob))
        log::warn("Failed to load d3dcompiler module for disassembly.");

      if (FAILED(result))
        log::warn("Failed to disassemble generated shader!");

      if (blob != nullptr) {

        char name[64];
        snprintf(name, 64, "shader_%d.html", shaderNum);

        FILE* file = fopen(name, "wb");
        fwrite(blob->GetBufferPointer(), 1, blob->GetBufferSize(), file);
        fclose(file);
      }
    }

    Com<D3D11> shader;
    HRESULT result = D3DERR_INVALIDCALL;

    if (Vertex)
      result = device->CreateVertexShader(bytecode->getBytecode(), bytecode->getByteSize(), nullptr, (ID3D11VertexShader**) &shader);
    else
      result = device->CreatePixelShader(bytecode->getBytecode(), bytecode->getByteSize(), nullptr, (ID3D11PixelShader**) &shader);

    if (FAILED(result)) {
      log::fail("Shader translation failed!");
      return D3DERR_INVALIDCALL;
    }

    *ppShader = ref(new D3D9(wrapDevice, pFunction, shader.ptr(), bytecode));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader) {
    return CreateShader<true, IDirect3DVertexShader9, Direct3DVertexShader9, ID3D11VertexShader>(pFunction, ppShader, m_device.ptr(), this);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShader(IDirect3DVertexShader9* pShader) {
    if (pShader == nullptr) {
      m_state->vertexShader = nullptr;
      return D3DERR_INVALIDCALL;
    }

    m_state->vertexShader = reinterpret_cast<Direct3DVertexShader9*>(pShader);
    m_state->dirtyFlags |= dirtyFlags::vertexShader;

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShader(IDirect3DVertexShader9** ppShader) {
    InitReturnPtr(ppShader);

    if (ppShader == nullptr)
      return D3DERR_INVALIDCALL;

    if (m_state->vertexShader == nullptr)
      return D3DERR_NOTFOUND;

    *ppShader = ref(m_state->vertexShader);

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {
    return m_constants.set(ShaderType::Vertex, BufferType::Float, StartRegister, (const void*)pConstantData, Vector4fCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
    return m_constants.get(ShaderType::Vertex, BufferType::Float, StartRegister, (void*)pConstantData, Vector4fCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) {
    return m_constants.set(ShaderType::Vertex, BufferType::Int, StartRegister, (const void*)pConstantData, Vector4iCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
    return m_constants.get(ShaderType::Vertex, BufferType::Int, StartRegister, (void*)pConstantData, Vector4iCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount) {
    return m_constants.set(ShaderType::Vertex, BufferType::Bool, StartRegister, (const void*)pConstantData, BoolCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
    return m_constants.get(ShaderType::Vertex, BufferType::Bool, StartRegister, (void*)pConstantData, BoolCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) {
    Direct3DVertexBuffer9* vertexBuffer = reinterpret_cast<Direct3DVertexBuffer9*>(pStreamData);
    ID3D11Buffer* buffer = vertexBuffer->GetD3D11Buffer();
    m_context->IASetVertexBuffers(StreamNumber, 1, &buffer, &Stride, &OffsetInBytes);

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride) {
    log::stub("Direct3DDevice9Ex::GetStreamSource");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetStreamSourceFreq(UINT StreamNumber, UINT Setting) {
    log::stub("Direct3DDevice9Ex::SetStreamSourceFreq");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting) {
    log::stub("Direct3DDevice9Ex::GetStreamSourceFreq");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetIndices(IDirect3DIndexBuffer9* pIndexData) {
    Direct3DIndexBuffer9* indexBuffer = reinterpret_cast<Direct3DIndexBuffer9*>(pIndexData);

    DXGI_FORMAT format = DXGI_FORMAT_R16_UINT;
    if (indexBuffer->GetFormat() == D3DFMT_INDEX32)
      format = DXGI_FORMAT_R32_UINT;

    m_context->IASetIndexBuffer(indexBuffer->GetD3D11Buffer(), format, 0);

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetIndices(IDirect3DIndexBuffer9** ppIndexData) {
    log::stub("Direct3DDevice9Ex::GetIndices");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreatePixelShader(CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader) {
    return CreateShader<false, IDirect3DPixelShader9, Direct3DPixelShader9, ID3D11PixelShader>(pFunction, ppShader, m_device.ptr(), this);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShader(IDirect3DPixelShader9* pShader) {
    m_state->dirtyFlags |= dirtyFlags::pixelShader;

    if (pShader == nullptr) {
      m_state->pixelShader = nullptr;
      return D3D_OK;
    }

    m_state->pixelShader = reinterpret_cast<Direct3DPixelShader9*>(pShader);

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShader(IDirect3DPixelShader9** ppShader) {
    InitReturnPtr(ppShader);

    if (ppShader == nullptr)
      return D3DERR_INVALIDCALL;

    if (m_state->pixelShader == nullptr)
      return D3DERR_NOTFOUND;

    *ppShader = ref(m_state->pixelShader);

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {
    return m_constants.set(ShaderType::Pixel, BufferType::Float, StartRegister, (const void*)pConstantData, Vector4fCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
    return m_constants.get(ShaderType::Pixel, BufferType::Float, StartRegister, (void*)pConstantData, Vector4fCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) {
    return m_constants.set(ShaderType::Pixel, BufferType::Int, StartRegister, (const void*)pConstantData, Vector4iCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
    return m_constants.get(ShaderType::Pixel, BufferType::Int, StartRegister, (void*)pConstantData, Vector4iCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount) {
    return m_constants.set(ShaderType::Pixel, BufferType::Bool, StartRegister, (const void*)pConstantData, BoolCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
    return m_constants.get(ShaderType::Pixel, BufferType::Bool, StartRegister, (void*)pConstantData, BoolCount);
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawRectPatch(UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO* DrawRectPatch) {
    log::stub("Direct3DDevice9Ex::DrawRectPatch");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawTriPatch(UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO* pTriPatchInfo) {
    log::stub("Direct3DDevice9Ex::DrawTriPatch");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DeletePatch(UINT Handle) {
    log::stub("Direct3DDevice9Ex::DeletePatch");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {
    log::stub("Direct3DDevice9Ex::CreateQuery");
    return D3D_OK;
  }

  // Ex

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetConvolutionMonoKernel(UINT width, UINT height, float* rows, float* columns) {
    log::stub("Direct3DDevice9Ex::SetConvolutionMonoKernel");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ComposeRects(IDirect3DSurface9* pSrc, IDirect3DSurface9* pDst, IDirect3DVertexBuffer9* pSrcRectDescs, UINT NumRects, IDirect3DVertexBuffer9* pDstRectDescs, D3DCOMPOSERECTSOP Operation, int Xoffset, int Yoffset) {
    log::stub("Direct3DDevice9Ex::ComposeRects");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetGPUThreadPriority(INT* pPriority) {
    if (pPriority == nullptr)
      return D3DERR_INVALIDCALL;

    if (FAILED(m_dxgiDevice->GetGPUThreadPriority(pPriority)))
      return D3DERR_INVALIDCALL;

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetGPUThreadPriority(INT Priority) {
    if (FAILED(m_dxgiDevice->SetGPUThreadPriority(Priority)))
      return D3DERR_INVALIDCALL;

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::WaitForVBlank(UINT iSwapChain) {
    Direct3DSwapChain9Ex* swapchain = GetInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    swapchain->WaitForVBlank();
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CheckResourceResidency(IDirect3DResource9** pResourceArray, UINT32 NumResources) {
    log::stub("Direct3DDevice9Ex::CheckResourceResidency");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetMaximumFrameLatency(UINT MaxLatency) {
    if (MaxLatency > 16)
      MaxLatency = 16;

    if (FAILED(m_dxgiDevice->SetMaximumFrameLatency(MaxLatency)))
      return D3DERR_INVALIDCALL;

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetMaximumFrameLatency(UINT* pMaxLatency) {
    if (pMaxLatency == nullptr)
      return D3DERR_INVALIDCALL;

    if (FAILED(m_dxgiDevice->GetMaximumFrameLatency(pMaxLatency)))
      return D3DERR_INVALIDCALL;

    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CheckDeviceState(HWND hDestinationWindow) {
    Direct3DSwapChain9Ex* swapchain = GetInternalSwapchain(0);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->TestSwapchain(hDestinationWindow, true);
  }

  void Direct3DDevice9Ex::DoDepthDiscardCheck() {
    if (m_state->depthStencil != nullptr && m_state->depthStencil->GetDiscard()) {
      ID3D11DepthStencilView* dsv = m_state->depthStencil->GetD3D11DepthStencil();
      if (dsv)
        m_context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 0.0f, 0);
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::PresentEx(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags) {
    // Not sure what swapchain to use here, going with this one ~ Josh
    HRESULT result = GetInternalSwapchain(0)->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

    DoDepthDiscardCheck();

    if (m_pendingCursorUpdate.update)
      SetCursorPosition(m_pendingCursorUpdate.x, m_pendingCursorUpdate.y, D3DCURSOR_IMMEDIATE_UPDATE);
    
    return result;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateRenderTargetEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage) {
    log::stub("Direct3DDevice9Ex::CreateRenderTargetEx");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage) {
    log::stub("Direct3DDevice9Ex::CreateOffscreenPlainSurfaceEx");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateDepthStencilSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle, DWORD Usage) {
    log::stub("Direct3DDevice9Ex::CreateDepthStencilSurfaceEx");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ResetEx(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode) {
    log::stub("Direct3DDevice9Ex::ResetEx");
    return D3D_OK;
  }
  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDisplayModeEx(UINT iSwapChain, D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation) {
    log::stub("Direct3DDevice9Ex::GetDisplayModeEx");
    return D3D_OK;
  }

  void Direct3DDevice9Ex::GetParent(Direct3D9Ex** parent) {
    *parent = static_cast<Direct3D9Ex*>( ref(m_parent) );
  }
  ID3D11DeviceContext* Direct3DDevice9Ex::GetContext() {
    return m_context.ptr();
  }
  ID3D11Device* Direct3DDevice9Ex::GetD3D11Device() {
    return m_device.ptr();
  }

}