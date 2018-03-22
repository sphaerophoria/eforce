#include <priv/OpcodeGeneratorX64.h>
#include <priv/Util.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace eforce
{
    /// Generated from doing __asm__("mov 0xffffffffffffffff\r\njmp 0x00") and
    /// analyzing disassembly.
    static constexpr const std::array<uint8_t, 15> k_doThrow = {{
        0x48, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00,    //movabs rbx,0x0000000000000000
        0x00, 0x00, 0x00,
        0xe9, 0x00, 0x00, 0x00, 0x00,                //jmp 0x00 offset
    }};

    std::vector<uint8_t> OpcodeGeneratorX64::GetThrowOpcode(
        void* fnStart, 
        void* throwFn,
        std::exception_ptr* pError) 
    {
        // Basic strategy here is to populate the first argument register with
        // our exception and jump to (not call) our throw wrapper function. RBX
        // is our argument register and we don't have to preserve it thanks to
        // our calling convention (lucky us). Because of this we can just dump
        // a pointer in RBX and jump to Throw
        
        std::vector<uint8_t> doThrow(k_doThrow.begin(), k_doThrow.end());

        auto throwFnChar = static_cast<char*>(throwFn);
        auto fnStartChar = static_cast<char*>(fnStart);

        uint64_t absThrowFnOffset = Difference(throwFnChar, fnStartChar + k_doThrow.size());
        if (absThrowFnOffset > static_cast<uint32_t>(std::abs(std::numeric_limits<int32_t>::min())))
            throw std::runtime_error("Cannot generate opcode");
        
        int32_t throwFnOffset = (throwFnChar > fnStartChar + k_doThrow.size())
            ? static_cast<int32_t>(absThrowFnOffset)
            : -static_cast<int32_t>(absThrowFnOffset);

        auto ppExceptionAddr = reinterpret_cast<uint64_t*>(&doThrow[2]);
        *ppExceptionAddr = reinterpret_cast<uint64_t>(pError);

        auto ppThrowFnAddr = reinterpret_cast<int32_t*>(&doThrow[11]);
        *ppThrowFnAddr = throwFnOffset;

        return doThrow;
    }
} // namespace eforce
