#ifndef MOLD_VERSION_H
#define MOLD_VERSION_H

/**
 * @file
 * @brief Library version macros and constants.
 */

#define MOLD_STRINGIFY(x)   #x
#define MOLD_TO_STRING(x)   MOLD_STRINGIFY(x)

#define MOLD_VERSION_MAJOR  0
#define MOLD_VERSION_MINOR  1
#define MOLD_VERSION_PATCH  0
#define MOLD_VERSION        (  \
    MOLD_VERSION_MAJOR << 16 | \
    MOLD_VERSION_MINOR <<  8 | \
    MOLD_VERSION_PATCH <<  0)
#define MOLD_VERSION_STR \
    MOLD_TO_STRING(MOLD_VERSION_MAJOR) "." \
    MOLD_TO_STRING(MOLD_VERSION_MINOR) "." \
    MOLD_TO_STRING(MOLD_VERSION_PATCH)

namespace mold {

inline constexpr auto version       = MOLD_VERSION;     ///< Version number.
inline constexpr auto version_str   = MOLD_VERSION_STR; ///< Version string.

}

#endif
