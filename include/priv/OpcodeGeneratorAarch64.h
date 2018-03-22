#pragma once

#include <priv/IOpcodeGenerator.h>

#include <cstdint>
#include <exception>
#include <vector>

namespace eforce
{
    class OpcodeGeneratorAarch64
        : public IOpcodeGenerator
    {
    public:
        std::vector<uint8_t> GetThrowOpcode(
            void* fnStart,
            void* throwFn,
            std::exception_ptr* error) override;
    };
} // namespace eforce
