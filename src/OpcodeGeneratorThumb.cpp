#include <priv/OpcodeGeneratorThumb.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace eforce
{
namespace
{
    // See arm thumb 2 reference manual at
    // infocenter.arm.com/help/topic/com.arm.doc.qrc0001l/QRC0001_UAL.pdf. Each
    // instruction is a little endian stored 16 bit value, so when you see 0x40,
    // 0xf2, that means 0xf240
    constexpr std::array<uint8_t, 12> k_doThrowThumb{{
        0x40, 0xf2, 0x00, 0x00, // T3 mov to bottom half of register 0
        0xc0, 0xf2, 0x00, 0x00, // T2 mov to top half of register 0
        0x00, 0xf0, 0x00, 0x90, // T4 branch immediate
    }};

    void LoadHalfWord(uint16_t halfWord, uint8_t* instruction)
    {
        uint8_t imm8 = halfWord & 0xff;
        uint8_t imm3 = (halfWord >> 8) & 0x7;
        uint8_t i = (halfWord >> 11) & 0x1;
        uint8_t imm4 = (halfWord >> 12) & 0xf;

        instruction[2] = imm8;
        instruction[3] = instruction[3] | (imm3 << 4);
        instruction[0] = instruction[0] | imm4;
        instruction[1] = instruction[1] | (i << 2);
    }

    void LoadBranchTarget(int32_t branchTarget, uint8_t* instruction)
    {
        uint16_t imm11 = (branchTarget >> 1) & 0x7ff;
        uint16_t imm10 = (branchTarget >> 12) & 0x3ff;
        uint8_t i2 = (branchTarget >> 22) & 0x01;
        uint8_t i1 = (branchTarget >> 23) & 0x01;
        uint8_t s = branchTarget < 0;

        uint8_t j1 = !(i1 ^ s);
        uint8_t j2 = !(i2 ^ s);

        instruction[0] = imm10 & 0xff;
        instruction[1] = instruction[1] | ((imm10 >> 8) & 0x3) | (s << 2);
        instruction[2] = imm11 & 0xff;
        instruction[3] = instruction[3] | ((imm11 >> 8) & 0x7) | (j2 << 3) | (j1 << 5);
    }
} // namespace

    std::vector<uint8_t> OpcodeGeneratorThumb::GetThrowOpcode(
            void* fnStart,
            void* throwFn,
            std::exception_ptr* pError) 
    {
        // Basic strategy here is to populate the first argument register with
        // our exception and jump to (not call) our throw wrapper function r0
        // is our argument register and we don't have to preserve it thanks to
        // our calling convention (lucky us). Because of this we can just dump
        // a pointer in r0 and jump to Throw

        auto errorPtr = reinterpret_cast<std::ptrdiff_t>(pError);
        auto lowHalf = static_cast<uint16_t>(errorPtr & 0xffff);
        auto highHalf = static_cast<uint16_t>((errorPtr >> 16) & 0xffff);

        std::vector<uint8_t> doThrow(k_doThrowThumb.begin(), k_doThrowThumb.end());

        LoadHalfWord(lowHalf, &doThrow[0]);
        LoadHalfWord(highHalf, &doThrow[4]);

        auto throwOffset = static_cast<char*>(throwFn) - (static_cast<char*>(fnStart) + k_doThrowThumb.size());

        LoadBranchTarget(static_cast<int32_t>(throwOffset), &doThrow[8]);

        return doThrow;
    }
} // namespace eforce
