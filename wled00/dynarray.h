/* dynarray.h

Macros for generating a "dynamic array", a static array of objects declared in different translation units

*/

#pragma once

// Declare the beginning and ending elements of a dynamic array of 'type'.
// This must be used in only one translation unit in your program for any given array.
#define DECLARE_DYNARRAY(type, array_name) \
  static type const DYNARRAY_BEGIN(array_name)[0] __attribute__((__section__(DYNARRAY_SECTION "." #array_name ".00000"), unused)) = {}; \
  static type const DYNARRAY_END(array_name)[0] __attribute__((__section__(DYNARRAY_SECTION "." #array_name ".99999"), unused)) = {};

// Declare an object that is a member of a dynamic array.  "member name" must be unique; "array_section" is 5-digit integer for ordering items.
// It is legal to define multiple items with the same section name; the order of those items will be up to the linker.
#define DYNARRAY_MEMBER(type, array_name, member_name, array_section) \
  DYNARRAY_CHECK_SECTION(#array_section) \
  type const member_name __attribute__((__section__(DYNARRAY_SECTION "." #array_name "." #array_section), used))

#define DYNARRAY_BEGIN(array_name) array_name##_begin
#define DYNARRAY_END(array_name) array_name##_end
#define DYNARRAY_LENGTH(array_name) (&DYNARRAY_END(array_name)[0] - &DYNARRAY_BEGIN(array_name)[0])

// The ordering key is pasted verbatim in to the section name, so it must be exactly five digits:
// the linker sorts sections lexicographically, and zero padding is what makes that match numeric order.
// The check is applied to the same stringified token the section attribute uses, so passing a macro that
// expands to a number fails here, just as it would produce a bad section name.
#define DYNARRAY_IS_DIGIT(s, i) ((s)[i] >= '0' && (s)[i] <= '9')
#define DYNARRAY_IS_KEY(s) (sizeof(s) == 6 && DYNARRAY_IS_DIGIT(s, 0) && DYNARRAY_IS_DIGIT(s, 1) && DYNARRAY_IS_DIGIT(s, 2) && DYNARRAY_IS_DIGIT(s, 3) && DYNARRAY_IS_DIGIT(s, 4))
#ifdef __cplusplus
#define DYNARRAY_CHECK_SECTION(str) \
  static_assert(DYNARRAY_IS_KEY(str), "DYNARRAY_MEMBER ordering key '" str "' must be exactly 5 digits, eg. 00100");
#else
#define DYNARRAY_CHECK_SECTION(str)  // no constant string subscripting in C
#endif

#define DYNARRAY_SECTION ".dynarray"
