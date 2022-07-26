/** \file
    \brief Counter Math
    \copyright Copyright (c) 2017-2018 Christopher A. Taylor.  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of Counter nor the names of its contributors may be
      used to endorse or promote products derived from this software without
      specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

/** \page Counter Math

    Represents an unsigned counter that can roll over from its maximum value
    back to zero.  A common example is a 32-bit timestamp from GetTickCount()
    on Windows, which can roll-over and cause software bugs despite testing.
    This also provides compression for counters.

    This class provides:
    + Counters of 2 bits through 64 bits e.g. 24-bit counters
    + Increment/decrement by 1 or a constant
    + Safe comparison operator overloads
    + Compression/decompression via truncation
    + Unit tested software
*/

#include <cstdint>
#include <type_traits>

// Compiler-specific force inline keyword
#if defined(_MSC_VER)
    #define COUNTER_FORCE_INLINE inline __forceinline
#else // _MSC_VER
    #define COUNTER_FORCE_INLINE inline __attribute__((always_inline))
#endif // _MSC_VER


//------------------------------------------------------------------------------
// Counter

template<typename T, unsigned TkBits> class Counter
{
public:
    typedef Counter<T, TkBits> ThisType;
    typedef T ValueType;
    typedef typename std::make_signed<ValueType>::type SignedType;

    static const unsigned kBits = TkBits; ///< Number of data bits
    static const T kMSB = (T)1 << (TkBits - 1); ///< Most Significant Bit

    /// Generate a bit mask with 1s for each data bit
    /// The mask gets optimized away when compiler optimizations are enabled
    static const T kMask = static_cast<T>(-(int64_t)1) >> (sizeof(T) * 8 - kBits);

    /// Counter value
    T Value;


    //--------------------------------------------------------------------------
    // Assignment

    Counter(T value = 0)
        : Value(value & kMask)
    {
    }
    Counter(const ThisType& b)
        : Value(b.Value)
    {
    }

    ThisType& operator=(T value)
    {
        Value = value & kMask;
        return *this;
    }
    ThisType& operator=(const ThisType b)
    {
        Value = b.Value;
        return *this;
    }


    //--------------------------------------------------------------------------
    // Accessors

    /// Get current value
    COUNTER_FORCE_INLINE T ToUnsigned() const
    {
        return Value;
    }


    //--------------------------------------------------------------------------
    // Increment

    /// Pre-increment
    COUNTER_FORCE_INLINE ThisType& operator++()
    {
        Value = (Value + 1) & kMask;
        return *this;
    }

    /// Pre-decrement
    COUNTER_FORCE_INLINE ThisType& operator--()
    {
        Value = (Value - 1) & kMask;
        return *this;
    }

    /// Post-increment
    COUNTER_FORCE_INLINE ThisType operator++(int)
    {
        const T oldValue = Value;
        Value = (Value + 1) & kMask;
        return oldValue;
    }

    /// Post-decrement
    COUNTER_FORCE_INLINE ThisType operator--(int)
    {
        const T oldValue = Value;
        Value = (Value - 1) & kMask;
        return oldValue;
    }


    //--------------------------------------------------------------------------
    // Addition

    COUNTER_FORCE_INLINE ThisType& operator+=(const ThisType b)
    {
        Value = (Value + b.Value) & kMask;
        return *this;
    }

    COUNTER_FORCE_INLINE ThisType operator+(const ThisType b) const
    {
        return Value + b.Value; // Implicit mask
    }

    COUNTER_FORCE_INLINE ThisType& operator-=(const ThisType b)
    {
        Value = (Value - b.Value) & kMask;
        return *this;
    }

    COUNTER_FORCE_INLINE ThisType operator-(const ThisType b) const
    {
        return Value - b.Value; // Implicit mask
    }


    //--------------------------------------------------------------------------
    // Comparison
    // We can only compare counters of the same type

    COUNTER_FORCE_INLINE bool operator==(const ThisType b) const
    {
        return Value == b.Value;
    }
    COUNTER_FORCE_INLINE bool operator!=(const ThisType b) const
    {
        return Value != b.Value;
    }
    COUNTER_FORCE_INLINE bool operator>=(const ThisType b) const
    {
        const T d = static_cast<T>(Value - b.Value) & kMask;
        return d < kMSB;
    }
    COUNTER_FORCE_INLINE bool operator<(const ThisType b) const
    {
        const T d = static_cast<T>(Value - b.Value) & kMask;
        return d >= kMSB;
    }
    COUNTER_FORCE_INLINE bool operator<=(const ThisType b) const
    {
        const T d = static_cast<T>(b.Value - Value) & kMask;
        return d < kMSB;
    }
    COUNTER_FORCE_INLINE bool operator>(const ThisType b) const
    {
        const T d = static_cast<T>(b.Value - Value) & kMask;
        return d >= kMSB;
    }


    //--------------------------------------------------------------------------
    // Counter Compression (Truncation) and Re-Expansion

    /**
        These routines will truncate a counter to a smaller number of bits,
        and later expand the smaller value back into the original counter value
        provided with a reference counter.  For example a 64-bit timestamp can
        be compressed down to 24 bits, sent over a network, and then expanded
        back to the original value given a local time at the receiver.

        This assumes that counters are counting up and that roll-over can only
        happen one time.  If a counter rolls over twice, then the resulting
        expanded counter value will be incorrect.
    */

    /// Compress to smaller counter by truncating
    template<class SmallerT>
    COUNTER_FORCE_INLINE SmallerT Truncate() const
    {
        static_assert(SmallerT::kBits < kBits, "Smaller type must be smaller");

        // Truncate to smaller type
        return SmallerT(static_cast<typename SmallerT::ValueType>(Value));
    }

    /// Expand from truncated counter
    /// Bias > 0 can be used to accept values farther in the past
    /// Bias < 0 can be used to accept values farther in the future
    template<class SmallerT>
    static COUNTER_FORCE_INLINE ThisType ExpandFromTruncatedWithBias(
        const ThisType recent,
        const SmallerT smaller,
        const SignedType bias)
    {
        static_assert(SmallerT::kBits < kBits, "Smaller type must be smaller");

        /**
            The bits in the smaller counter were all truncated from the correct
            value, so what needs to be determined now is all the higher bits.

            Examples:

            Recent    Smaller  =>  Expanded
            ------    -------      --------
            0x100     0xff         0x0ff
            0x16f     0x7f         0x17f
            0x17f     0x6f         0x16f
            0x1ff     0xa0         0x1a0
            0x1ff     0x01         0x201

            The choice to make is between -1, 0, +1 for the next bit position.

            Since we have no information about the high bits, it should be
            sufficient to compare the recent low bits with the smaller value
            in order to decide which one is correct:

            00 - ff = -ff -> -1
            6f - 7f = -10 -> 0
            7f - 6f = +10 -> 0
            ff - a0 = +5f -> 0
            ff - 01 = +fe -> +1
        */

        // First insert the low bits to get the default result
        ThisType result = smaller.ToUnsigned() | (recent.ToUnsigned() & ~static_cast<T>(SmallerT::kMask));

        // Grab the low bits of the recent counter
        const T recentLow = recent.ToUnsigned() & SmallerT::kMask;

        // If recent - smaller would be negative:
        if (recentLow < smaller.ToUnsigned())
        {
            // If it is large enough to roll back a MSB:
            const T absDiff = smaller.ToUnsigned() - recentLow;
            if (absDiff >= static_cast<T>(SmallerT::kMSB - bias))
                result -= static_cast<T>(SmallerT::kMSB) << 1;
        }
        else
        {
            // If it is large enough to roll ahead a MSB:
            const T absDiff = recentLow - smaller.ToUnsigned();
            if (absDiff > static_cast<T>(SmallerT::kMSB + bias))
                result += static_cast<T>(SmallerT::kMSB) << 1;
        }

        return result;
    }

    /// Expand from truncated counter without any bias
    template<class SmallerT>
    static COUNTER_FORCE_INLINE ThisType ExpandFromTruncated(
        const ThisType recent,
        const SmallerT smaller)
    {
        static_assert(SmallerT::kBits < kBits, "Smaller type must be smaller");

        ValueType smallerMSB = smaller.Value & SmallerT::kMSB;
        SignedType smallerSigned = smaller.Value - (smallerMSB << 1);

        auto smallRecent = static_cast<typename SmallerT::ValueType>(recent.Value & SmallerT::kMask);

        // Signed gap = partial - prev
        auto gap = static_cast<typename SmallerT::ValueType>(smallerSigned - smallRecent) & SmallerT::kMask;

        ValueType gapMSB = gap & SmallerT::kMSB;
        SignedType gapSigned = gap - (gapMSB << 1);

        // Result = recent + gap
        return recent.Value + gapSigned;
    }

    // Template specialization to optimize cases where the word size matches
    // the field size.  Otherwise the extra sign handling above is not elided
    // by the compiler's optimizer:

    static COUNTER_FORCE_INLINE ThisType ExpandFromTruncated(
        const ThisType recent,
        const Counter<uint32_t, 32> smaller)
    {
        static_assert(32 < kBits, "Smaller type must be smaller");

        const int32_t gap = static_cast<int32_t>(smaller.Value - static_cast<uint32_t>(recent.Value));
        return recent + gap;
    }

    static COUNTER_FORCE_INLINE ThisType ExpandFromTruncated(
        const ThisType recent,
        const Counter<uint16_t, 16> smaller)
    {
        static_assert(16 < kBits, "Smaller type must be smaller");

        const int16_t gap = static_cast<int16_t>( smaller.Value - static_cast<uint16_t>(recent.Value) );
        return recent + gap;
    }

    static COUNTER_FORCE_INLINE ThisType ExpandFromTruncated(
        const ThisType recent,
        const Counter<uint8_t, 8> smaller)
    {
        static_assert(8 < kBits, "Smaller type must be smaller");

        const int8_t gap = static_cast<int8_t>(smaller.Value - static_cast<uint8_t>(recent.Value));
        return recent + gap;
    }


    static_assert(std::is_pod<T>::value, "Type must be a plain-old data type");
    static_assert(std::is_unsigned<T>::value, "Type must be unsigned");
    static_assert(TkBits > 0, "Invalid input");
    static_assert(sizeof(T) * 8 >= TkBits, "Base type is not wide enough");

    static_assert(kMask != 0, "bugcheck");
    static_assert(kMSB < kMask, "bugcheck");
    static_assert(kMSB != 0, "bugcheck");
};


/// Convenience declarations
typedef Counter<uint64_t, 64> Counter64;
typedef Counter<uint64_t, 56> Counter56;
typedef Counter<uint64_t, 48> Counter48;
typedef Counter<uint64_t, 40> Counter40;
typedef Counter<uint32_t, 32> Counter32;
typedef Counter<uint32_t, 24> Counter24;
typedef Counter<uint16_t, 16> Counter16;
typedef Counter<uint16_t, 10> Counter10;
typedef Counter<uint8_t, 8> Counter8;
typedef Counter<uint8_t, 4> Counter4;

static_assert(sizeof(Counter8) == 1, "Unexpected padding");
static_assert(sizeof(Counter16) == 2, "Unexpected padding");
static_assert(sizeof(Counter32) == 4, "Unexpected padding");
static_assert(sizeof(Counter64) == 8, "Unexpected padding");

/**
    CounterExpand()

    This is a common utility function that expands a 1-7 byte truncated
    counter back into a 64-bit (8 byte) counter, based on the largest
    counter value seen so far.

    Preconditions: bytes > 0 && bytes < 8
*/
COUNTER_FORCE_INLINE Counter64 CounterExpand(
    uint64_t largest,
    uint64_t partial,
    unsigned bytes)
{
    switch (bytes)
    {
    case 1: return Counter64::ExpandFromTruncated(largest, Counter8((uint8_t)partial));
    case 2: return Counter64::ExpandFromTruncated(largest, Counter16((uint16_t)partial));
    case 3: return Counter64::ExpandFromTruncated(largest, Counter24((uint32_t)partial));
    case 4: return Counter64::ExpandFromTruncated(largest, Counter32((uint32_t)partial));
    case 5: return Counter64::ExpandFromTruncated(largest, Counter40(partial));
    case 6: return Counter64::ExpandFromTruncated(largest, Counter48(partial));
    case 7: return Counter64::ExpandFromTruncated(largest, Counter56(partial));
    default:
        break;
    }

    return 0;
}