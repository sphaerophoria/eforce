#pragma once

#include <string>
#include <vector>

struct bfd;
struct bfd_symbol;

namespace eforce
{
    // Class to help resolve symbols in an elf file
    // 
    // Elf files request to be loaded at a certain address. It seems that if
    // this field is non-null the rest of the binary works as if this has
    // happened. This means that if we read any *value* flags from our binary,
    // they will not be relative to the start of our elf (like offsets) but
    // relative to address 0.
    // 
    // This gets a little more confusing if our binary is flagged as a dynamic
    // executable. I'm not sure why but this seems to happen occasionally. In
    // this scenario we have addresses relative to where we loaded (address 0),
    // but not the address we are actually at. In this scenario we must add our
    // runtime mapped address to the function.
    // 
    // Since this class may not be used on /proc/self/exe the only sane thing to
    // do is to return in a way that the caller can re-map addresses themself
    // relative to what they see in /proc/self/maps if that's what they want to
    // do.
    // 
    // For this reason we use offsets to the load address, and not addresses in
    // the api to this class. If users want to map this information to a running
    // binary they can manually map the offsets themselves with the help of
    // /proc/self/maps
    class Elf
    {
    public:
        struct Function_t
        {
            /// Start of function relative to file start
            void* startOffset;
            /// End of function relative to file start
            void* endOffset;
            /// Demangled function name
            std::string name;
        };

        explicit Elf(const char* filename);
        ~Elf();
        Elf(Elf const& other) = delete;
        Elf(Elf&& other) = delete;
        Elf& operator=(Elf const& other) = delete;
        Elf& operator=(Elf&& other) = delete;

        /**
         * @brief Gets function containing offset
         * @param[in] offset an address, relative to the start of the file
         */
        Function_t GetContainingFunction(void* offset);

    private:
        bfd* m_bfd;
        std::vector<bfd_symbol*> m_sortedSymbols;
    };
} // namespace eforce
