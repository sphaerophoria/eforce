#pragma once

#include <priv/IOpcodeGenerator.h>

#include <cstdint> 
#include <exception>
#include <vector>

namespace eforce
{
    class OpcodeGeneratorX64
        : public IOpcodeGenerator 
    {
    public:
        std::vector<uint8_t> GetThrowOpcode(
            void* fnStart,
            void* throwFn,
            std::exception_ptr* pError) override;
    };
} // namespace eforce
