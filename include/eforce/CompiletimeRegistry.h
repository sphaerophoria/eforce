#pragma once
#include <iterator>

namespace eforce
{
    /**
     * @brief Iterator for CompiletimeRegistry class. Could be a random access iterator but 
     *   we haven't implemented that yet.
     */
    // FIXME: Implement random access iterator
    template <typename T>
    class CompiletimeRegistryIterator : public std::iterator<std::bidirectional_iterator_tag, T>
    {
    public:
        explicit CompiletimeRegistryIterator(T** const start)
            : m_current(start)
        {}

        CompiletimeRegistryIterator& operator++() { ++m_current; return *this; }
        CompiletimeRegistryIterator& operator--() { --m_current; return *this; }
        bool operator==(CompiletimeRegistryIterator const& other) const { return m_current == other.m_current; }
        bool operator!=(CompiletimeRegistryIterator const& other) const { return m_current != other.m_current; }
        T& operator*() { return **m_current; }
        T* operator->() { return *m_current; }

    private:
        T** m_current;
    };

    /**
     * @brief Wrapper class around a copmile time registry. Given a start and stop pointer provides an 
     *   stl like interface around that section. Can be constructed with the COMPILETIME_REGISTRY macro
     *   or with __start_* __stop_* linker symbols.
     */
    template <typename T>
    class CompiletimeRegistry
    {
    public:
        CompiletimeRegistry(T** start, T** stop)
            : mk_start(start)
            , mk_stop(stop)
        {}

        CompiletimeRegistryIterator<T> begin() const
        {
            return CompiletimeRegistryIterator<T>{mk_start};
        }

        CompiletimeRegistryIterator<T> end() const
        {
            return CompiletimeRegistryIterator<T>{mk_stop};
        }

        size_t size() const
        {
            return mk_stop - mk_start;
        }

    private:
        T** const mk_start;
        T** const mk_stop;

    };
} // namespace eforce

/**
 * @brief This puts a pointer to an item in a compile time generated map. This
 *  can later be used wtih the CompiletimeRegistry class or COMPILETIME_REGISTRY
 *  macro to iterate over a list of items generated at compile time.
 * @param[in] __addr address of the item to register
 * @param[in] __loc which section to register the item in
 */
#define COMPILETIME_REGISTER(__addr, __loc)  \
    /*
     * Compiletime registery is done with the help of our linker. We ask ld to put
     * a pointer to the given address into a special section in the binary. We later
     * can iterate over pointers in this section to get a list of all registered 
     * items.
     *
     * Ideally we could just create a static pointer with __attibute__((section()).
     * GCC unfortunately cannot handle this when we are in inline functions or
     * templates. Because of this we do some inline assembly to generate something
     * similar to what GCC would generate in the correct case.
     *
     * For those of us not too familiar with inline assembly tihs basically does
     *
     * #if sizeof(int) == sizeof(void*)
     * allocate an int with value __addr
     * #else if sizeof(void*) == 8
     * allocate 8 byte value __addr
     * #endif
     *
     * in the section of __loc
     */ \
    __asm__( \
        ".pushsection \"" __loc "\",\"a\",%%progbits\r\n" \
        ".if %c0 == %c1\r\n" \
        ".int %c2\r\n" \
        ".elseif %c0 == 8\r\n" \
        ".quad %c2\r\n" \
        ".endif\r\n" \
        ".popsection\r\n" \
        :: "i"(sizeof(void*)), "i"(sizeof(int)), "i"(__addr))

#define COMPILETIME_CAT_HELPER(a, b) a ## b
#define COMPILETIME_CAT(a, b) COMPILETIME_CAT_HELPER(a, b)

/**
 * @brief Construct a CompiletimeRegistry
 * @param[in] __type[in] Type of the compiletime registry
 * @param[in] __loc[in] The section our type is stored in
 *   Note: Not a string parameter
 * @note This macro will not work if used inside a namespace
 */
#define COMPILETIME_REGISTRY(__type, __loc) []()-> ::eforce::CompiletimeRegistry<__type> { \
        extern __type* COMPILETIME_CAT(__start_, __loc)[] __attribute__((weak)); \
        extern __type* COMPILETIME_CAT(__stop_, __loc)[] __attribute__((weak)); \
        return {COMPILETIME_CAT(__start_, __loc), COMPILETIME_CAT(__stop_, __loc)}; \
    }()
    
