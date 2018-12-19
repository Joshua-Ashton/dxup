#include "d3d9_util.h"
#include "../util/type_converter.h"
#include <array>

namespace dxapex {

  namespace convert {

    using FormatConverter = TypeConverter<D3DFORMAT, DXGI_FORMAT, 34>;

    #define D3DFMT_INTZ ((D3DFORMAT)(MAKEFOURCC('I','N','T','Z')))
    #define D3DFMT_RAWZ ((D3DFORMAT)(MAKEFOURCC('R','A','W','Z')))
    #define D3DFMT_DF24 ((D3DFORMAT)(MAKEFOURCC('D','F','2','4')))
    #define D3DFMT_DF16 ((D3DFORMAT)(MAKEFOURCC('D','F','1','6')))

    FormatConverter::TypeMapping formats[] = {
      { D3DFMT_UNKNOWN, DXGI_FORMAT_UNKNOWN },
      //{ D3DFMT_B32G32R32F, DXGI_FORMAT_R32G32B32_FLOAT },
      { D3DFMT_A32B32G32R32F, DXGI_FORMAT_R32G32B32A32_FLOAT },
      { D3DFMT_A16B16G16R16F, DXGI_FORMAT_R16G16B16A16_FLOAT },
      { D3DFMT_R32F, DXGI_FORMAT_R32_FLOAT },
      { D3DFMT_R16F, DXGI_FORMAT_R16_FLOAT },
      { D3DFMT_Q16W16V16U16, DXGI_FORMAT_R16G16B16A16_SNORM },
      //{ D3DFMT_D3DDECLTYPE_SHORT4, DXGI_FORMAT_R16G16B16A16_SINT },
      //{ D3DFMT_D3DDECLTYPE_UBYTE4, DXGI_FORMAT_R8G8B8A8_UINT },
      { D3DFMT_V16U16, DXGI_FORMAT_R16G16_SNORM },
      //{ D3DFMT_D3DDECLTYPE_SHORT2, DXGI_FORMAT_R16G16_SINT },
      { D3DFMT_R32F, DXGI_FORMAT_R32_FLOAT },
      { D3DFMT_INDEX32, DXGI_FORMAT_R32_UINT },
      { D3DFMT_INDEX16, DXGI_FORMAT_R16_UINT },
      { D3DFMT_A16B16G16R16, DXGI_FORMAT_R16G16B16A16_UNORM },
      { D3DFMT_G32R32F, DXGI_FORMAT_R32G32_FLOAT },
      //{ D3DFMT_S8D24, DXGI_FORMAT_D24_UNORM_S8_UINT },
      { D3DFMT_D16, DXGI_FORMAT_D16_UNORM },
      { D3DFMT_A8B8G8R8, DXGI_FORMAT_R8G8B8A8_UNORM },
      { D3DFMT_X8R8G8B8, DXGI_FORMAT_R8G8B8A8_UNORM },
      { D3DFMT_A8B8G8R8, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
      { D3DFMT_Q8W8V8U8, DXGI_FORMAT_R8G8B8A8_SNORM },
      { D3DFMT_G16R16F, DXGI_FORMAT_R16G16_FLOAT },
      { D3DFMT_G16R16, DXGI_FORMAT_R16G16_UNORM },
      { D3DFMT_V16U16, DXGI_FORMAT_R16G16_SNORM },
      { D3DFMT_R32F, DXGI_FORMAT_R32_FLOAT },
      { D3DFMT_L16, DXGI_FORMAT_R16_UNORM }, // Swizzle
      { D3DFMT_V8U8, DXGI_FORMAT_R8G8_SNORM },
      { D3DFMT_L8, DXGI_FORMAT_R8_UNORM }, // Swizzle
      { D3DFMT_DXT1, DXGI_FORMAT_BC1_UNORM },
      { D3DFMT_DXT1, DXGI_FORMAT_BC1_UNORM_SRGB },
      { D3DFMT_DXT3, DXGI_FORMAT_BC2_UNORM },
      { D3DFMT_DXT3, DXGI_FORMAT_BC2_UNORM_SRGB },
      { D3DFMT_DXT5, DXGI_FORMAT_BC3_UNORM },
      { D3DFMT_DXT5, DXGI_FORMAT_BC3_UNORM_SRGB },
      { D3DFMT_A8R8G8B8, DXGI_FORMAT_B8G8R8A8_UNORM },
      { D3DFMT_A8, DXGI_FORMAT_A8_UNORM },
      { D3DFMT_R5G6B5, DXGI_FORMAT_B5G6R5_UNORM },
      { D3DFMT_A1R5G5B5, DXGI_FORMAT_B5G5R5A1_UNORM },
      { D3DFMT_A4R4G4B4, DXGI_FORMAT_B4G4R4A4_UNORM },

      { D3DFMT_D15S1, DXGI_FORMAT_D24_UNORM_S8_UINT },
      { D3DFMT_D24S8, DXGI_FORMAT_D24_UNORM_S8_UINT },
      { D3DFMT_D24X8, DXGI_FORMAT_D24_UNORM_S8_UINT },
      { D3DFMT_D24X4S4, DXGI_FORMAT_D24_UNORM_S8_UINT },
      { D3DFMT_D24FS8, DXGI_FORMAT_D24_UNORM_S8_UINT },
      { D3DFMT_D32, DXGI_FORMAT_D32_FLOAT },
      { D3DFMT_D16, DXGI_FORMAT_D32_FLOAT },
      { D3DFMT_DF24, DXGI_FORMAT_D24_UNORM_S8_UINT },
      { D3DFMT_DF16, DXGI_FORMAT_D24_UNORM_S8_UINT },
      //{ D3DFMT_INTZ, DXGI_FORMAT_D32_FLOAT },
      { D3DFMT_S8_LOCKABLE, DXGI_FORMAT_D24_UNORM_S8_UINT },
      { D3DFMT_D16_LOCKABLE, DXGI_FORMAT_D32_FLOAT },
      { D3DFMT_D32F_LOCKABLE, DXGI_FORMAT_D24_UNORM_S8_UINT },
      { D3DFMT_D32_LOCKABLE, DXGI_FORMAT_D32_FLOAT },
    };

    FormatConverter formatConverter{ formats };

    DXGI_FORMAT format(D3DFORMAT Format) {
      return formatConverter.toJ(Format);
    }
    D3DFORMAT format(DXGI_FORMAT Format) {
      return formatConverter.toT(Format);
    }

    //

    using ScanlineConverter = TypeConverter<D3DSCANLINEORDERING, DXGI_MODE_SCANLINE_ORDER, 4>;

    ScanlineConverter::TypeMapping scanlines[] = {
      { D3DSCANLINEORDERING_UNKNOWN, DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED },
      { D3DSCANLINEORDERING_PROGRESSIVE, DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE },
      { D3DSCANLINEORDERING_INTERLACED, DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST},
      { D3DSCANLINEORDERING_INTERLACED, DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST},
    };

    ScanlineConverter scanlineConverter{ scanlines };

    DXGI_MODE_SCANLINE_ORDER scanlineOrdering(D3DSCANLINEORDERING ScanlineOrdering) {
      return scanlineConverter.toJ(ScanlineOrdering);
    }
    D3DSCANLINEORDERING scanlineOrdering(DXGI_MODE_SCANLINE_ORDER ScanlineOrdering) {
      return scanlineConverter.toT(ScanlineOrdering);
    }

    D3D11_USAGE usage(D3DPOOL pool, UINT usage) {
      if (usage & D3DUSAGE_DYNAMIC || pool == D3DPOOL_SCRATCH || pool == D3DPOOL_SYSTEMMEM)
        return D3D11_USAGE_DYNAMIC;

      return D3D11_USAGE_DEFAULT;
    }

    UINT cpuFlags(D3DPOOL pool, UINT usage) {
      if (convert::usage(pool, usage) == D3D11_USAGE_DYNAMIC) {
        if (usage & D3DUSAGE_WRITEONLY)
          return D3D11_CPU_ACCESS_WRITE;
        return /*D3D11_CPU_ACCESS_READ |*/ D3D11_CPU_ACCESS_WRITE;
      }

      return 0;
    }

    UINT fvf(UINT fvf) {
      UINT structureCount = 0;

      if (fvf & D3DFVF_DIFFUSE) structureCount += sizeof(DWORD) * 4;
      if (fvf & D3DFVF_NORMAL) structureCount += sizeof(float) * 3;
      if (fvf & D3DFVF_PSIZE) structureCount += sizeof(float) * 1;
      if (fvf & D3DFVF_SPECULAR) structureCount += sizeof(DWORD) * 4;
      if (fvf & D3DFVF_XYZ) structureCount += sizeof(float) * 3;
      if (fvf & D3DFVF_XYZRHW) structureCount += sizeof(float) * 4;
      //if (fvf & D3DFVF_XYZB1) structureCount += sizeof(float) * 4;
      if (fvf & D3DFVF_XYZW) structureCount += sizeof(float) * 4;

      return structureCount;
    }

    UINT primitiveData(D3DPRIMITIVETYPE type, UINT count, D3D_PRIMITIVE_TOPOLOGY& topology)
    {
      switch (type) {

      default:
        log::warn("Unknown topology used.");
      case D3DPT_TRIANGLELIST: topology = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST; return count * 3;
      case D3DPT_POINTLIST: topology = D3D10_PRIMITIVE_TOPOLOGY_POINTLIST; return count;
      case D3DPT_LINELIST: topology = D3D10_PRIMITIVE_TOPOLOGY_LINELIST; return count * 2;
      case D3DPT_LINESTRIP: topology = D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP; return count + 1;
      case D3DPT_TRIANGLESTRIP: topology = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; return count + 2;
      case D3DPT_TRIANGLEFAN: log::warn("Topology D3DPT_TRIANGLEFAN used."); topology = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; return count + 2;

      }
    }

    DXGI_FORMAT declType(D3DDECLTYPE type) {

      switch (type) {

      case D3DDECLTYPE_FLOAT1: return DXGI_FORMAT_R32_FLOAT;
      case D3DDECLTYPE_FLOAT2: return DXGI_FORMAT_R32G32_FLOAT;
      case D3DDECLTYPE_FLOAT3: return DXGI_FORMAT_R32G32B32_FLOAT;
      case D3DDECLTYPE_FLOAT4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
      case D3DDECLTYPE_D3DCOLOR: return DXGI_FORMAT_R8G8B8A8_UINT;
      case D3DDECLTYPE_UBYTE4: return DXGI_FORMAT_R8G8B8A8_UINT;
      case D3DDECLTYPE_SHORT2: return DXGI_FORMAT_R16G16_SINT;
      case D3DDECLTYPE_SHORT4: return DXGI_FORMAT_R16G16B16A16_SINT;
      case D3DDECLTYPE_UBYTE4N: return DXGI_FORMAT_R8G8B8A8_UNORM;
      case D3DDECLTYPE_SHORT2N: return DXGI_FORMAT_R16G16_SNORM;
      case D3DDECLTYPE_SHORT4N: return DXGI_FORMAT_R16G16B16A16_SNORM;
      case D3DDECLTYPE_USHORT2N: return DXGI_FORMAT_R16G16_UNORM;
      case D3DDECLTYPE_USHORT4N: return DXGI_FORMAT_R16G16B16A16_UNORM;
      case D3DDECLTYPE_UDEC3: return DXGI_FORMAT_R10G10B10A2_UINT;
      case D3DDECLTYPE_DEC3N: return DXGI_FORMAT_R10G10B10A2_UNORM;
      case D3DDECLTYPE_FLOAT16_2: return DXGI_FORMAT_R16G16_FLOAT;
      case D3DDECLTYPE_FLOAT16_4: return DXGI_FORMAT_R16G16B16A16_FLOAT;
      default:
      case D3DDECLTYPE_UNUSED: return DXGI_FORMAT_UNKNOWN;

      }

    }

    void color(D3DCOLOR color, FLOAT* d3d11Color) {

      // Encoded in D3DCOLOR as argb
      d3d11Color[3] = (float)((color & 0xff000000) >> 24) / 255.0f;
      d3d11Color[0] = (float)((color & 0x00ff0000) >> 16) / 255.0f;
      d3d11Color[1] = (float)((color & 0x0000ff00) >> 8) / 255.0f;
      d3d11Color[2] = (float)((color & 0x000000ff)) / 255.0f;

    }

  }
}