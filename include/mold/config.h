#ifndef MOLD_CONFIG_H
#define MOLD_CONFIG_H

/**
 * @file
 * @brief Compile-time feature flags and configuration.
 */

#ifndef MOLD_REFLECTION_JSON_ENABLED
#define MOLD_REFLECTION_JSON_ENABLED        true
#endif
#ifndef MOLD_REFLECTION_CBOR_ENABLED
#define MOLD_REFLECTION_CBOR_ENABLED        true
#endif
#ifndef MOLD_REFLECTION_MSGPACK_ENABLED
#define MOLD_REFLECTION_MSGPACK_ENABLED     true
#endif
#ifndef MOLD_REFLECTION_CBOR_FIELD_KEYS
#define MOLD_REFLECTION_CBOR_FIELD_KEYS     false
#endif
#ifndef MOLD_REFLECTION_SIZE_ENABLED
#define MOLD_REFLECTION_SIZE_ENABLED        false
#endif
#ifndef MOLD_REFLECTION_TYPE_NAME_ENABLED
#define MOLD_REFLECTION_TYPE_NAME_ENABLED   false
#endif

#ifndef MOLD_PRINT_ENABLED
#define MOLD_PRINT_ENABLED                  true
#endif
#ifndef MOLD_DEBUG_ENABLED
#define MOLD_DEBUG_ENABLED                  false
#endif

/**
 * @brief True when at least one format needs the string member name.
 *
 */
#define MOLD_REFLECTION_NAME_NEEDED (   \
    MOLD_REFLECTION_JSON_ENABLED ||     \
    MOLD_REFLECTION_MSGPACK_ENABLED ||  \
    (MOLD_REFLECTION_CBOR_ENABLED && !MOLD_REFLECTION_CBOR_FIELD_KEYS) || \
    MOLD_PRINT_ENABLED ||              \
    MOLD_DEBUG_ENABLED)

#endif
