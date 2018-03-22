#include <priv/OpcodeGeneratorAarch64.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace eforce
{
namespace
{
    /// Array used as a template for generating assmbly from GetThrowOpcode
    /// This array was generated referencing 
    /// https://developer.arm.com/docs/ddi0487/latest/arm-architecture-reference-manual-armv8-for-armv8-a-architecture-profile
    /// We use MOVZ and MOVN to populate our argument resgister
    /// we use B to jump to our desired address
    constexpr std::array<uint8_t, 20> k_doThrow{{
        0x00, 0x00, 0x80, 0xd2, // Popuplate register 0 with lowest 16 bits,
                                // (and zero the rest) (movz)
        0x00, 0x00, 0xa0, 0xf2, // Populate register 0 with next 16 bits (movn)
        0x00, 0x00, 0xc0, 0xf2, // Populate register 0 with next 16 bits (movn)
        0x00, 0x00, 0xe0, 0xf2, // Populate register 0 with high 16 bits (movn)
        0x00, 0x00, 0x00, 0x14, // Branch to address (b)
    }};
    
    /**
     * @brief Populates a mov instruction with a 16 bit immediate value
     * @param[in] quarterWord 16 bit immediate value to insert into instruction
     * @param[in/out] pMoveInsn mov instruction to be populated
     */
    void PopulateQuarterWord(uint16_t quarterWord, uint8_t* pMovInsn)
    {
        pMovInsn[0] = pMovInsn[0] | ((quarterWord << 5) & 0xe0);
        pMovInsn[1] = (quarterWord >> 3) & 0xff;
        pMovInsn[2] = pMovInsn[2] | ((quarterWord >> 11) & 0x1f);
    }

    /**
     * @brief Populates a branch instruction with a location
     * @param[in] relJumpAddr relative address to instruction to jump to
     * @param[in/out] pJmpInsn instruction to populate
     */
    void PopulateJumpAddr(std::ptrdiff_t relJumpAddr, uint8_t* pJmpInsn)
    {
        if ((relJumpAddr >> 28) != 0)
            throw std::runtime_error("Throw helper too far from target function");
        
        relJumpAddr = relJumpAddr & ((1 << 28) - 1);
        pJmpInsn[0] = (relJumpAddr >> 2) & 0xff;
        pJmpInsn[1] = (relJumpAddr >> 10) & 0xff;
        pJmpInsn[2] = (relJumpAddr >> 18) & 0xff;
        pJmpInsn[3] = pJmpInsn[3] | ((relJumpAddr >> 26) & 0x3);
    }
} // namespace

    std::vector<uint8_t> OpcodeGeneratorAarch64::GetThrowOpcode(
        void* fnStart,
        void* throwFn,
        std::exception_ptr *error)
    {
        // Basic strategy here is to populate the first argument register with
        // our exception and jump to (not call) our throw wrapper function. r0
        // is our argument register and we don't have to preserve it thanks to
        // our calling convention (lucky us). Because of this we can just dump
        // a pointer in r0 and jump to Throw

        auto lowQuarter = static_cast<uint16_t>(uint64_t(error) & 0xffff);
        auto midLowQuarter = static_cast<uint16_t>((uint64_t(error) >> 16) & 0xffff);
        auto midHighQuarter = static_cast<uint16_t>((uint64_t(error) >> 32) & 0xffff);
        auto highQuarter = static_cast<uint16_t>((uint64_t(error) >> 48) & 0xffff);

        std::vector<uint8_t> doThrow(k_doThrow.begin(), k_doThrow.end());

        PopulateQuarterWord(lowQuarter, &doThrow[0]);
        PopulateQuarterWord(midLowQuarter, &doThrow[4]);
        PopulateQuarterWord(midHighQuarter, &doThrow[8]);
        PopulateQuarterWord(highQuarter, &doThrow[12]);

        PopulateJumpAddr(static_cast<char*>(throwFn) - (static_cast<char*>(fnStart) + 16), &doThrow[16]);

        return doThrow;    
    }
} // namespace eforce
