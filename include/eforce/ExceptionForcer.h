#pragma once

#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace eforce
{
    struct ThrowInfo;

    struct ExceptionInfo
    {
        struct ParentFunction
        {
            /// Start address of parent function
            void* start;
            /// End address of parent function
            void* end;
            /// Demangled name of parent function
            std::string name;
        };

        /// Approximate address of throw
        void* addr;
        /// File throw is in
        char const* file;
        /// Line thrown from
        int line;
        /// Stringized exception constructor
        char const* exceptionStr;
        /// Information about the function thrown from
        ParentFunction parentFn;
    };

    /**
     * @brief Forces exceptions to be thrown on next fn call.
     */
    class ExceptionForcer
    {
    public:
        ExceptionForcer();
        ~ExceptionForcer();

        /**
         * @brief Gets information about all registered exceptions
         * @return A vector of exception information. See ExceptionInfo struct
         *  for more info
         */
        std::vector<ExceptionInfo> GetExceptions();

        /**
         * @brief Forces an exception that is thrown from location loc
         * @param[in] loc location that the exception is thrown from, retrieved from GetExceptions
         */
        void ForceException(void* loc);

        /**
         * @brief Forces an exception that is thrown from location loc with a custom exception
         * @param[in] loc location that the exception is thrown from, retrieved from GetExceptions
         * @param[in] pError exception to throw
         */
        void ForceException(void* loc, std::exception_ptr pError);

        /**
         * @brief Disable a forced exception at loc
         * @param[in] loc location we've previously forced an exception at with ForceException
         */
        void UnforceException(void* loc);
    private:
        class Impl;
        std::unique_ptr<Impl> m_pImpl;
    };
} // namespace eforce
