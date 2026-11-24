/*
 * Awesome-OS libcargo C11 ABI Header (B8G_ABI_MAGIC_V3)
 *
 * Allows holo-compiler and native C/C++/WASI hosts to invoke Cargo in-process
 * with zero shell-outs and unified B8G CAS target directory resolution.
 */

#ifndef AWESOME_OS_LIBCARGO_H
#define AWESOME_OS_LIBCARGO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Canonical B8G ABI v3 magic: ASCII "BGV3" in little-endian (0x33564742). */
#define B8G_ABI_MAGIC_V3 0x33564742u

/** 64-byte aligned ABI descriptor block for zero-copy arena handoff. */
typedef struct B8gCargoAbiHeader {
#if defined(_MSC_VER)
    __declspec(align(64)) uint32_t magic;
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    _Alignas(64) uint32_t magic;
#else
    __attribute__((aligned(64))) uint32_t magic;
#endif
    uint32_t abi_version;
    uint64_t flags;
    uint8_t  reserved[48];
} B8gCargoAbiHeader;

/** Return codes for buffer-writing FFI functions (non-negative = bytes written). */
#define B8G_CARGO_ERR_INVALID_ARG (-1)
#define B8G_CARGO_ERR_INTERNAL    (-2)
#define B8G_CARGO_ERR_BUF_SMALL   (-3)

/** Returns B8G_ABI_MAGIC_V3 (0x33564742). */
uint32_t b8g_cargo_abi_magic(void);

/** Returns a static NUL-terminated Cargo version string. */
const char* b8g_cargo_version(void);

/**
 * Resolves the unified Awesome-OS Cargo CAS root directory.
 * Honors B8G_GHOST_CACHE_DIR -> "<B8G_GHOST_CACHE_DIR>/cargo".
 * Writes a NUL-terminated UTF-8 path into out_buf (capacity out_cap).
 * Returns the byte length (excluding NUL) on success, or a negative error code.
 */
int32_t b8g_cargo_resolve_cas_root(char* out_buf, size_t out_cap);

/**
 * Loads a Cargo manifest/workspace in-process (zero subprocesses) and writes
 * Cargo metadata JSON (format-version 1, --no-deps) into out_buf.
 * Returns the byte length (excluding NUL) on success, or a negative error code.
 */
int32_t b8g_cargo_metadata_json(const char* manifest_path, char* out_buf, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* AWESOME_OS_LIBCARGO_H */
