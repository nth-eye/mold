#ifndef MOLD_UTIL_HALF_H
#define MOLD_UTIL_HALF_H

/**
 * @file
 * @brief Software IEEE 754 half-precision floating-point type.
 *
 * Provides `mold::half_t`, a 2-byte trivially-copyable type that is
 * `bit_cast`-compatible with `uint16_t`. Used as a fallback when neither
 * `<stdfloat>` nor the compiler-intrinsic `_Float16` is available.
 */

#include <cstdint>
#include <bit>
#include <limits>
#include <compare>

namespace mold {
namespace detail {

/**
 * @brief Sign-extend bit 31 to all bits (branchless: 0x00000000 or 0xffffffff).
 *
 * @param a Value whose sign bit is extended
 * @return 0x00000000 if MSB is clear, 0xffffffff if set
 */
constexpr uint32_t u32_ext(uint32_t a)
{
    return uint32_t(int32_t(a) >> 31);
}

/**
 * @brief Branchless select: returns `a` if MSB of `test` is set, `b` otherwise.
 *
 * @param test Bit to test
 * @param a Value to return if `test` is set
 * @param b Value to return if `test` is not set
 * @return `a` if `test` is set, `b` otherwise
 */
constexpr uint32_t u32_sels(uint32_t test, uint32_t a, uint32_t b)
{
    const uint32_t mask = u32_ext(test);
    return (a & mask) | (b & ~mask);
}

/**
 * @brief Convert IEEE 754 binary32 bit pattern to binary16 (branchless).
 *
 * @param f Raw bits of a 32-bit float (`bit_cast<uint32_t>(float_val)`)
 * @return Raw bits of the corresponding 16-bit half
 */
constexpr uint16_t float_to_half(uint32_t f)
{
    constexpr uint32_t one                      = 0x00000001;
    constexpr uint32_t f_s_mask                 = 0x80000000;
    constexpr uint32_t f_e_mask                 = 0x7f800000;
    constexpr uint32_t f_m_mask                 = 0x007fffff;
    constexpr uint32_t f_m_hidden_bit           = 0x00800000;
    constexpr uint32_t f_m_round_bit            = 0x00001000;
    constexpr uint32_t f_snan_mask              = 0x7fc00000;
    constexpr uint32_t f_e_pos                  = 0x00000017;
    constexpr uint32_t h_e_pos                  = 0x0000000a;
    constexpr uint32_t h_e_mask                 = 0x00007c00;
    constexpr uint32_t h_snan_mask              = 0x00007e00;
    constexpr uint32_t h_e_mask_value           = 0x0000001f;
    constexpr uint32_t f_h_s_pos_offset         = 0x00000010;
    constexpr uint32_t f_h_bias_offset          = 0x00000070;
    constexpr uint32_t f_h_m_pos_offset         = 0x0000000d;
    constexpr uint32_t h_nan_min                = 0x00007c01;
    constexpr uint32_t f_h_e_biased_flag        = 0x0000008f;

    const uint32_t f_s                          = f & f_s_mask;
    const uint32_t f_e                          = f & f_e_mask;
    const uint16_t h_s                          = f_s >> f_h_s_pos_offset;
    const uint32_t f_m                          = f & f_m_mask;
    const uint16_t f_e_amount                   = f_e >> f_e_pos;
    const uint32_t f_e_half_bias                = f_e_amount - f_h_bias_offset;
    const uint32_t f_snan                       = f & f_snan_mask;
    const uint32_t f_m_round_mask               = f_m & f_m_round_bit;
    const uint32_t f_m_round_offset             = f_m_round_mask << one;
    const uint32_t f_m_rounded                  = f_m + f_m_round_offset;
    const uint32_t f_m_denorm_sa                = one - f_e_half_bias;
    const uint32_t f_m_with_hidden              = f_m_rounded | f_m_hidden_bit;
    const uint32_t f_m_denorm                   = f_m_with_hidden >> (f_m_denorm_sa & 31);
    const uint32_t h_m_denorm                   = f_m_denorm >> f_h_m_pos_offset;
    const uint32_t f_m_rounded_overflow         = f_m_rounded & f_m_hidden_bit;
    const uint32_t m_nan                        = f_m >> f_h_m_pos_offset;
    const uint32_t h_em_nan                     = h_e_mask | m_nan;
    const uint32_t h_e_norm_overflow_offset     = f_e_half_bias + 1;
    const uint32_t h_e_norm_overflow            = h_e_norm_overflow_offset << h_e_pos;
    const uint32_t h_e_norm                     = f_e_half_bias << h_e_pos;
    const uint32_t h_m_norm                     = f_m_rounded >> f_h_m_pos_offset;
    const uint32_t h_em_norm                    = h_e_norm | h_m_norm;
    const uint32_t is_h_ndenorm_msb             = f_h_bias_offset - f_e_amount;
    const uint32_t is_f_e_flagged_msb           = f_h_e_biased_flag - f_e_half_bias;
    const uint32_t is_h_denorm_msb              = ~is_h_ndenorm_msb;
    const uint32_t is_f_m_eqz_msb               = f_m - 1;
    const uint32_t is_h_nan_eqz_msb             = m_nan - 1;
    const uint32_t is_f_inf_msb                 = is_f_e_flagged_msb & is_f_m_eqz_msb;
    const uint32_t is_f_nan_underflow_msb       = is_f_e_flagged_msb & is_h_nan_eqz_msb;
    const uint32_t is_e_overflow_msb            = h_e_mask_value - f_e_half_bias;
    const uint32_t is_h_inf_msb                 = is_e_overflow_msb | is_f_inf_msb;
    const uint32_t is_f_nsnan_msb               = f_snan - f_snan_mask;
    const uint32_t is_m_norm_overflow_msb       = -f_m_rounded_overflow;
    const uint32_t is_f_snan_msb                = ~is_f_nsnan_msb;
    const uint32_t h_em_overflow_result         = u32_sels(is_m_norm_overflow_msb, h_e_norm_overflow, h_em_norm);
    const uint32_t h_em_nan_result              = u32_sels(is_f_e_flagged_msb, h_em_nan, h_em_overflow_result);
    const uint32_t h_em_nan_underflow_result    = u32_sels(is_f_nan_underflow_msb, h_nan_min, h_em_nan_result);
    const uint32_t h_em_inf_result              = u32_sels(is_h_inf_msb, h_e_mask, h_em_nan_underflow_result);
    const uint32_t h_em_denorm_result           = u32_sels(is_h_denorm_msb, h_m_denorm, h_em_inf_result);
    const uint32_t h_em_snan_result             = u32_sels(is_f_snan_msb, h_snan_mask, h_em_denorm_result);
    const uint32_t h_result                     = h_s | h_em_snan_result;

    return uint16_t(h_result);
}

/**
 * @brief Convert IEEE 754 binary16 bit pattern to binary32 (branchless).
 *
 * @param h Raw bits of a 16-bit half
 * @return Raw bits of the corresponding 32-bit float (`bit_cast<float>(result)`)
 */
constexpr uint32_t half_to_float(uint16_t h)
{
    constexpr uint32_t h_e_mask             = 0x00007c00;
    constexpr uint32_t h_m_mask             = 0x000003ff;
    constexpr uint32_t h_s_mask             = 0x00008000;
    constexpr uint32_t h_f_s_pos_offset     = 0x00000010;
    constexpr uint32_t h_f_e_pos_offset     = 0x0000000d;
    constexpr uint32_t h_f_bias_offset      = 0x0001c000;
    constexpr uint32_t f_e_mask             = 0x7f800000;
    constexpr uint32_t f_m_mask             = 0x007fffff;
    constexpr uint32_t h_f_e_denorm_bias    = 0x0000007e;
    constexpr uint32_t h_f_m_denorm_sa_bias = 0x00000008;
    constexpr uint32_t f_e_pos              = 0x00000017;
    constexpr uint32_t h_e_mask_minus_one   = 0x00007bff;

    const uint32_t h_e                      = h & h_e_mask;
    const uint32_t h_m                      = h & h_m_mask;
    const uint32_t h_s                      = h & h_s_mask;
    const uint32_t h_e_f_bias               = h_e + h_f_bias_offset;
    const uint32_t h_m_nlz                  = std::countl_zero(h_m);
    const uint32_t f_s                      = h_s << h_f_s_pos_offset;
    const uint32_t f_e                      = h_e_f_bias << h_f_e_pos_offset;
    const uint32_t f_m                      = h_m << h_f_e_pos_offset;
    const uint32_t f_em                     = f_e | f_m;
    const uint32_t h_f_m_sa                 = h_m_nlz - h_f_m_denorm_sa_bias;
    const uint32_t f_e_denorm_unpacked      = h_f_e_denorm_bias - h_f_m_sa;
    const uint32_t h_f_m                    = h_m << h_f_m_sa;
    const uint32_t f_m_denorm               = h_f_m & f_m_mask;
    const uint32_t f_e_denorm               = f_e_denorm_unpacked << f_e_pos;
    const uint32_t f_em_denorm              = f_e_denorm | f_m_denorm;
    const uint32_t f_em_nan                 = f_e_mask | f_m;
    const uint32_t is_e_eqz_msb             = h_e - 1;
    const uint32_t is_m_nez_msb             = -h_m;
    const uint32_t is_e_flagged_msb         = h_e_mask_minus_one - h_e;
    const uint32_t is_zero_msb              = is_e_eqz_msb & ~is_m_nez_msb;
    const uint32_t is_inf_msb               = is_e_flagged_msb & ~is_m_nez_msb;
    const uint32_t is_denorm_msb            = is_m_nez_msb & is_e_eqz_msb;
    const uint32_t is_nan_msb               = is_e_flagged_msb & is_m_nez_msb;
    const uint32_t is_zero                  = u32_ext(is_zero_msb);
    const uint32_t f_zero_result            = f_em & ~is_zero;
    const uint32_t f_denorm_result          = u32_sels(is_denorm_msb, f_em_denorm, f_zero_result);
    const uint32_t f_inf_result             = u32_sels(is_inf_msb, f_e_mask, f_denorm_result);
    const uint32_t f_nan_result             = u32_sels(is_nan_msb, f_em_nan, f_inf_result);
    const uint32_t f_result                 = f_s | f_nan_result;

    return f_result;
}

}

/**
 * @brief Software IEEE 754 half-precision (binary16) floating-point type.
 *
 * Stores a 16-bit value in IEEE 754 format. Trivially copyable and
 * `bit_cast`-compatible with `uint16_t`. All arithmetic is performed
 * by promoting to `float`, so this is a storage/interchange type.
 */
struct half_t {

    constexpr half_t() = default;
    constexpr half_t(float f) : bits(detail::float_to_half(std::bit_cast<uint32_t>(f))) {}
    constexpr half_t(double d) : half_t(float(d)) {}

    constexpr operator float() const
    {
        return std::bit_cast<float>(detail::half_to_float(bits));
    }

    explicit constexpr operator double() const
    {
        return double(float(*this));
    }

    friend constexpr bool operator==(half_t a, half_t b) 
    { 
        return float(a) == float(b);
    }
    friend constexpr auto operator<=>(half_t a, half_t b) 
    { 
        return float(a) <=> float(b); 
    }

    friend constexpr bool operator==(half_t a, float b) 
    { 
        return float(a) == b; 
    }

    friend constexpr auto operator<=>(half_t a, float b) 
    { 
        return float(a) <=> b; 
    }

    friend constexpr bool operator==(half_t a, double b) 
    { 
        return double(float(a)) == b; 
    }

    friend constexpr auto operator<=>(half_t a, double b) 
    { 
        return double(float(a)) <=> b; 
    }
private:
    uint16_t bits = 0;
};

static_assert(sizeof(half_t) == 2, "half_t must be 2 bytes");
static_assert(std::is_trivially_copyable_v<half_t>, "half_t must be trivially copyable");

}

/**
 * @brief `std::numeric_limits` specialization for IEEE 754 binary16.
 *
 * Constants match the half-precision format: 1 sign bit, 5 exponent bits,
 * 10 significand bits (11 digits including the implicit leading bit).
 */
template<>
struct std::numeric_limits<mold::half_t> {
    static constexpr auto is_specialized    = true;
    static constexpr auto is_signed         = true;
    static constexpr auto is_integer        = false;
    static constexpr auto is_exact          = false;
    static constexpr auto has_infinity      = true;
    static constexpr auto has_quiet_NaN     = true;
    static constexpr auto has_signaling_NaN = true;
    static constexpr auto digits            = 11;    ///< Significand bits (including implicit leading bit).
    static constexpr auto digits10          = 3;     ///< Decimal digits of precision.
    static constexpr auto max_digits10      = 5;     ///< Decimal digits needed for round-trip.
    static constexpr auto max_exponent      = 16;    ///< Maximum binary exponent.
    static constexpr auto min_exponent      = -13;   ///< Minimum binary exponent (normalized).

    static constexpr auto min()         { return std::bit_cast<mold::half_t>(uint16_t(0x0400)); }
    static constexpr auto max()         { return std::bit_cast<mold::half_t>(uint16_t(0x7bff)); }
    static constexpr auto lowest()      { return std::bit_cast<mold::half_t>(uint16_t(0xfbff)); }
    static constexpr auto epsilon()     { return std::bit_cast<mold::half_t>(uint16_t(0x1400)); }
    static constexpr auto infinity()    { return std::bit_cast<mold::half_t>(uint16_t(0x7c00)); }
};

#endif
