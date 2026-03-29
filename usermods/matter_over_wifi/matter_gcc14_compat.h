// matter_gcc14_compat.h — Workaround for CHIP SDK + GCC 14 two-phase lookup
//
// The CHIP SDK defines chip::to_underlying() in TypeTraits.h and calls it
// without qualification in template code.  GCC 14's stricter two-phase
// name lookup can't find it via ADL because the function lives in the
// 'chip' namespace but the enum argument types are in child namespaces.
//
// This header pre-defines chip::to_underlying so it is visible during
// Phase-1 lookup in every TU that force-includes it before TypeTraits.h.
//
// It also defines CHIP_TO_UNDERLYING_DEFINED so that TypeTraits.h skips
// its broken C++23 "using to_underlying = std::to_underlying;" alias (which
// is invalid C++ — std::to_underlying is a function template, not a type)
// and its duplicate template definition in the #else branch.
//
// NOTE: We intentionally do NOT add "using chip::to_underlying;" at global
// scope — that causes ambiguity with other libraries (e.g. IRremoteESP8266)
// that define their own local to_underlying.

#pragma once

#ifdef __cplusplus
#include <type_traits>

namespace chip {
template <class T>
constexpr std::underlying_type_t<T> to_underlying(T e)
{
    static_assert(std::is_enum<T>::value, "to_underlying called on non-enum value.");
    return static_cast<std::underlying_type_t<T>>(e);
}
} // namespace chip

// Signal to TypeTraits.h that chip::to_underlying is already defined.
#define CHIP_TO_UNDERLYING_DEFINED 1

#endif // __cplusplus

