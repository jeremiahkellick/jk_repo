#ifndef LF_H
#define LF_H 1

#include <string.h> // memcpy, memset

/// Abbreviated Primitive Types ///
#include <stdint.h>
#include <stdbool.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef intptr_t sptr;
typedef uintptr_t uptr;
typedef float f32;
typedef double f64;

#define s8_MAX 0x7F
#define s8_MIN (-1 - 0x7F)
#define s16_MAX 0x7FFF
#define s16_MIN (-1 - 0x7FFF)
#define s32_MAX (0x7FFFFFFF)
#define s32_MIN (-1 - 0x7FFFFFFF)
#define s64_MAX (0x7FFFFFFFFFFFFFFFll)
#define s64_MIN (-1 - 0x7FFFFFFFFFFFFFFFll)
#define u8_MAX (0xFF)
#define u16_MAX (0xFFFF)
#define u32_MAX (0xFFFFFFFF)
#define u64_MAX (0xFFFFFFFFFFFFFFFFll)
#define sptr_MIN INTPTR_MIN
#define sptr_MAX INTPTR_MAX
#define uptr_MAX UINTPTR_MAX

/// Macros ///
#define ARRAY_LEN(A) (sizeof(A)/sizeof(*(A)))
#define UNUSED(X) do { (void) (X); } while(0) // mark X as used to silence unused var warnings

#if __GNUC__
  #define INLINE __attribute__((always_inline)) inline
#elif _MSC_VER
  #define INLINE __forceinline
#elif !defined(INLINE)
  #error "INLINE undefined!"
#endif

// Math macros
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define ABS(a) (((a) < 0)? -(a) : (a))
#define CLAMP(x,a,b) (((x)<(a))?(a):((b)<(x))?(b):(x))
#define IS_POW2(x) ((x & (x-1)) == 0)
// https://fgiesen.wordpress.com/2012/08/15/linear-interpolation-past-present-and-future/
#define LERP(a, b, t) ((a) + ((b) - (a))*(t))
#define LERP_EX(a, b, t) ((a)*(1-(t)) + (b)*(t)) // guarantees L(0) = a, L(1) = b
#define UNLERP(x, a, b) (((x) - (a))/((b) - (a)))
#define REMAP(x, a, b, c, d) LERP(c, d, UNLERP(x, a, b))
#define APPROACH(x, t, s) (((x) > (t))? MAX((x)-(s), (t)) : MIN((x)+(s), (t)))

// Memory sizes
#define KB(x) ((x) << 10)
#define MB(x) ((x) << 20)
#define GB(x) ((x) << 30)
#define TB(x) ((x) << 40)

// Endianness detection
#if __GNUC__
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	#define IS_LITTLE_ENDIAN (1)
  #else
	#define IS_LITTLE_ENDIAN (0)
  #endif
#elif _MSC_VER
  #define IS_LITTLE_ENDIAN (1)
#elif !defined(IS_LITTLE_ENDIAN)
  #error "IS_LITTLE_ENDIAN is not defined!"
#endif

// Alignment (C/C++ 11 or compiler extensions)
#if defined(__cplusplus) && __cplusplus >= 2011
#define ALIGNOF(T) alignof(T)
#elif defined(__STDC_VERSION) && __STDC_VERSION >= 2011
#define ALIGNOF(T) _Alignof(T)
#elif __GNUC__
#define ALIGNOF(T) __alignof__(T)
#elif _MSC_VER
#define ALIGNOF(T) __alignof(T)
#else
#error "ALIGNOF is not defined!"
#endif

// Defer/with using for loops.
// NOTE: should not use any nonlinear flow control in one of these blocks,
// which includes break, continue, return, goto.
#define DEFER(end) for(int ___i = 1; ___i; ___i = 0, end)
#define WITH(init, close) \
  for (int ___i = 1; ___i; ___i = 0) \
  for (init; ___i; close, ___i = 0)

//// Asserts
// Expression that will be called to stop the program in asserts. can be overriden.
#if __GNUC__
#define DEF_ASSERT_BREAK (__builtin_trap())
#elif _MSC_VER
#define DEF_ASSERT_BREAK (__debugbreak())
#else
#define DEF_ASSERT_BREAK (*(volatile int *)0 = 0)
#endif
		
#ifndef ASSERT_BREAK
#define ASSERT_BREAK DEF_ASSERT_BREAK
#endif // ASSERT_BREAK

// Defines what to do when an assert fails. can be overriden.
#include <stdio.h>
#define DEF_ASSERT_KILL(fmt, ...) do { fprintf(stderr, "" __FILE__ ":%d: " fmt "\n", __LINE__, ##__VA_ARGS__); ASSERT_BREAK; } while (0)
#ifndef ASSERT_KILL
#define ASSERT_KILL DEF_ASSERT_KILL
#endif

// Standard hard assert. Always stops program immediately. Should be used for
// completely invalid/unrecoverable cases.
#define ASSERT(cond, fmt, ...) while (!(cond)) { ASSERT_KILL(fmt, ##__VA_ARGS__); }

// Static check of conditions. Only some conditions can be checked this way, but
// these checks have no run-time cost, which is useful.
// Ex: verify size/alignment of a struct
#define ASSERT_STATIC(C,label) do { \
  u8 static_assert_##label[(C)?(1):(-1)]; \
  (void) static_assert_##label; \
} while(0)

#if __GNUC__
# define EXPECT(expr, val) __builtin_expect((expr), (val))
#else
# define EXPECT(expr, val) (expr)
#endif
#define LIKELY(expr)            EXPECT(expr,1)
#define UNLIKELY(expr)          EXPECT(expr,0)

// Macro macros
#define MACRO_EXPAND(S) #S
#define _MACRO_CONCAT(S1,S2) S1##S2
#define MACRO_CONCAT(S1,S2) _MACRO_CONCAT(S1,S2)
#define MACRO_VAR(var) MACRO_CONCAT(var, __LINE__)

// Allows using default constructor in C++ or designated initializers in C to init
// a struct in a single expression
// Ex: str s = STRUCT(str){ "lf", 2 };
#ifdef __cplusplus
#define STRUCT(type) type
#define STRUCT_ZERO(type) {}
#else
#define STRUCT(type) (type)
#define STRUCT_ZERO(type) (type){0}
#endif

/// Memory ///

/// Arenas ///
typedef struct Arena {
	u8 *buf;
	uptr pos;
	uptr size;
	uptr commit_pos; // for fixed size, should be uptr_MAX
	sptr commit_size;
} Arena;

// Create arenas
#define Arena_buffer(buf) Arena_fixed(buf, sizeof(buf))
Arena Arena_fixed(u8* buf, uptr size);
Arena Arena_alloc(Arena params);
void Arena_destroy(Arena *a); // NOTE(lf): Not needed at program exit

// Allocate
void* Arena_bytes_align(Arena *a, uptr size, uptr align);
void* Arena_bytes_align_nz(Arena *a, uptr size, uptr align);
#define Arena_bytes(a, size) Arena_bytes_align(a, size, 1)
#define Arena_bytes_nz(a, size) Arena_bytes_align_nz(a, size, 1)
#define Arena_array(a, type, len) ((type*) Arena_bytes_align(a, (len)*sizeof(type), ALIGNOF(type)))
#define Arena_array_nz(a, type, len) ((type*) Arena_bytes_align_nz(a, (len)*sizeof(type), ALIGNOF(type)))
#define Arena_struct(a, type) ((type*) Arena_array(a, type, 1))
#define Arena_struct_nz(a, type) ((type*) Arena_array_nz(a, type, 1))

// Free memory
void Arena_reset(Arena *a, uptr pos);
void Arena_free(Arena* a, void *prev_allocation); // Reset arena to before prev_allocation

// Macro for doing temp allocations in the following scope
#define ARENA_TEMP(a) WITH(uptr MACRO_VAR(pos) = (a).pos, (a).pos = MACRO_VAR(pos))

#ifdef LF_IMPL
#ifdef __linux__ // detect linux, use virtual mem
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#define LF_MEMORY_RESERVE(size) mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, (off_t)0)
#define LF_MEMORY_COMMIT(ptr, size) (mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0)
#define LF_MEMORY_FREE(ptr, size) do { munmap(ptr, size); } while (0)
#define LF_MEMORY_DECOMMIT(ptr, size) do { mprotect(ptr, size, PROT_NONE); madvise(ptr, size, MADV_DONTNEED); } while (0)
#define LF_MEMORY_VIRTUAL 1
#else // fallback on malloc/free, no VMEM
#include <stdlib.h>
#define LF_MEMORY_RESERVE(size) malloc(size)
#define LF_MEMORY_COMMIT(ptr, size) (1)
#define LF_MEMORY_FREE(ptr, size) (free(size))
#define LF_MEMORY_DECOMMIT(ptr, size)
#define LF_MEMORY_VIRTUAL 0
#endif

#define LF_ARENA_FIXED -1

Arena Arena_fixed(u8* buf, uptr size) {
	return STRUCT(Arena){ buf, 0, size, size, LF_ARENA_FIXED};
}

static inline uptr next_align(u8* mem, uptr offset, uptr align) {
	ASSERT(IS_POW2(align), "align must be a power of 2.");
	uptr ptr = ((uptr)mem) + offset;
	/* Fast replacement for mod because align is power of 2 */
	uptr modulo = ptr & (align-1);
	if (modulo != 0) {
		ptr += align - modulo;
	}
	return ptr - ((uptr) mem);
}

Arena Arena_alloc(Arena params) {
	if (params.commit_size <= 0) { params.commit_size = KB(4); }

	if (LF_MEMORY_VIRTUAL) {
		ASSERT(IS_POW2(params.commit_size),
			"Arena_new: for virtual mem params.commit_size must be power of 2");
		ASSERT(next_align(0, params.size, params.commit_size) == params.size,
			"Arena_new: for virtual mem params.size must be a multiple of "
			"params.commit_size");
	}
	params.buf = (u8*) LF_MEMORY_RESERVE(params.size);
	ASSERT(params.buf, "Arena_new: Failed to reserve memory!");
	bool commited = LF_MEMORY_COMMIT(params.buf, params.commit_size);
	UNUSED(commited);
	ASSERT(commited, "Arena_new: Failed to commit memory!");
	params.commit_pos = params.commit_size;
	return params;
}

void Arena_destroy(Arena *a) {
	if (a->commit_size > 0) {
		LF_MEMORY_FREE(a->buf, a->size);
	}
	a->buf = 0;
}

void* Arena_bytes_align_nz(Arena *a, uptr size, uptr align) {
	void* result = 0;

	// Align pos pointer
	uptr aligned_pos = next_align(a->buf, a->pos, align);
	uptr new_pos = aligned_pos + size;

	// Check that there is space
	if (new_pos <= a->size) {
		// Commit memory if needed
		bool in_commit_range = a->commit_size < 0 || new_pos <= a->commit_pos;
		if (!in_commit_range) {
			uptr new_commit_pos = next_align(a->buf, new_pos, a->commit_size);
			in_commit_range = LF_MEMORY_COMMIT(a->buf, new_commit_pos);
			if (in_commit_range) {
				a->commit_pos = new_commit_pos;
			} else {
				ASSERT(0, "Arena failed to commit memory!");
			}
		}
		if (in_commit_range) {
			result = a->buf + aligned_pos;
			a->pos = new_pos;
		}
	}
	ASSERT(result, "Arena out of memory!"); // Arena out of memory!
	return result;
}

void *Arena_bytes_align(Arena *a, uptr size, uptr align) {
	void *mem = Arena_bytes_align_nz(a, size, align);
	memset(mem, 0, size);
	return mem;
}

void Arena_reset(Arena *a, uptr pos) {
	ASSERT(pos <= a->pos, "Arena_reset: should be reset to pos <= a->pos!");
	// Clear memory in DEV build to make use-after-free more obvious
	#ifdef LF_DEV
	if (pos) {
		memset(a->buf + pos, 0xDD, a->pos - pos);
	}
	#endif // LF_DEV
	a->pos = pos;
}

void Arena_decommit(Arena *a, uptr pos) {
	// Exit if attempt to decommit fixed size arena
	if (a->commit_size < 0) return;
	// By default decommit everything not used, leaving atleast one page
	if (!pos) {
		pos = a->commit_size;
	}
	// Never decommit used memory
	pos = MAX(pos, a->pos);

	uptr new_commit_pos = next_align(a->buf, pos, a->commit_size);
	uptr over_commited = a->commit_pos - new_commit_pos;
	if (over_commited >= (uptr) a->commit_size) {
		LF_MEMORY_DECOMMIT(a->buf+new_commit_pos, over_commited);
		a->commit_pos = new_commit_pos;
	}
}

void Arena_free(Arena *a, void *previous_alloc) {
	ASSERT(previous_alloc, "Arena_free: previous alloc should be non-null!");
	uptr pos = (u8*) previous_alloc - a->buf;
	ASSERT(pos < a->size, "Arena_free: previous alloc must be in arena buf!");
	Arena_reset(a, pos);
}
#endif // LF_IMPL

u8 char_lower(u8 c);
u8 char_upper(u8 c);

bool char_is_ws(u8 c);
bool char_is_letter(u8 c);
bool char_is_num(u8 c);
bool char_is_alphanum(u8 c);

// Str
typedef struct str {
	u8 *str;
	sptr len;
} str;

#define strl(literal) STRUCT(str){(u8*)literal, sizeof(literal"") - 1}
str strc(char *cstring); // does a strlen

str str_first(str s, sptr n); // s[0, n)
str str_skip(str s, sptr n);  // s[n, s.len)
str str_trim(str s, sptr n);   // s[0, s.len-n)
str str_last(str s, sptr n);  // s[s.len-n, s.len)

str str_skip_start(str s, str prefix);
str str_trim_end(str s, str suffix);
#define str_skip_startl(s, l) str_skip_start(s, strl(l))
#define str_trim_endl(s, l) str_trim_end(s, strl(l))

str str_skip_ws(str s);
str str_trim_ws(str s);
#define str_strip_ws(s) str_trim_ws(str_skip_ws(s))

#define str_empty(s) ((!(s).str) || ((s).len == 0))
bool str_eq(str s, str b); // Checks that contents of strings match exactly
bool str_start(str s, str prefix);
bool str_end(str s, str suffix);
#define str_eql(s,l) str_eq(s, strl(l))
#define str_startl(s,l) str_start(s, strl(l))
#define str_endl(s,l) str_end(s, strl(l))

// NOTE(lf): all the following functions return >= 0 for a location, or -1 if not found
sptr str_find_dif(str s, str b);
sptr str_find_char(str s, u8 c);
sptr str_find_sub(str s, str sub);
sptr str_find_delims(str s, str delims); // treats delims as array of chars to match any of

// NOTE(lf): all cut methods return the first part of a string, and advance src past that part
str str_cut(str *src, sptr index, sptr skip);
str str_cut_char(str *src, u8 c);
str str_cut_sub(str *src, str sub);
#define str_cut_subl(ps, l) str_cut_sub((ps), strl(l));
str str_cut_delims(str *src, str delims);

#define str_iter(i, c, s) \
	if (!!(s).str) \
	for (sptr i = 0; i < (s).len; i++) \
	for (u8 c = (s).str[i], __c = 1; __c; __c = 0)

// Allocating ops
str strf(Arena *a, const char *fmt, ...);
str str_sized(Arena *a, sptr size);
str str_copy(Arena *a, str s);
str str_concat(Arena *a, str s1, str s2);
char* str_cstring(Arena *a, str s); // Adds null terminator

// strp_int
// - returns parsed integer or 0 if invalid. 
//   The returned value is unsigned but will be cast to a signed value correctly.
// - if valid != null, *valid == true if ws strip'd str represents 
//   an integer in base 8, 10, or 16. 
// - if valid and base != null, writes base of number.
// - if *base == 8, 10, 16, the selected base will be used
//   for parsing and could fail even if it would be valid with *base == 0
// - Integers greater than uptr_MAX will overflow
// - Octal numbers can be prefixed with 0 or 0o.
uptr strp_int(str s, s32 *base, bool *valid);

// Misc
// the purpose of these hashes is that it is easy to implement them in any
// language, so they are good for example if doing codegen that needs to 
// bridge across a C runtime and Python script.
u32 str_hash_fnv1a(str s, u32 current);
u64 str_hash_fnv1a_u64(str s, u64 current);

#ifdef LF_IMPL

u8 char_lower(u8 c) {
	return (c >= 'A' && c <= 'Z') ? (c | 0x20) : c;
}
u8 char_upper(u8 c) {
	return (c >= 'a' && c <= 'z') ? (c &~0x20) : c;
}
bool char_is_ws(u8 c) {
	return ((c == ' ') || (c == '\n') || (c == '\t')
					  || (c == '\r') || (c == '\v'));
}
bool char_is_letter(u8 c) {
	return ((unsigned)c|32) - 'a' < 26;
}
bool char_is_num(u8 c) {
	return ((unsigned)c) - '0' < 10;
}
bool char_is_alphanum(u8 c) {
	return (((unsigned)c|32) - 'a' < 26) || (((unsigned)c) - '0' < 10);
}

str strc(char *cstring) {
	str out = STRUCT_ZERO(str);
	if (!cstring) return out;
	out.str = (u8*)cstring;
	out.len = strlen(cstring);
	return out;
}

str str_first(str s, sptr len) {
	len = MIN(len, s.len);
	return STRUCT(str){s.str, len};
}
str str_skip(str s, sptr len) {
	len = MIN(len, s.len);
	return STRUCT(str){s.str + len, (s.len - len)};
}
str str_trim(str s, sptr len) {
	len = MIN(len, s.len);
	return STRUCT(str){s.str, s.len - len};
}
str str_last(str s, sptr len) {
	len = MIN(len, s.len);
	return STRUCT(str){s.str + s.len - len, len};
}

str str_skip_start(str s, str prefix) {
	if (str_start(s, prefix)) {
		s.str += prefix.len;
		s.len -= prefix.len;
	}
	return s;
}
str str_trim_end(str s, str suffix) {
	if (str_end(s, suffix)) {
		s.len -= suffix.len;
	}
	return s;
}
str str_skip_ws(str s) {
	if (s.str) {
		while((s.len > 0) && char_is_ws(s.str[0])) {
			s.str++; s.len--;
		}
	}
	return s;
}
str str_trim_ws(str s) {
	if (s.str) {
		while((s.len > 0) && char_is_ws(s.str[s.len-1])) {
			s.len--;
		}
	}
	return s;
}

bool str_eq(str s, str b) {
	if (str_empty(s)) return str_empty(b);
	return ((s.len == b.len) && (memcmp(s.str, b.str, b.len) == 0));
}
bool str_start(str s, str prefix) {
	if (str_empty(s)) return str_empty(prefix);
	return (prefix.len <= s.len) && (memcmp(s.str, prefix.str, prefix.len) == 0);
}
bool str_end(str s, str suffix) {
	if (str_empty(s)) return str_empty(suffix);
	sptr offset = s.len - suffix.len;
	return (offset > 0) && (memcmp(s.str+offset, suffix.str, suffix.len) == 0);
}

sptr str_find_dif(str s, str b) {
	if (str_empty(s)) return str_empty(b)? -1 : 0;
	sptr len = MIN(s.len, b.len);
	for (sptr i = 0; i < len; i++) {
		if (s.str[i] != b.str[i]) {
			return i;
		}
	}
	return (s.len != b.len)? len : -1;
}
sptr str_find_char(str s, u8 find) {
	str_iter(i,c, s) {
		if (c == find) return i;
	}
	return -1;
}
sptr str_find_sub(str s, str sub) {
	if (str_empty(s) || str_empty(sub)) return -1;
	sptr match = 0;
	str_iter(i,c, s) {
		if (c == sub.str[match]) {
			if (++match == sub.len) {
				return i+1-match;
			}
		} else {
			i += match;
			match = 0;
		}
	}
	return -1;
}
sptr str_find_delims(str s, str delims) {
	if (str_empty(s) || str_empty(delims)) return -1;
	str_iter(i,c, s) {
		str_iter(j,d, delims) {
			if (c == d) {
				return i;
			}
		}
	}
	return -1;
}

str str_cut(str *src, sptr index, sptr skip) {
	str out = *src;
	if (index >= 0) {
		*src = str_skip(*src, index+skip);
		out.len = MIN(index, out.len);
	} else {
		src->len = 0;
	}
	return out;
}
str str_cut_char(str *src, u8 c) {
	return str_cut(src, str_find_char(*src, c), 1);
}
str str_cut_sub(str *src, str sub) {
	return str_cut(src, str_find_sub(*src, sub), sub.len);
}
str str_cut_delims(str *src, str delims) {
	return str_cut(src, str_find_delims(*src, delims), 1);
}

#include <stdarg.h>
static str strfv(Arena *a, const char *fmt, va_list args) {
	str result = STRUCT_ZERO(str);
	va_list args2;
	va_copy(args2, args);
	result.len = vsnprintf(0, 0, fmt, args2);
	result.str = (u8 *)Arena_bytes_align(a, result.len+1, 1);
	vsnprintf((char *)result.str, (s32)result.len+1, fmt, args);
	return result;
}
str strf(Arena *a, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	str result = strfv(a, fmt, args);
	va_end(args);
	return result;
}
str str_sized(Arena *a, sptr size) {
	str out = STRUCT(str) { (u8 *)Arena_bytes_align(a, size, 1), size };
	return out;
}
str str_copy(Arena *a, str s) {
	if (str_empty(s)) return s;
	str out = STRUCT(str) { (u8 *)Arena_bytes_align(a, s.len, 1), s.len };
	memcpy(out.str, s.str, s.len);
	return out;
}
str str_concat(Arena *a, str s1, str s2) {
	if (str_empty(s1)) return str_copy(a, s2);
	str out = s1;
	// If s1 is not the most recent alloc, copy it
	u8* s1_end = s1.str + s1.len;
	u8* a_end = a->buf + a->pos;
	if (s1_end != a_end) {
		out = str_copy(a, s1);
	}
	// append s2
	if (str_empty(s2)) return out;
	Arena_bytes_align(a, s2.len, 1);
	memcpy(out.str + out.len, s2.str, s2.len);
	out.len += s2.len;
	return out;
}
char* str_cstring(Arena *a, str s) {
	if (str_empty(s)) return (char*) "";
	char *out = (char*) Arena_bytes_align(a, s.len+1, 1);
	memcpy(out, s.str, s.len);
	out[s.len] = '\0';
	return out;
}

INLINE s8 _strb16_i16_hdigit(u8 c) {
	u8 d;
	d = c - '0'; if (d < 10) return d;
	d = (c | 32) - 'a'; if (d < 6) return d+10;
	return -1;
}
uptr strp_int(str s, s32 *base, bool *valid) {
	s = str_strip_ws(s);
	s32 b = base? *base : 0;
	uptr x = 0;
	bool neg = false;

	if (s.len > 0 && (s.str[0] == '+' || s.str[0] == '-')) {
		neg = s.str[0] == '-';
		s.str++; s.len--;
	}

	bool pre8 = str_start(s, strl("0"));
	bool pre16 = str_start(s, strl("0x"));

	if ((b == 16) || (b == 0 && pre16)) {
		if (pre16) { // second side of || above
			s.str += 2; s.len -= 2;
		}
		if (UNLIKELY(s.len == 0)) goto invalid;
		for (sptr i = 0; i < s.len; i++) {
			if (UNLIKELY(_strb16_i16_hdigit(s.str[i]) == -1)) goto invalid;
		} 
		b = 16;
		for (sptr i = 0; i < s.len; i++) {
			x = 16*x + _strb16_i16_hdigit(s.str[i]);
		}
	} else if ((b == 8) || (b == 0 && pre8)) {
	 	// NOTE(lf) skip non-standard octal prefix if present	
		s = str_skip_startl(s, "0o");
		if (UNLIKELY(s.len == 0)) goto invalid;
		for (sptr i = 0; i < s.len; i++) {
			if (UNLIKELY((u8)(s.str[i] - '0') >= 8)) goto invalid;
		} 
		b = 8;
		for (sptr i = 0; i < s.len; i++) {
			x = 8*x + (s.str[i] - '0');
		}
	} else if (b == 10 || b == 0) {
		if (UNLIKELY(s.len == 0)) goto invalid;
		for (sptr i = 0; i < s.len; i++) {
			if (UNLIKELY((u8)(s.str[i] - '0') >= 10)) goto invalid;
		} 
		b = 10;
		for (sptr i = 0; i < s.len; i++) {
			x = 10*x + (s.str[i] - '0');
		}
	} else {
		goto invalid;
	}
	if (base) *base = b;
  if (valid) *valid = true;
	return neg? (uptr)(-((sptr) x)) : x;

invalid:
	if (valid) *valid = false;
	return 0;
}

u32 str_hash_fnv1a(str s, u32 current) {
	u32 hash = current? current : 0x811c9dc5;
	str_iter(i, c, s) {
		c = (c - 'A') < 26? (c | 32) : c; // Convert char to lowercase
		hash = (hash ^ (u32)(c)) * 0x01000193;
	}
	return hash;
}
u64 str_hash_fnv1a_u64(str s, u64 current) {
	u64 hash = current? current : 0xcbf29ce484222325;
	str_iter(i, c, s) {
		c = (c - 'A') < 26? (c | 32) : c; // Convert char to lowercase
		hash = (hash ^ (u32)(c)) * 0x100000001b3;
	}
	return hash;
}
#endif // LF_IMPL
#endif // LF_H

