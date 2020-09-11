#ifndef _ANGEL_ENUM_BITWISE_H
#define _ANGEL_ENUM_BITWISE_H

#include <type_traits>

#define define_enum_bitwise(__op, __op_s) \
    template <typename T, \
              typename = typename std::enable_if<std::is_enum<T>::value>::type> \
    constexpr T operator __op(T lhs, T rhs) \
    { \
        return static_cast<T>( \
                static_cast<typename std::underlying_type<T>::type>(lhs) __op \
                static_cast<typename std::underlying_type<T>::type>(rhs)); \
    } \
    template <typename T, \
              typename = typename std::enable_if<std::is_enum<T>::value>::type> \
    T& operator __op_s(T& lhs, T rhs) \
    { \
        lhs = lhs __op rhs; \
        return lhs; \
    }

define_enum_bitwise(&, &=)
define_enum_bitwise(|, |=)
define_enum_bitwise(^, ^=)

template <typename T,
          typename = typename std::enable_if<std::is_enum<T>::value>::type>
constexpr T operator ~(T t)
{
    return static_cast<T>(
            ~ static_cast<typename std::underlying_type<T>::type>(t));
}

namespace angel {
    template <typename T,
              typename = typename std::enable_if<std::is_enum<T>::value>::type>
    auto get_enum_value(T t)
    {
        return static_cast<typename std::underlying_type<T>::type>(t);
    }
    template <typename T,
              typename = typename std::enable_if<std::is_enum<T>::value>::type>
    bool is_enum_true(T t)
    {
        return static_cast<typename std::underlying_type<T>::type>(t);
    }
}

#endif // _ANGEL_ENUM_BITWISE_H
