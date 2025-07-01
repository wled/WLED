#include <FastLED.h>

FASTLED_USING_NAMESPACE

// Define a dummy FastPin<255> that does nothing
template<>
class FastPin<255> {
public:
    static void setOutput() {}
    static void setInput() {}
    static void hi() {}
    static void lo() {}
    static void toggle() {}
    static void strobe() {}
    static void set(bool) {}
    static bool isset() { return false; }
    static void mark() {}
    static void init() {}
    static void config() {}
    static void unset() {}
    static constexpr bool validpin() { return true; }
};