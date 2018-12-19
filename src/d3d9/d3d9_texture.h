#pragma once

#include "d3d9_resource.h"
#include <vector>

namespace dxapex {

  template <typename D3D11ResourceType, D3DRESOURCETYPE ResourceType, typename... ID3D9BaseType>
  class Direct3DBaseTexture9 : public Direct3DResource9<ResourceType, ID3D9BaseType...> {
  public:
    Direct3DBaseTexture9(Direct3DDevice9Ex* device, D3D11ResourceType* resource, ID3D11ShaderResourceView* srv, D3DPOOL pool, DWORD usage)
      : Direct3DResource9<ResourceType, ID3D9BaseType...>(device, pool, usage)
      , m_resource(resource)
      , m_srv{ srv }
    {}

    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) override {
      log::stub("Direct3DBaseTexture9::SetLOD");
      return 0;
    }
    DWORD STDMETHODCALLTYPE GetLOD() override {
      log::stub("Direct3DBaseTexture9::GetLOD");
      return 0;
    }

    HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override {
      log::stub("Direct3DBaseTexture9::SetAutoGenFilterType");
      return D3D_OK;
    }
    D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() override {
      log::stub("Direct3DBaseTexture9::GetAutoGenFilterType");
      return D3DTEXF_ANISOTROPIC;
    }
    void STDMETHODCALLTYPE GenerateMipSubLevels() override {
      log::stub("Direct3DBaseTexture9::GenerateMipSubLevels");
    }

    D3D11ResourceType* GetResource() {
      return m_resource.ptr();
    }

    D3D11ResourceType* GetStaging() {
      return m_stagingResource.ptr();
    }

    ID3D11ShaderResourceView* GetSRV() {
      return m_srv.ptr();
    }

  protected:

    void SetStaging(D3D11ResourceType* res) {
      m_stagingResource = res;
    }

  private:
    Com<D3D11ResourceType> m_resource;
    Com<D3D11ResourceType> m_stagingResource;
    Com<ID3D11ShaderResourceView> m_srv;
  };

  using Direct3DTexture9Base = Direct3DBaseTexture9<ID3D11Texture2D, D3DRTYPE_TEXTURE, IDirect3DTexture9>;
  class Direct3DTexture9 final : public Direct3DTexture9Base
  {
  public:
    Direct3DTexture9(Direct3DDevice9Ex* device, ID3D11Texture2D* texture, ID3D11ShaderResourceView* srv, D3DPOOL pool, DWORD usage, BOOL discard);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj);

    DWORD STDMETHODCALLTYPE GetLevelCount() override;
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) override;
    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel) override;
    HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) override;
    HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level) override;
    HRESULT STDMETHODCALLTYPE AddDirtyRect(const RECT* pDirtyRect) override;

    inline void SetSubresourceMapped(UINT subresource) {
      m_mappedSubresources |= 1 << subresource;
    }
    inline void SetSubresourceUnmapped(UINT subresource) {
      m_unmappedSubresources |= 1 << subresource;
    }
    inline uint64_t GetChangedSubresources() {
      return m_mappedSubresources;
    }
    inline bool CanPushStaging() {
      return ((m_mappedSubresources - m_unmappedSubresources) == 0);
    }
    inline void ResetSubresourceMapInfo() {
      m_mappedSubresources = 0;
      m_unmappedSubresources = 0;
    }

  private:
    uint64_t m_mappedSubresources;
    uint64_t m_unmappedSubresources;
    std::vector<Com<IDirect3DSurface9>> m_surfaces;
  };

}