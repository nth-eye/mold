#ifndef MOLD_REFL_IO_H
#define MOLD_REFL_IO_H

/**
 * @file
 * @brief Format-agnostic ABI types shared between spec callbacks and format-specific glue.
 */

#include <cstdint>
#include <string_view>

namespace mold {

/**
 * @brief Abstract accessor for a decoded primitive value.
 *
 * Stack-allocated 2-pointer wrapper created by the JSON/CBOR glue layer
 * and passed into `spec_t<T>::read`. Hides the format-specific primitive
 * type (`json_primitive_t` / `cbor_primitive_t`) behind a uniform interface.
 * The vtable pointer always refers to a constexpr static table, so there
 * is no heap allocation and the compiler can devirtualise calls under LTO.
 */
struct io_value_t {

    /**
     * @brief Vtable for format-specific primitive accessors.
     *
     */
    struct vtable_t {
        bool             (*null    )(const void*);
        bool             (*boolean )(const void*);
        int64_t          (*integer )(const void*);
        uint64_t         (*uinteger)(const void*);
        double           (*number  )(const void*);
        std::string_view (*string  )(const void*);
    };

    const vtable_t* vt;
    const void*     impl;

    bool             null()     const { return vt->null    (impl); }
    bool             boolean()  const { return vt->boolean (impl); }
    int64_t          integer()  const { return vt->integer (impl); }
    uint64_t         uinteger() const { return vt->uinteger(impl); }
    double           number()   const { return vt->number  (impl); }
    std::string_view string()   const { return vt->string  (impl); }
};

/**
 * @brief Abstract sink for emitting a primitive value.
 *
 * Stack-allocated 2-pointer wrapper created by the JSON/CBOR glue layer
 * and passed into `spec_t<T>::emit`. Hides the format-specific sink type
 * (`json_sink_t` / `cbor_sink_t`) behind a uniform interface. `write_string` 
 * emits a properly quoted/encoded string for the active format.
 */
struct io_sink_t {

    /**
     * @brief Vtable for format-specific primitive emitters.
     *
     */
    struct vtable_t {
        void (*write_bool  )(const void*, bool);
        void (*write_sint  )(const void*, int64_t);
        void (*write_uint  )(const void*, uint64_t);
        void (*write_float )(const void*, double);
        void (*write_string)(const void*, std::string_view);
    };

    const vtable_t* vt;
    const void*     impl;

    void write_bool  (bool v)             const { vt->write_bool  (impl, v); }
    void write_sint  (int64_t v)          const { vt->write_sint  (impl, v); }
    void write_uint  (uint64_t v)         const { vt->write_uint  (impl, v); }
    void write_float (double v)           const { vt->write_float (impl, v); }
    void write_string(std::string_view v) const { vt->write_string(impl, v); }
};

/**
 * @brief Event type for the unified format-agnostic handler dispatch.
 *
 * Both JSON and CBOR parsers/writers call handlers through this enum.
 */
enum class io_type_t : uint8_t {
    prepare_value,
    emit_or_next,
    visit_nullable,
};

/**
 * @brief Tagged payload passed to io_handler_t.
 *
 * Only a subset of the members is valid for a given event type:
 * - `prepare_value`:  `io_val` wraps the parsed primitive (or `slot_idx` for containers).
 * - `emit_or_next`:   `io_sink` wraps the output sink (or `previous_element` for iteration).
 * - `visit_nullable`: `slot_idx` is 1 to initialize, 0 to peek.
 */
struct io_data_t {
    union {
        void* mptr;
        const void* cptr;
    };
    union {
        size_t slot_idx;
        const io_value_t* io_val;
        const io_sink_t* io_sink;
        const void* previous_element;
    };
};
static_assert(sizeof(io_data_t) == sizeof(void*) * 2);

/**
 * @brief Unified handler signature for both decoding and encoding.
 *
 * The handler returns a pointer whose meaning depends on the event
 * (slot pointer, next element pointer, nullable inner pointer, etc.).
 */
using io_handler_t = void* (*)(io_type_t type, io_data_t data);

}

#endif
