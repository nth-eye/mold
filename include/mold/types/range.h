#ifndef MOLD_TYPES_RANGE_H
#define MOLD_TYPES_RANGE_H

/**
 * @file
 * @brief Compile-time range-checked integer and floating-point proxy types.
 */

#include <type_traits>
#include "mold/refl/spec.h"

namespace mold {

/**
 * @brief Proxy object to store integer within given compile-time range.
 *
 * Automatically selects the smallest type with correct signedness.
 *
 * @tparam Min The minimum value of the range (inclusive).
 * @tparam Max The maximum value of the range (inclusive).
 */
template<int64_t Min, int64_t Max>
struct irange_t {
    static_assert(Min <= Max, "Min value in irange_t cannot be greater than Max value.");
    using value_type = std::conditional_t<Min >= 0,
        std::conditional_t<Max <= std::numeric_limits< uint8_t>::max(), uint8_t,
        std::conditional_t<Max <= std::numeric_limits<uint16_t>::max(), uint16_t,
        std::conditional_t<Max <= std::numeric_limits<uint32_t>::max(), uint32_t,
        uint64_t>>>,
        std::conditional_t<Min >= std::numeric_limits< int8_t>::min() && Max <= std::numeric_limits< int8_t>::max(), int8_t,
        std::conditional_t<Min >= std::numeric_limits<int16_t>::min() && Max <= std::numeric_limits<int16_t>::max(), int16_t,
        std::conditional_t<Min >= std::numeric_limits<int32_t>::min() && Max <= std::numeric_limits<int32_t>::max(), int32_t,
        int64_t>>>>;
    constexpr irange_t() = default;
    constexpr irange_t(value_type value) : value_(value) {}
    constexpr value_type value() const { return value_; }
    constexpr operator value_type() const { return value(); }
private:
    value_type value_ = 0;
};

/**
 * @brief spec_t specialization for bounded integer ranges.
 *
 * @tparam Min Lower bound (inclusive)
 * @tparam Max Upper bound (inclusive)
 */
template<int64_t Min, int64_t Max>
struct spec_t<irange_t<Min, Max>> {

    using value_type = typename irange_t<Min, Max>::value_type;

    static constexpr json_type_t json_type = json_type_t::integer;
    static constexpr cbor_type_t cbor_type = cbor_type_t::integer;

    static error_t read(irange_t<Min, Max>& out, const io_value_t& val)
    {
        int64_t v = val.integer();
        if (v < Min || v > Max) {
            MOLD_DEBUG_LOG("FAIL: Integer value %ld is out of range [%ld, %ld].", v, Min, Max);
            return error_t::handler_failure;
        }
        out = value_type(v);
        return error_t::ok;
    }

    static void emit(const irange_t<Min, Max>& in, const io_sink_t& sink)
    {
        if constexpr (std::is_signed_v<value_type>) {
            sink.write_sint(int64_t(in));
        } else {
            sink.write_uint(uint64_t(in));
        }
    }
};

/**
 * @brief Proxy object to store floating-point number within given compile-time range.
 *
 * Automatically selects the smallest type that can hold the range.
 *
 * @tparam Min The minimum value of the range (inclusive).
 * @tparam Max The maximum value of the range (inclusive).
 */
template<double Min, double Max>
struct frange_t {
    static_assert(Min <= Max, "Min value in frange_t cannot be greater than Max value.");
    using value_type =
        std::conditional_t<Min >= std::numeric_limits<mold::float16_t>::lowest() && Max <= std::numeric_limits<mold::float16_t>::max(), mold::float16_t,
        std::conditional_t<Min >= std::numeric_limits<mold::float32_t>::lowest() && Max <= std::numeric_limits<mold::float32_t>::max(), mold::float32_t,
        mold::float64_t>>;
    constexpr frange_t() = default;
    constexpr frange_t(value_type value) : value_(value) {}
    constexpr value_type value() const { return value_; }
    constexpr operator value_type() const { return value(); }
private:
    value_type value_ = 0;
};

/**
 * @brief spec_t specialization for bounded floating-point ranges.
 *
 * @tparam Min Lower bound (inclusive)
 * @tparam Max Upper bound (inclusive)
 */
template<double Min, double Max>
struct spec_t<frange_t<Min, Max>> {
    
    using value_type = typename frange_t<Min, Max>::value_type;

    static constexpr json_type_t json_type = json_type_t::floating;
    static constexpr cbor_type_t cbor_type = cbor_type_t::floating;

    static error_t read(frange_t<Min, Max>& out, const io_value_t& val)
    {
        double v = val.number();
        if (v < Min || v > Max) {
            MOLD_DEBUG_LOG("FAIL: Floating value %f is out of range [%f, %f].", v, Min, Max);
            return error_t::handler_failure;
        }
        out = value_type(v);
        return error_t::ok;
    }

    static void emit(const frange_t<Min, Max>& in, const io_sink_t& sink)
    {
        sink.write_float(double(in));
    }
};

}

#endif
