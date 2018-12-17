#include "dx9asm_translator.h"
#include "dx9asm_modifiers.h"
#include "../util/fourcc.h"
#include "../util/config.h"
#include <functional>

namespace dxapex {

  namespace dx9asm {

    bool ShaderCodeTranslator::handleComment(DX9Operation& operation) {
      uint32_t commentTokenCount = operation.getCommentCount();

      uint32_t cc = nextToken();
      if (cc == fourcc("CTAB")) {
        m_ctab = (CTHeader*)(m_head);
        uint32_t tableSize = (commentTokenCount - 1) * sizeof(uint32_t);

        if (tableSize < sizeof(CTHeader) || m_ctab->size != sizeof(CTHeader)) {
          log::fail("CTAB invalid!");
          return false; // fatal
        }
      }

      skipTokens(commentTokenCount - 1);
      return true;
    }

    bool ShaderCodeTranslator::handleDcl(DX9Operation& operation) {
      const DX9Operand* usageToken = operation.getOperandByType(optype::UsageToken);
      const DX9Operand* dst = operation.getOperandByType(optype::Dst);

      RegisterMapping* mapping = m_map.lookupOrCreateRegisterMapping(getShaderType(), getMajorVersion(), getMinorVersion(), *dst);

      if (dst->getRegType() == D3DSPR_INPUT)
        mapping->dclInfo.type = UsageType::Input;
      else if (dst->getRegType() == D3DSPR_OUTPUT)
        mapping->dclInfo.type = UsageType::Output;
      else {
        log::warn("Unhandled reg type in dcl.");
        mapping->dclInfo.type = UsageType::Output;
      }
      mapping->dclInfo.usage = usageToken->getUsage();
      mapping->dclInfo.usageIndex = usageToken->getUsageIndex();
      mapping->dclInfo.centroid = dst->centroid();
      
      return true;
    }

    bool ShaderCodeTranslator::handleDef(DX9Operation& operation) {
      const DX9Operand* dst = operation.getOperandByType(optype::Dst);
      const DX9Operand* vec4 = operation.getOperandByType(optype::Vec4);

      RegisterMapping mapping;
      mapping.dx9Id = dst->getRegNumber();
      mapping.dx9Type = dst->getRegType();
      mapping.dxbcOperand.setRegisterType(D3D10_SB_OPERAND_TYPE_IMMEDIATE32);
      mapping.dxbcOperand.stripModifier();
      mapping.dxbcOperand.setDimension(D3D10_SB_OPERAND_INDEX_0D);
      uint32_t data[4];
      vec4->getValues(data);
      mapping.dxbcOperand.setData(data, 4);
      mapping.dxbcOperand.setupLiteral(4);

      mapping.dxbcOperand.setSwizzleOrWritemask(noSwizzle);

      m_map.addRegisterMapping(false, mapping);

      return true;
    }

    bool ShaderCodeTranslator::handleUniqueOperation(DX9Operation& operation) {
      UniqueFunction function = operation.getUniqueFunction();
      if (function == nullptr) {
        log::fail("Unimplemented operation %s encountered.", operation.getName());
        return !config::getBool(config::UnimplementedFatal);
      }

      return std::invoke(function, this, operation);
    }

  }

}