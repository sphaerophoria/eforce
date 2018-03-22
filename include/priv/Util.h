#pragma once

namespace eforce
{
    /**
     * @brief Returns the difference between two items
     */
    template <typename T, typename U>
    auto Difference(T const& a, U const& b) -> decltype(a - b)
    {
        return (a > b) ? a - b : b - a;
    }

    /**
     * @brief To be used in a unique ptr as a deleter for a c style function 
     *   that returned a pointer that was originally malloc'd
     */
    template <typename T>
    struct MallocDeleter 
    {
        void operator()(T* ptr)
        {
            free(ptr);
        }
    };
} // namespace eforce