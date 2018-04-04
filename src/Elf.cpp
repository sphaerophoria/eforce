#include <priv/Elf.h>
#include <priv/Util.h>

// Unfortunately the bfd header doesn't work if we don't define these
#ifndef PACKAGE
    #define PACKAGE
#endif
#ifndef PACKAGE_VERSION
    #define PACKAGE_VERSION
#endif

#include <bfd.h>
#include <cxxabi.h>
#include <algorithm>
#include <memory>
#include <vector>

#include <iostream>

namespace eforce
{
    Elf::Elf(char const* filename)
    {
        bfd_init();
        m_bfd = bfd_openr(filename, nullptr);
        bfd_check_format(m_bfd, bfd_object);
    }

    Elf::~Elf()
    {
        bfd_close(m_bfd);
    }

    Elf::Function_t Elf::GetContainingFunction(void *offset)
    {
        if (m_sortedSymbols.empty())
        {
            auto storage_needed = bfd_get_symtab_upper_bound(m_bfd);
            m_sortedSymbols.resize(storage_needed / sizeof(asymbol*), nullptr);
            auto number_of_symbols = bfd_canonicalize_symtab(m_bfd, &m_sortedSymbols[0]);
            m_sortedSymbols.resize(number_of_symbols);

            auto endIt = std::remove_if(m_sortedSymbols.begin(), m_sortedSymbols.end(), [&] (asymbol* symbol) {
                return !(symbol->flags & BSF_FUNCTION);
            });

            m_sortedSymbols.erase(endIt, m_sortedSymbols.end());

            std::sort(m_sortedSymbols.begin(), m_sortedSymbols.end(), [](asymbol* a, asymbol* b) {
                return a->value < b->value;
            });
        }

        auto nextFn = std::partition_point(m_sortedSymbols.cbegin(), m_sortedSymbols.cend(), [&] (asymbol* const symbol) {
            // symbol->value is relative to the start of the section, we care
            // about the value relative to the start of the file so we add it
            // to symbol->section->filepos
            return reinterpret_cast<void*>(symbol->value + symbol->section->filepos) < offset;
        });

        auto thisFn = std::prev(nextFn);
        void* start = reinterpret_cast<void*>((*thisFn)->value + (*thisFn)->section->filepos);
        char const* name = (*thisFn)->name;
        void* end = reinterpret_cast<void*>((*nextFn)->value + (*thisFn)->section->filepos);

        std::unique_ptr<char, MallocDeleter<char>> demangledName(abi::__cxa_demangle(name, nullptr, nullptr, nullptr));

        return Elf::Function_t {
            start,
            end,
            demangledName.get()
        };
    }

} // namespace eforce
