#ifndef DRAGAZO_CSX64_PUNNING_H
#define DRAGAZO_CSX64_PUNNING_H

#include <cstring>
#include <type_traits>
#include <memory>

namespace CSX64
{
    // safely puns an object of type U into an object of type T.
    // this function only participates in overload resolution if T and U are both trivial types and equal in size.
    template<typename T, typename U, std::enable_if_t<std::is_trivial<T>::value && std::is_trivial<U>::value && sizeof(T) == sizeof(U), int> = 0>
    constexpr T pun(U value) 
    {
        T temp;
        std::memcpy(std::addressof(temp), std::addressof(value), sizeof(T));
        return temp; 
    }

    // -------------------------------------------------------------------------------------------------------

    // safely writes an object of type T to an arbitrary memory location.
    // only participates in overload resolution if T is a trivial type.
    template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
    constexpr void bin_write(void *dest, T value)
    {
        std::memcpy(dest, std::addressof(value), sizeof(T));
    }

    // safely reads an object of type T from an arbitrary memory location.
    // only participates in overload resolution if T is a trivial type.
    template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
    constexpr T bin_read(const void *src)
    {
        T temp;
        std::memcpy(std::addressof(temp), src, sizeof(T));
        return temp;
    }
}

#endif
