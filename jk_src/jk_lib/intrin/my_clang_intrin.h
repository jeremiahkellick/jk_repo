typedef double __v2df __attribute__((__vector_size__(16)));
typedef double __v4df __attribute__ ((__vector_size__ (32)));
typedef float __v8sf __attribute__ ((__vector_size__ (32)));
typedef long long __v4di __attribute__ ((__vector_size__ (32)));
typedef int __v8si __attribute__ ((__vector_size__ (32)));
typedef short __v16hi __attribute__ ((__vector_size__ (32)));
typedef char __v32qi __attribute__ ((__vector_size__ (32)));
typedef unsigned long long __v4du __attribute__ ((__vector_size__ (32)));
typedef unsigned int __v8su __attribute__ ((__vector_size__ (32)));
typedef unsigned short __v16hu __attribute__ ((__vector_size__ (32)));
typedef unsigned char __v32qu __attribute__ ((__vector_size__ (32)));

#define __DEFAULT_FN_ATTRS                                                     \
  __attribute__((__always_inline__, __nodebug__, __target__("avx"), \
                 __min_vector_width__(256)))
#define __DEFAULT_FN_ATTRS_CONSTEXPR __DEFAULT_FN_ATTRS
#define __DEFAULT_FN_ATTRS128                                                  \
  __attribute__((__always_inline__, __nodebug__, __target__("fma"),            \
                 __min_vector_width__(128)))
#define __DEFAULT_FN_ATTRS128_CONSTEXPR __DEFAULT_FN_ATTRS128
#define __DEFAULT_FN_ATTRS256                                                  \
  __attribute__((__always_inline__, __nodebug__,                               \
                 __target__("avx2"), __min_vector_width__(256)))
typedef double __m128d __attribute__((__vector_size__(16), __aligned__(16)));
typedef long long __m128i __attribute__((__vector_size__(16), __aligned__(16)));
typedef double __m128d_u __attribute__((__vector_size__(16), __aligned__(1)));
typedef long long __m128i_u
    __attribute__((__vector_size__(16), __aligned__(1)));
typedef float __m256 __attribute__ ((__vector_size__ (32), __aligned__(32)));
typedef long long __m256i __attribute__((__vector_size__(32), __aligned__(32)));
typedef float __m256_u __attribute__ ((__vector_size__ (32), __aligned__(1)));
typedef long long __m256i_u __attribute__((__vector_size__(32), __aligned__(1)));
static __inline __m256i __DEFAULT_FN_ATTRS
_mm256_setzero_si256(void)
{
  return __extension__ (__m256i)(__v4di){ 0, 0, 0, 0 };
}
static __inline __m256i __DEFAULT_FN_ATTRS
_mm256_loadu_si256(__m256i_u const *__p)
{
  struct __loadu_si256 {
    __m256i_u __v;
  } __attribute__((__packed__, __may_alias__));
  return ((const struct __loadu_si256*)__p)->__v;
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_loadu_ps(float const *__p)
{
  struct __loadu_ps {
    __m256_u __v;
  } __attribute__((__packed__, __may_alias__));
  return ((const struct __loadu_ps*)__p)->__v;
}
static __inline void __DEFAULT_FN_ATTRS
_mm256_storeu_si256(__m256i_u *__p, __m256i __a)
{
  struct __storeu_si256 {
    __m256i_u __v;
  } __attribute__((__packed__, __may_alias__));
  ((struct __storeu_si256*)__p)->__v = __a;
}
static __inline__ __m256i __DEFAULT_FN_ATTRS256
_mm256_and_si256(__m256i __a, __m256i __b)
{
  return (__m256i)((__v4du)__a & (__v4du)__b);
}
static __inline__ __m256i __DEFAULT_FN_ATTRS256
_mm256_or_si256(__m256i __a, __m256i __b)
{
  return (__m256i)((__v4du)__a | (__v4du)__b);
}
static __inline __m256i __DEFAULT_FN_ATTRS
_mm256_set_epi32(int __i0, int __i1, int __i2, int __i3,
                 int __i4, int __i5, int __i6, int __i7)
{
  return __extension__ (__m256i)(__v8si){ __i7, __i6, __i5, __i4, __i3, __i2, __i1, __i0 };
}
static __inline __m256i __DEFAULT_FN_ATTRS
_mm256_set1_epi32(int __i)
{
  return _mm256_set_epi32(__i, __i, __i, __i, __i, __i, __i, __i);
}
static __inline__ __m256i __DEFAULT_FN_ATTRS256
_mm256_add_epi32(__m256i __a, __m256i __b)
{
  return (__m256i)((__v8su)__a + (__v8su)__b);
}
static __inline__ __m256i __DEFAULT_FN_ATTRS256
_mm256_sub_epi32(__m256i __a, __m256i __b)
{
  return (__m256i)((__v8su)__a - (__v8su)__b);
}
static __inline__ __m256i __DEFAULT_FN_ATTRS256
_mm256_slli_epi32(__m256i __a, int __count)
{
  return (__m256i)__builtin_ia32_pslldi256((__v8si)__a, __count);
}
static __inline__ __m256i __DEFAULT_FN_ATTRS256
_mm256_srli_epi32(__m256i __a, int __count)
{
  return (__m256i)__builtin_ia32_psrldi256((__v8si)__a, __count);
}
static __inline__ __m256i __DEFAULT_FN_ATTRS256
_mm256_srai_epi32(__m256i __a, int __count)
{
  return (__m256i)__builtin_ia32_psradi256((__v8si)__a, __count);
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_setzero_ps(void)
{
  return __extension__ (__m256){ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_set_ps(float __a, float __b, float __c, float __d,
              float __e, float __f, float __g, float __h)
{
  return __extension__ (__m256){ __h, __g, __f, __e, __d, __c, __b, __a };
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_set1_ps(float __w)
{
  return _mm256_set_ps(__w, __w, __w, __w, __w, __w, __w, __w);
}
static __inline void __DEFAULT_FN_ATTRS
_mm256_storeu_ps(float *__p, __m256 __a)
{
  struct __storeu_ps {
    __m256_u __v;
  } __attribute__((__packed__, __may_alias__));
  ((struct __storeu_ps*)__p)->__v = __a;
}
static __inline__ __m256 __DEFAULT_FN_ATTRS
_mm256_undefined_ps(void)
{
  return (__m256)__builtin_ia32_undef256();
}
/* Compare */
#define _CMP_EQ_OQ    0x00 /* Equal (ordered, non-signaling)  */
#define _CMP_LT_OS    0x01 /* Less-than (ordered, signaling)  */
#define _CMP_LE_OS    0x02 /* Less-than-or-equal (ordered, signaling)  */
#define _CMP_UNORD_Q  0x03 /* Unordered (non-signaling)  */
#define _CMP_NEQ_UQ   0x04 /* Not-equal (unordered, non-signaling)  */
#define _CMP_NLT_US   0x05 /* Not-less-than (unordered, signaling)  */
#define _CMP_NLE_US   0x06 /* Not-less-than-or-equal (unordered, signaling)  */
#define _CMP_ORD_Q    0x07 /* Ordered (non-signaling)   */
#define _CMP_EQ_UQ    0x08 /* Equal (unordered, non-signaling)  */
#define _CMP_NGE_US   0x09 /* Not-greater-than-or-equal (unordered, signaling)  */
#define _CMP_NGT_US   0x0a /* Not-greater-than (unordered, signaling)  */
#define _CMP_FALSE_OQ 0x0b /* False (ordered, non-signaling)  */
#define _CMP_NEQ_OQ   0x0c /* Not-equal (ordered, non-signaling)  */
#define _CMP_GE_OS    0x0d /* Greater-than-or-equal (ordered, signaling)  */
#define _CMP_GT_OS    0x0e /* Greater-than (ordered, signaling)  */
#define _CMP_TRUE_UQ  0x0f /* True (unordered, non-signaling)  */
#define _CMP_EQ_OS    0x10 /* Equal (ordered, signaling)  */
#define _CMP_LT_OQ    0x11 /* Less-than (ordered, non-signaling)  */
#define _CMP_LE_OQ    0x12 /* Less-than-or-equal (ordered, non-signaling)  */
#define _CMP_UNORD_S  0x13 /* Unordered (signaling)  */
#define _CMP_NEQ_US   0x14 /* Not-equal (unordered, signaling)  */
#define _CMP_NLT_UQ   0x15 /* Not-less-than (unordered, non-signaling)  */
#define _CMP_NLE_UQ   0x16 /* Not-less-than-or-equal (unordered, non-signaling)  */
#define _CMP_ORD_S    0x17 /* Ordered (signaling)  */
#define _CMP_EQ_US    0x18 /* Equal (unordered, signaling)  */
#define _CMP_NGE_UQ   0x19 /* Not-greater-than-or-equal (unordered, non-signaling)  */
#define _CMP_NGT_UQ   0x1a /* Not-greater-than (unordered, non-signaling)  */
#define _CMP_FALSE_OS 0x1b /* False (ordered, signaling)  */
#define _CMP_NEQ_OS   0x1c /* Not-equal (ordered, signaling)  */
#define _CMP_GE_OQ    0x1d /* Greater-than-or-equal (ordered, non-signaling)  */
#define _CMP_GT_OQ    0x1e /* Greater-than (ordered, non-signaling)  */
#define _CMP_TRUE_US  0x1f /* True (unordered, signaling)  */
#define _mm256_cmp_ps(a, b, c) \
  ((__m256)__builtin_ia32_cmpps256((__v8sf)(__m256)(a), \
                                   (__v8sf)(__m256)(b), (c)))
#define _mm256_i32gather_ps(m, i, s) \
  ((__m256)__builtin_ia32_gatherd_ps256((__v8sf)_mm256_undefined_ps(), \
                                        (float const *)(m), \
                                        (__v8si)(__m256i)(i), \
                                        (__v8sf)_mm256_cmp_ps(_mm256_setzero_ps(), \
                                                              _mm256_setzero_ps(), \
                                                              _CMP_EQ_OQ), \
                                        (s)))
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_add_ps(__m256 __a, __m256 __b)
{
  return (__m256)((__v8sf)__a+(__v8sf)__b);
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_sub_ps(__m256 __a, __m256 __b)
{
  return (__m256)((__v8sf)__a-(__v8sf)__b);
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_mul_ps(__m256 __a, __m256 __b)
{
  return (__m256)((__v8sf)__a * (__v8sf)__b);
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_div_ps(__m256 __a, __m256 __b)
{
  return (__m256)((__v8sf)__a/(__v8sf)__b);
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_rcp_ps(__m256 __a)
{
  return (__m256)__builtin_ia32_rcpps256((__v8sf)__a);
}
/* SSE4 Rounding macros. */
#define _MM_FROUND_TO_NEAREST_INT 0x00
#define _MM_FROUND_TO_NEG_INF 0x01
#define _MM_FROUND_TO_POS_INF 0x02
#define _MM_FROUND_TO_ZERO 0x03
#define _MM_FROUND_CUR_DIRECTION 0x04
#define _MM_FROUND_RAISE_EXC 0x00
#define _MM_FROUND_NO_EXC 0x08
#define _MM_FROUND_NINT (_MM_FROUND_RAISE_EXC | _MM_FROUND_TO_NEAREST_INT)
#define _MM_FROUND_FLOOR (_MM_FROUND_RAISE_EXC | _MM_FROUND_TO_NEG_INF)
#define _MM_FROUND_CEIL (_MM_FROUND_RAISE_EXC | _MM_FROUND_TO_POS_INF)
#define _MM_FROUND_TRUNC (_MM_FROUND_RAISE_EXC | _MM_FROUND_TO_ZERO)
#define _MM_FROUND_RINT (_MM_FROUND_RAISE_EXC | _MM_FROUND_CUR_DIRECTION)
#define _MM_FROUND_NEARBYINT (_MM_FROUND_NO_EXC | _MM_FROUND_CUR_DIRECTION)
#define _mm256_round_ps(V, M) \
  ((__m256)__builtin_ia32_roundps256((__v8sf)(__m256)(V), (M)))
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_min_ps(__m256 __a, __m256 __b)
{
  return (__m256)__builtin_ia32_minps256((__v8sf)__a, (__v8sf)__b);
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_max_ps(__m256 __a, __m256 __b)
{
  return (__m256)__builtin_ia32_maxps256((__v8sf)__a, (__v8sf)__b);
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_andnot_ps(__m256 __a, __m256 __b)
{
  return (__m256)(~(__v8su)__a & (__v8su)__b);
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_and_ps(__m256 __a, __m256 __b)
{
  return (__m256)((__v8su)__a & (__v8su)__b);
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_or_ps(__m256 __a, __m256 __b)
{
  return (__m256)((__v8su)__a | (__v8su)__b);
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_blendv_ps(__m256 __a, __m256 __b, __m256 __c)
{
  return (__m256)__builtin_ia32_blendvps256(
    (__v8sf)__a, (__v8sf)__b, (__v8sf)__c);
}
static __inline int __DEFAULT_FN_ATTRS
_mm256_testz_ps(__m256 __a, __m256 __b)
{
  return __builtin_ia32_vtestzps256((__v8sf)__a, (__v8sf)__b);
}
static __inline int __DEFAULT_FN_ATTRS
_mm256_testc_ps(__m256 __a, __m256 __b)
{
  return __builtin_ia32_vtestcps256((__v8sf)__a, (__v8sf)__b);
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_castsi256_ps(__m256i __a)
{
  return (__m256)__a;
}
static __inline __m256 __DEFAULT_FN_ATTRS
_mm256_cvtepi32_ps(__m256i __a)
{
  return (__m256)__builtin_convertvector((__v8si)__a, __v8sf);
}
static __inline __m256i __DEFAULT_FN_ATTRS
_mm256_castps_si256(__m256 __a)
{
  return (__m256i)__a;
}
static __inline __m256i __DEFAULT_FN_ATTRS
_mm256_cvttps_epi32(__m256 __a)
{
  return (__m256i)__builtin_ia32_cvttps2dq256((__v8sf) __a);
}
static __inline__ double __DEFAULT_FN_ATTRS _mm_cvtsd_f64(__m128d __a) {
  return __a[0];
}
static __inline__ __m128d __DEFAULT_FN_ATTRS128_CONSTEXPR
_mm_fmadd_sd(__m128d __A, __m128d __B, __m128d __C) {
  __A[0] = __builtin_elementwise_fma(__A[0], __B[0], __C[0]);
  return __A;
}
static __inline__ __m128d __DEFAULT_FN_ATTRS _mm_set_sd(double __w) {
  return __extension__(__m128d){__w, 0};
}
static __inline__ __m128d __DEFAULT_FN_ATTRS_CONSTEXPR _mm_setzero_pd(void) {
  return __extension__(__m128d){0.0, 0.0};
}
static __inline__ __m128d __DEFAULT_FN_ATTRS _mm_sqrt_sd(__m128d __a,
                                                         __m128d __b) {
  return __extension__(__m128d){__builtin_elementwise_sqrt(__b[0]), __a[1]};
}
