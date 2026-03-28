#ifndef MOLD_TYPES_NULL_H
#define MOLD_TYPES_NULL_H

/**
 * @file
 * @brief Explicit null value type for serialization.
 */

#include "mold/refl/spec.h"

namespace mold {

/**
 * @brief Special type to represent explicit null value for some frontends.
 *
 */
struct null_t {};

/**
 * @brief Specialization for `null_t`.
 *
 */
template<>
struct spec_t<null_t> {
    static constexpr json_type_t json_type = json_type_t::null;
    static constexpr cbor_type_t cbor_type = cbor_type_t::null;
    static constexpr bool write_null = true;
};

}

#endif
