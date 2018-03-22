#include <eforce/CompiletimeRegistry.h>
#include <eforce/Exception.h>
#include <eforce/ExceptionForcer.h>

#include <priv/OpcodeGeneratorAarch64.h>
#include <priv/OpcodeGeneratorThumb.h>
#include <priv/OpcodeGeneratorX64.h>
#include <priv/Elf.h>
#include <priv/IOpcodeGenerator.h>
#include <priv/ProgOffsetResolver.h>

#include <sys/mman.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

static auto s_throwInfos = COMPILETIME_REGISTRY(eforce::ThrowInfo, throw_locations);

namespace eforce
{
    void Throw(std::exception_ptr* error)
    {
        std::rethrow_exception(*error);
    }

namespace 
{
    /**
    * @brief Parses /proc/self/maps to determine the executable area of
    *   memory. This currently only considers the application, not any shared
    *   libraries linked in
    * @param[out] start: Start of executable area of process
    * @param[out] end: End of executable area of process
    */
    void GetExecutableArea(void** start, void** end)
    {
        // FIXME: Impl here is pretty dumb, if the first line isn't our exe we break
        std::ifstream iss("/proc/self/maps");
        std::string input;
        iss >> input;
        std::string delimiter = "-";
        auto delim_pos = input.find(delimiter);
        std::string start_s = input.substr(0, delim_pos);
        std::string end_s = input.substr(delim_pos + 1, input.length());
        std::stringstream ss;
        ss << std::hex << start_s;
        ss >> *start;
        ss.clear();
        ss << std::hex << end_s;
        ss >> *end;
    }

    struct ExecutableArea
    {
        void* start{};
        void* end{};
        ExecutableArea()
        {
            GetExecutableArea(&start, &end);
        }
    };

    /**
    * @brief: Class that allows writable executable segment of a program
    */
    class ScopedMprotect
    {
    public:
        /**
         * @brief Disables write protection on executable area
         */
        ScopedMprotect();

        /**
         * @brief Enables write protection on executable area
         */
        ~ScopedMprotect();

        // Remove default copy/move constructors
        ScopedMprotect(ScopedMprotect const& other) = delete;
        ScopedMprotect(ScopedMprotect&& other) = delete;
        ScopedMprotect& operator=(ScopedMprotect const& other) = delete;
        ScopedMprotect& operator=(ScopedMprotect&& other) = delete;
    private:
        void* m_start;
        void* m_end;
    };

    ScopedMprotect::ScopedMprotect()
    {
        // We use a static here because after one use the executable area in 
        // /proc/self/maps moves an our parser is too dumb to notice
        static ExecutableArea s_executableArea;
        m_start = s_executableArea.start;
        m_end = s_executableArea.end;

        int err = mprotect(
            m_start, 
            static_cast<char*>(m_end) - static_cast<char*>(m_start),
            PROT_WRITE | PROT_READ | PROT_EXEC);

        if (err < 0)
            throw std::runtime_error("Failed to mprotect");
    }

    ScopedMprotect::~ScopedMprotect()
    {
        mprotect(m_start, static_cast<char*>(m_end) - static_cast<char*>(m_start), PROT_READ | PROT_EXEC);
    }

    /**
     * @brief Enum describing which architecture we are constructing for
     */
    enum class Arch
    {
        Amd64,
        ArmThumb2,
        Aarch64,
        Fallback,
    };

    /**
     * @return Which architecture we are currently running on
     */
    Arch GetCurrentArch()
    {
    // We assume GCC for now
    #if defined(__GNUC__) && defined(__amd64__)
        return Arch::Amd64;
    #elif defined(__GNUC__) && defined(__ARM_ARCH_7A__) && defined(__thumb2__)
        return Arch::ArmThumb2;
    #elif defined(__GNUC__) && defined(__aarch64__)
        return Arch::Aarch64;
    #else
        return Arch::Fallback;
    #endif
    }

    /**
     * @brief Based on current architecture, constructs an IOpcodeGenerator which 
     *  will generate machine code for our current platform that we can copy 
     *  into our executable area.
     */
    std::unique_ptr<IOpcodeGenerator> GetOpcodeGenerator() {
        switch (GetCurrentArch())
        {
        case Arch::Amd64:
            return std::unique_ptr<IOpcodeGenerator>(new OpcodeGeneratorX64);
        case Arch::ArmThumb2:
            return std::unique_ptr<IOpcodeGenerator>(new OpcodeGeneratorThumb);
        case Arch::Aarch64:
            return std::unique_ptr<IOpcodeGenerator>(new OpcodeGeneratorAarch64);
        case Arch::Fallback:
            return std::unique_ptr<IOpcodeGenerator>(new OpcodeGeneratorFallback);
        default:
            throw std::logic_error("Invalid Arch");
        }
    }

    class ForcedException 
    {
    public:
        ForcedException(Elf::Function_t const& containingFn, std::exception_ptr pException, ProgOffsetResolver const& rOffsetResolver);
        ~ForcedException();
        ForcedException(ForcedException const& other) = delete;
        ForcedException(ForcedException&& other) noexcept;
        ForcedException& operator=(ForcedException const& other) = delete;
        ForcedException& operator=(ForcedException&& other) noexcept;
    private:
        void* m_fnStart;
        std::unique_ptr<std::exception_ptr> m_pException; 
        std::vector<uint8_t> m_originalData;
        ProgOffsetResolver const& m_rOffsetResolver;
    };

    ForcedException::ForcedException(Elf::Function_t const& containingFn, std::exception_ptr pException, ProgOffsetResolver const& rOffsetResolver)
        : m_fnStart(rOffsetResolver.FromOffset(containingFn.startOffset))
        , m_pException(new std::exception_ptr(std::move(pException)))
        , m_rOffsetResolver(rOffsetResolver)
    {
        auto fnEnd = m_rOffsetResolver.FromOffset(containingFn.endOffset);

        auto opcodeGenerator = GetOpcodeGenerator();
        auto doThrowOpcode = opcodeGenerator->GetThrowOpcode(m_fnStart, reinterpret_cast<void*>(&Throw),m_pException.get());

        size_t fnSize = reinterpret_cast<char*>(fnEnd) - reinterpret_cast<char*>(m_fnStart);

        if (doThrowOpcode.size() > fnSize)
            throw std::runtime_error("Generated opcode too large");

        std::copy(reinterpret_cast<char const*>(m_fnStart), reinterpret_cast<char const*>(m_fnStart) + doThrowOpcode.size(), std::back_inserter(m_originalData));

        ScopedMprotect scopedMprotect [[gnu::unused]];
        std::copy(doThrowOpcode.begin(), doThrowOpcode.end(), static_cast<uint8_t*>(m_fnStart));
    }

    ForcedException::ForcedException(ForcedException&& other) noexcept
        : m_fnStart(other.m_fnStart)
        , m_pException(std::move(other.m_pException))
        , m_originalData(std::move(other.m_originalData))
        , m_rOffsetResolver(other.m_rOffsetResolver)
    {}

    ForcedException& ForcedException::operator=(ForcedException&& other) noexcept
    {
        ForcedException tmp(std::move(other));
        std::swap(*this, tmp);
        return *this;
    }

    ForcedException::~ForcedException()
    {
        if (m_originalData.empty())
            return;

        ScopedMprotect protector [[gnu::unused]];

        std::copy(m_originalData.begin(), m_originalData.end(), static_cast<uint8_t*>(m_fnStart));
    }
} // namespace

    // https://monoinfinito.wordpress.com/series/exception-handling-in-c/
    class ExceptionForcer::Impl
    {
    public:
        std::vector<ExceptionInfo> GetExceptions();
        void ForceException(void* loc);
        void ForceException(void* loc, std::exception_ptr pError);
        void UnforceException(void* loc);
    private:
        Elf m_elf{"/proc/self/exe"};
        ProgOffsetResolver m_offsetResolver;
        std::map<void*, ForcedException> m_forcedExceptions;
    };

    std::vector<ExceptionInfo> ExceptionForcer::Impl::GetExceptions()
    {
        std::vector<ExceptionInfo> ret;
        ret.reserve(s_throwInfos.size());

        std::transform(s_throwInfos.begin(), s_throwInfos.end(), std::back_inserter(ret),
            [&] (ThrowInfo& throwInfo) { 
                auto func = m_elf.GetContainingFunction(m_offsetResolver.ToOffset(throwInfo.throwAddr));
                return ExceptionInfo {
                    throwInfo.throwAddr,
                    throwInfo.file,
                    throwInfo.line,
                    throwInfo.exceptionStr,
                    { 
                        m_offsetResolver.FromOffset(func.startOffset),
                        m_offsetResolver.FromOffset(func.endOffset),
                        func.name,
                    }
            };});
        
        return ret;
    }

    void ExceptionForcer::Impl::ForceException(void* loc)
    {
        ForceException(loc, std::exception_ptr());
    }

    void ExceptionForcer::Impl::ForceException(void* loc, std::exception_ptr pError)
    {
        auto throwInfo = std::find_if(s_throwInfos.begin(), s_throwInfos.end(), [&] (ThrowInfo& throwInfo) { return throwInfo.throwAddr == loc; });
        
        if (throwInfo == s_throwInfos.end())
            throw std::runtime_error("Could not find addr");

        auto containingFn = m_elf.GetContainingFunction(m_offsetResolver.ToOffset(throwInfo->throwAddr));

        if (!throwInfo->GetException && !pError)
            throw std::runtime_error("Exception input is not constant");

        auto errorToThrow = (pError) ? pError : throwInfo->GetException();

        m_forcedExceptions.emplace(loc, ForcedException(containingFn, errorToThrow, m_offsetResolver));
    }

    void ExceptionForcer::Impl::UnforceException(void* loc)
    {
        auto forcedIt = m_forcedExceptions.find(loc);
        if (forcedIt != m_forcedExceptions.end())
            m_forcedExceptions.erase(loc);
    }

    ExceptionForcer::ExceptionForcer()
        : m_pImpl(new Impl)
    {}

    ExceptionForcer::~ExceptionForcer() = default;

    std::vector<ExceptionInfo> ExceptionForcer::GetExceptions()
    {
        return m_pImpl->GetExceptions();
    }

    void ExceptionForcer::ForceException(void* loc)
    {
        m_pImpl->ForceException(loc);
    }

    void ExceptionForcer::ForceException(void* loc, std::exception_ptr pError)
    {
        m_pImpl->ForceException(loc, pError);
    }

    void ExceptionForcer::UnforceException(void* loc)
    {
        m_pImpl->UnforceException(loc);
    }
} // namespace eforce
