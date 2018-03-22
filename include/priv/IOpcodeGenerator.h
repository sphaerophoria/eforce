#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace eforce
{
    class IOpcodeGenerator
    {
    public:
        virtual ~IOpcodeGenerator() = default;

        /**
         * @brief Gets executable code that can be placed at fnStart to throw
         *  pError.
         * @param[in] fnStart The address we will be using this code at
         * @param[in] throwFn A function that throws pError, 
         *  of signature void ThrowFn(std::excption_ptr*)
         * @param[in] pError A pointer to an error to throw
         */
        virtual std::vector<uint8_t> GetThrowOpcode(
            void* fnStart,
            void* throwFn,
            std::exception_ptr* pError) = 0;
    };

    class OpcodeGeneratorFallback
        : public IOpcodeGenerator
    {
    public:
        std::vector<uint8_t> GetThrowOpcode(
            void* /*fnStart*/,
            void* /*throwFn*/,
            std::exception_ptr* /*pError*/) override 
        {
            assert(!"Not implemented");
            return {};
        }
    };
} // namespace eforce
