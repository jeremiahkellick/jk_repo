typedef union __declspec(intrin_type) __declspec(align(16)) __m128 {
     float               m128_f32[4];
     unsigned __int64    m128_u64[2];
     __int8              m128_i8[16];
     __int16             m128_i16[8];
     __int32             m128_i32[4];
     __int64             m128_i64[2];
     unsigned __int8     m128_u8[16];
     unsigned __int16    m128_u16[8];
     unsigned __int32    m128_u32[4];
 } __m128;

#define _MM_FROUND_TO_NEAREST_INT    0x00
#define _MM_FROUND_TO_NEG_INF        0x01
#define _MM_FROUND_TO_POS_INF        0x02
#define _MM_FROUND_TO_ZERO           0x03
#define _MM_FROUND_CUR_DIRECTION     0x04
#define _MM_FROUND_RAISE_EXC         0x00
#define _MM_FROUND_NO_EXC            0x08
#define _MM_FROUND_NINT      _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_RAISE_EXC
#define _MM_FROUND_FLOOR     _MM_FROUND_TO_NEG_INF     | _MM_FROUND_RAISE_EXC
#define _MM_FROUND_CEIL      _MM_FROUND_TO_POS_INF     | _MM_FROUND_RAISE_EXC
#define _MM_FROUND_TRUNC     _MM_FROUND_TO_ZERO        | _MM_FROUND_RAISE_EXC
#define _MM_FROUND_RINT      _MM_FROUND_CUR_DIRECTION  | _MM_FROUND_RAISE_EXC
#define _MM_FROUND_NEARBYINT _MM_FROUND_CUR_DIRECTION  | _MM_FROUND_NO_EXC
extern __m128  _mm_round_ss(__m128 /* dst */, __m128  /* val */, int /* iRoundMode */);

extern __m128 _mm_setzero_ps(void);
extern __m128 _mm_set_ss(float _A);
extern float _mm_cvtss_f32(__m128 _A);
extern __m128 _mm_sqrt_ss(__m128 _A);

typedef union __declspec(intrin_type) __declspec(align(32)) __m256 {
    float m256_f32[8];
} __m256;
typedef union  __declspec(intrin_type) __declspec(align(32)) __m256i {
    __int8              m256i_i8[32];
    __int16             m256i_i16[16];
    __int32             m256i_i32[8];
    __int64             m256i_i64[4];
    unsigned __int8     m256i_u8[32];
    unsigned __int16    m256i_u16[16];
    unsigned __int32    m256i_u32[8];
    unsigned __int64    m256i_u64[4];
} __m256i;
extern __m256  __cdecl _mm256_round_ps(__m256, int);
extern __m256  __cdecl _mm256_setzero_ps(void);
extern __m256i __cdecl _mm256_setzero_si256(void);
extern __m256i __cdecl _mm256_loadu_si256(__m256i const *);
extern void    __cdecl _mm256_storeu_si256(__m256i *, __m256i);
extern __m256i __cdecl _mm256_and_si256(__m256i, __m256i);
extern __m256i __cdecl _mm256_andnot_si256(__m256i, __m256i);
extern __m256i __cdecl _mm256_or_si256(__m256i, __m256i);
extern __m256i __cdecl _mm256_set1_epi32(int);
extern __m256i __cdecl _mm256_add_epi32(__m256i, __m256i);
extern __m256i __cdecl _mm256_sub_epi32(__m256i, __m256i);
extern __m256i __cdecl _mm256_slli_epi32(__m256i, int);
extern __m256i __cdecl _mm256_srli_epi32(__m256i, int);
extern __m256i __cdecl _mm256_srai_epi32(__m256i, int);
extern __m256  __cdecl _mm256_set1_ps(float);
extern __m256  __cdecl _mm256_loadu_ps(float const *);
extern void    __cdecl _mm256_storeu_ps(float *, __m256);
extern __m256  __cdecl _mm256_i32gather_ps(float  const * /* ptr */,
                                           __m256i        /* vindex */,
                                           const int      /* index_scale */);
extern __m256 __cdecl _mm256_add_ps(__m256, __m256);
extern __m256 __cdecl _mm256_sub_ps(__m256, __m256);
extern __m256 __cdecl _mm256_mul_ps(__m256, __m256);
extern __m256 __cdecl _mm256_div_ps(__m256, __m256);
extern __m256  __cdecl _mm256_rcp_ps(__m256);
extern __m256 __cdecl _mm256_min_ps(__m256, __m256);
extern __m256 __cdecl _mm256_max_ps(__m256, __m256);
extern __m256 __cdecl _mm256_andnot_ps(__m256, __m256);
extern __m256 __cdecl _mm256_and_ps(__m256, __m256);
extern __m256 __cdecl _mm256_or_ps(__m256, __m256);
extern __m256 __cdecl _mm256_andnot_ps(__m256, __m256);

// Compare predicates for scalar and packed compare intrinsic functions
#define _CMP_EQ_OQ     0x00 
#define _CMP_LT_OS     0x01
#define _CMP_LE_OS     0x02
#define _CMP_UNORD_Q   0x03
#define _CMP_NEQ_UQ    0x04
#define _CMP_NLT_US    0x05
#define _CMP_NLE_US    0x06
#define _CMP_ORD_Q     0x07
#define _CMP_EQ_UQ     0x08
#define _CMP_NGE_US    0x09
#define _CMP_NGT_US    0x0A
#define _CMP_FALSE_OQ  0x0B
#define _CMP_NEQ_OQ    0x0C
#define _CMP_GE_OS     0x0D
#define _CMP_GT_OS     0x0E
#define _CMP_TRUE_UQ   0x0F
#define _CMP_EQ_OS     0x10
#define _CMP_LT_OQ     0x11
#define _CMP_LE_OQ     0x12
#define _CMP_UNORD_S   0x13
#define _CMP_NEQ_US    0x14
#define _CMP_NLT_UQ    0x15
#define _CMP_NLE_UQ    0x16
#define _CMP_ORD_S     0x17
#define _CMP_EQ_US     0x18
#define _CMP_NGE_UQ    0x19
#define _CMP_NGT_UQ    0x1A
#define _CMP_FALSE_OS  0x1B
#define _CMP_NEQ_OS    0x1C
#define _CMP_GE_OQ     0x1D
#define _CMP_GT_OQ     0x1E
#define _CMP_TRUE_US   0x1F
extern __m256 __cdecl _mm256_cmp_ps(__m256, __m256, const int);

extern __m256 __cdecl _mm256_blendv_ps(__m256, __m256, __m256);
extern int     __cdecl _mm256_testz_ps(__m256, __m256);
extern int     __cdecl _mm256_testc_ps(__m256, __m256);
extern __m256  __cdecl _mm256_castsi256_ps(__m256i);
extern __m256  __cdecl _mm256_cvtepi32_ps(__m256i);
extern __m256i __cdecl _mm256_castps_si256(__m256);
extern __m256i __cdecl _mm256_cvttps_epi32(__m256);
