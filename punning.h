#ifndef DRAGAZO_CSX64_PUNNING_H
#define DRAGAZO_CSX64_PUNNING_H

#include <cstring>
#include <type_traits>
#include <memory>

/*
    this header is meant to ease the headache of fixing all the strict aliasing violations.
    adds utility functions for punning types from other types and potentially other punny things.
*/

namespace CSX64
{
    // safely performs a binary copy from src to dest without violating any strict aliasing rules.
    // this function only participates in overload resolution if T and U are both trivial types and equal in size.
    // the (scalar) objects pointed by dest and src must not overlap.
    template<typename T, typename U, std::enable_if_t<std::is_trivial<T>::value && std::is_trivial<U>::value && sizeof(T) == sizeof(U), int> = 0>
    constexpr void pun(T *dest, const U *src) { std::memcpy(dest, src, sizeof(T)); }

    // as pun(dest, src) where dest is a temporary value that is returned.
    template<typename T, typename U, std::enable_if_t<std::is_trivial<T>::value && std::is_trivial<U>::value && sizeof(T) == sizeof(U), int> = 0>
    constexpr T pun(const U *src) { T temp; pun(std::addressof(temp), src); return temp; }

    // -------------------------------------------------------------------------------------------------------

    // safely performs a binary copy from src to dest without violating any strict aliasing rules - the amount of memory copied is sizeof(T).
    // this function only participates in overload resolution if T is a trivial type.
    // the source and dest regions up to sizeof(T) bytes must not overlap.
    template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
    constexpr void bin_cpy(void *dest, const void *src) { std::memmove(dest, src, sizeof(T)); }

    // writes a value to memory without violating strict aliasing rules - equivalent to bin_cpy<T>(dest, &value).
    template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
    constexpr void bin_write(void *dest, T value) { bin_cpy<T>(dest, std::addressof(value)); }

    // as bin_cpy(dest, src) where dest is a temporary value that is returned.
    template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
    constexpr T bin_cpy(const void *src) { T temp; bin_cpy<T>(std::addressof(temp), src); return temp; }
}

#endif
