//
// Function naming convention <prefix>: <function> <length> <suffix>
//  Length
//                  8 means input is 8 bytes
//                  4 means input is 4 bytes
//                  None means input is any length
//  Prefix
//          p       Printable. Ascii 0 to 127.
//                  pmemchr vs memchr is slighly optimized for printable input
//          _       Mostly internal use
//  Suffix
//          k       Haystack is known to contain needle
//          _nc     Non-const, modifiable, input
//                  These functions modify and restore the input. Not thread safe
//
// Performance
//  Functions with 8 suffix are branchless. Function with longer input must
//  have a branch per word. This can be improved with SSE.
//  - SSE may switch some processors to different P state, if the BIOS allows,
//    and the switching itself can take a few hundred cycles
//  - Branchless code is not always faster than branched code.
//    Lab tests are typically less impacted by branch miss-predictions, then real
//    world applications.
//    Loops may be fully predicted, if BP caches are all working for the test.
//    However, in a real world app, using branchless low level code means that BP
//    caches have more room for the application logic so the app as a whole may
//    become faster.
//    The only way to know for sure, is to test within the app.
//

//
// Utils
//
#include "rtypes.h"
#include "exception.h"

// Cast char* to T using memcpy. memcpy is optimized away on x86
C_API uintptr_t cast(const char *src);
C_API uintptr_t cast8(const char *s, uint32_t len);

// Fill T with c's
C_API RAII_INLINE uintptr_t extend(char c);

/* Returns the number of trailing 0-bits in x,
starting at the least significant bit position.*/
C_API RAII_INLINE int countr_zero(uintptr_t mask);

/* Returns the number of leading 0-bits in x,
starting at the most significant bit position. */
C_API RAII_INLINE int countl_zero(uintptr_t mask);

/* Returns the index of the least significant 1-bit in x,
or the value zero if x is zero. */
C_API RAII_INLINE int countr_index(uintptr_t mask);

//
// Find byte in word
//

// Check if word has zero byte
C_API RAII_INLINE bool haszero(uint64_t x);

// Check if word has some byte
C_API RAII_INLINE bool hasbyte(uint64_t x, uint8_t c);

// Find char in binary string of 8 chars
C_API uint32_t memchr8(const char* s, uint8_t c);

// Find char in binary string of 8 chars
// * The string is known to contain the char
C_API RAII_INLINE uint32_t memchr8k(const char* s, uint8_t c);

//
// Strlen variants
//

/* Return the length of the null-terminated string STR.  Scan for
   the null terminator quickly by testing four bytes at a time. */
C_API size_t simd_strlen(const char *s);

C_API int simd_strcmp(const char *s, const char *c);

//
// Find byte in const string. Like memchr
//

// Find char in binary string
C_API string simd_memchr(const char *s, uint8_t c, uint32_t len);

// Find char in binary string. Char c is known to be in s + len
C_API RAII_INLINE uint32_t memchrk(const char* s, uint32_t len, uint8_t c);

// Find char in printable string
C_API RAII_INLINE uint32_t pmemchr(const char* s, uint32_t len, uint8_t c);

// Find char in printable string. Char c is known to be in s + len
C_API RAII_INLINE uint32_t pmemchrk(const char* s, uint32_t len, uint8_t c);

//
// Find byte, from end, in const string. Like memrchr
//

// Find char, in reverse, in binary string
C_API string simd_memrchr(const char* s, uint8_t c, uint32_t len);

// Find char in binary string. Char c is known to be in s + len
C_API RAII_INLINE uint32_t memrchrk(const char* s, uint32_t len, uint8_t c);

// Find char in printable string
C_API RAII_INLINE uint32_t pmemrchr(const char* s, uint32_t len, uint8_t c);

// Find char in printable string. Char c is known to be in s + len
C_API RAII_INLINE uint32_t pmemrchrk(const char* s, uint32_t len, uint8_t c);

//// string to int

// Parse uint64_t from string of up to 20 chars
// *** More than 20 char returns junk.
C_API uint64_t simd_atou(const char* s, uint32_t len);
C_API uint32_t atou8(const char *s, uint32_t len);

// Parse _signed_ int from string of up to 20 chars. No spaces
C_API int64_t simd_atoi(const char *s, uint32_t len);

// Parse hex int from string of up to 16 chars
C_API uint64_t simd_htou(const char* s, uint32_t len);

C_API uint32_t htou8(const char *s, uint32_t len);

//// int to string

// *** p suffix means zero-padded

// Convert uint, of less than 100, to %02u, as int 16
C_API uint16_t utoa2p(uint64_t x);

// Convert uint, of less than 100, to %02u
C_API void utoa2p_ex(uint64_t x, char* s);

// Convert uint to %0<N>u, N <= 20
C_API char* utoap(int N, uint64_t x, char* s);

// Convert signed int 64 to string. String buffer is at least 22 bytes.
// Returns length
// *** this feels inefficient :( ***
C_API uint32_t simd_itoa(int64_t x, char *buf);
C_API uint32_t itoa8(int32_t x, char *buf);

//// Double to string

// Parse double from string
// *** More than 20 char integer part returns junk.
// *** Too much decimal char will get lost to precision
C_API double simd_atod(const char *s, uint32_t len);
