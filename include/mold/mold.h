#ifndef MOLD_MOLD_H
#define MOLD_MOLD_H

/**
 * @file
 * @brief Aggregate header that includes the entire mold library.
 */

#include "mold/version.h"

#include "mold/json/json_parse.h"
#include "mold/json/json_write.h"
#include "mold/json/json_debug.h"

#include "mold/cbor/cbor_parse.h"
#include "mold/cbor/cbor_write.h"
#include "mold/cbor/cbor_debug.h"

#include "mold/msgpack/msgpack_parse.h"
#include "mold/msgpack/msgpack_write.h"
#include "mold/msgpack/msgpack_debug.h"

#include "mold/util/debug.h"

#include "mold/types/std.h"
#include "mold/types/null.h"
#include "mold/types/nullable.h"
#include "mold/types/uuid.h"
#include "mold/types/range.h"
#include "mold/types/bytes.h"
#include "mold/types/string.h"
#include "mold/types/field.h"
#include "mold/types/vector.h"

#endif
