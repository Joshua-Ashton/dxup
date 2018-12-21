#pragma once

#include "dx9asm_meta.h"
#include "dx9asm_util.h"
#include "dxbc_helpers.h"
#include <stdint.h>
#include <algorithm>
#include <vector>
#include "dxbc_bytecode.h"
#include "dx9asm_register_map.h"

namespace dxup {

  namespace dx9asm {

    void toDXBC(const uint32_t* dx9asm, ShaderBytecode** dxbc);

    class DX9Operation;

    struct SamplerDesc {
      uint32_t index;
      uint32_t dimension;
    };

    class ShaderCodeTranslator {

    public:

      ShaderCodeTranslator(const uint32_t* code)
        : m_base{code} 
        , m_head{ code } {}

      inline uint32_t getHeaderToken() const {
        return *m_base;
      }

      inline uint32_t getMajorVersion() const {
        return D3DSHADER_VERSION_MAJOR(getHeaderToken());
      }

      inline uint32_t getMinorVersion() const {
        return D3DSHADER_VERSION_MINOR(getHeaderToken());
      }

      inline uint32_t nextToken() {
        return *m_head++;
      }

      inline void skipTokens(uint32_t count) {
        for (uint32_t i = 0; i < count; i++)
          nextToken();
      }

      inline ShaderType getShaderType() const {
        return (getHeaderToken() & 0xFFFF0000) == 0xFFFF0000 ? ShaderType::Pixel : ShaderType::Vertex;
      }

      bool handleOperation(uint32_t token);

      inline bool isSamplerUsed(uint32_t i) {
        for (auto& desc : m_samplers) {
          if (desc.index == i)
            return true;
        }
        
        return false;
      }

      inline bool isAnySamplerUsed() {
        return !m_samplers.empty();
      }

      inline SamplerDesc* getSampler(uint32_t i) {
        for (auto& desc : m_samplers) {
          if (desc.index == i)
            return &desc;
        }

        return nullptr;
      }

      bool translate();

      bool handleComment(DX9Operation& operation);
      bool handleDcl(DX9Operation& operation);
      bool handleTex(DX9Operation& operation);
      bool handleLit(DX9Operation& operation);
      bool handleNrm(DX9Operation& operation);
      bool handleDef(DX9Operation& operation);

      inline std::vector<uint32_t>& getCode() {
        return m_dxbcCode;
      }

      inline RegisterMap& getRegisterMap() {
        return m_map;
      }

    private:
      bool handleUniqueOperation(DX9Operation& operation);

      bool handleStandardOperation(DX9Operation& operation);

      const uint32_t* m_base = nullptr;
      const uint32_t* m_head = nullptr;
      const CTHeader* m_ctab = nullptr;

      RegisterMap m_map;

      std::vector<SamplerDesc> m_samplers;
      std::vector<uint32_t> m_dxbcCode;
    };

  }

}