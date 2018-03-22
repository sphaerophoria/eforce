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
#include <memory>
#include <vector>


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
        auto storage_needed = bfd_get_symtab_upper_bound(m_bfd);
        std::vector<asymbol*> symbol_table(storage_needed / sizeof(asymbol*), nullptr);
        auto number_of_symbols = bfd_canonicalize_symtab(m_bfd, &symbol_table[0]);
        symbol_table.resize(number_of_symbols);

        void* start = nullptr;
        auto* end = reinterpret_cast<void*>(-1);
        char const* name = nullptr;

        for (auto const& symbol : symbol_table)
        {
            if (!(symbol->flags & BSF_FUNCTION))
                continue; 

            // symbol->value is relative to the start of the section, we care
            // about the value relative to the start of the file so we add it
            // to symbol->section->filepos
            auto value = reinterpret_cast<void*>(symbol->value + symbol->section->filepos);
            if (value <= offset && value > start)
            {
                start = value;
                name = symbol->name;
            }

            if (value >= offset && value < end)
                end = value;
        }

        std::unique_ptr<char, MallocDeleter<char>> demangledName(abi::__cxa_demangle(name, nullptr, nullptr, nullptr));

        return Elf::Function_t {
            start,
            end,
            demangledName.get()
        };
    }
} // namespace eforce
