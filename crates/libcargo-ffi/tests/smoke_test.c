/*
 * C11 Dynamic-Load Smoke Test for Awesome-OS libcargo.dll (B8G_ABI_MAGIC_V3)
 * Compiled natively on Windows via `e:\awesome-os\bin\zig.cmd cc` (zero WSL).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../../../include/libcargo.h"

typedef uint32_t (*fn_abi_magic)(void);
typedef const char* (*fn_version)(void);
typedef int32_t (*fn_resolve_cas_root)(char*, size_t);
typedef int32_t (*fn_metadata_json)(const char*, char*, size_t);

int main(int argc, char** argv) {
    const char* dll_path = (argc > 1) ? argv[1] : "dist/libcargo.dll";
    const char* manifest_path = (argc > 2) ? argv[2] : "crates/libcargo-ffi/Cargo.toml";

    HMODULE mod = LoadLibraryA(dll_path);
    if (!mod) {
        fprintf(stderr, "FAIL: LoadLibraryA(%s) failed (err=%lu)\n", dll_path, GetLastError());
        return 1;
    }

    fn_abi_magic abi_magic = (fn_abi_magic)(void*)GetProcAddress(mod, "b8g_cargo_abi_magic");
    fn_version version = (fn_version)(void*)GetProcAddress(mod, "b8g_cargo_version");
    fn_resolve_cas_root resolve_cas = (fn_resolve_cas_root)(void*)GetProcAddress(mod, "b8g_cargo_resolve_cas_root");
    fn_metadata_json metadata_json = (fn_metadata_json)(void*)GetProcAddress(mod, "b8g_cargo_metadata_json");

    if (!abi_magic || !version || !resolve_cas || !metadata_json) {
        fprintf(stderr, "FAIL: Missing exported symbol(s) in %s\n", dll_path);
        FreeLibrary(mod);
        return 2;
    }

    uint32_t magic = abi_magic();
    if (magic != B8G_ABI_MAGIC_V3) {
        fprintf(stderr, "FAIL: b8g_cargo_abi_magic() = 0x%08X (expected 0x%08X)\n", magic, B8G_ABI_MAGIC_V3);
        FreeLibrary(mod);
        return 3;
    }
    printf("PASS: b8g_cargo_abi_magic() = 0x%08X\n", magic);

    const char* ver = version();
    if (!ver || strlen(ver) == 0) {
        fprintf(stderr, "FAIL: b8g_cargo_version() returned empty\n");
        FreeLibrary(mod);
        return 4;
    }
    printf("PASS: b8g_cargo_version() = %s\n", ver);

    SetEnvironmentVariableA("B8G_GHOST_CACHE_DIR", "E:\\b8g-ghost-test");
    char cas_buf[512];
    int32_t cas_len = resolve_cas(cas_buf, sizeof(cas_buf));
    if (cas_len <= 0 || strstr(cas_buf, "b8g-ghost-test") == NULL || strstr(cas_buf, "cargo") == NULL) {
        fprintf(stderr, "FAIL: b8g_cargo_resolve_cas_root() len=%d buf='%s'\n", cas_len, cas_buf);
        FreeLibrary(mod);
        return 5;
    }
    printf("PASS: b8g_cargo_resolve_cas_root() = %s (%d bytes)\n", cas_buf, cas_len);

    size_t meta_cap = 1024 * 1024;
    char* meta_buf = (char*)malloc(meta_cap);
    if (!meta_buf) {
        fprintf(stderr, "FAIL: malloc failed\n");
        FreeLibrary(mod);
        return 6;
    }
    int32_t meta_len = metadata_json(manifest_path, meta_buf, meta_cap);
    if (meta_len <= 0 || strstr(meta_buf, "\"libcargo-ffi\"") == NULL) {
        fprintf(stderr, "FAIL: b8g_cargo_metadata_json(%s) len=%d\n", manifest_path, meta_len);
        free(meta_buf);
        FreeLibrary(mod);
        return 7;
    }
    printf("PASS: b8g_cargo_metadata_json(%s) = %d bytes (contains \"libcargo-ffi\")\n", manifest_path, meta_len);

    free(meta_buf);
    FreeLibrary(mod);
    printf("ALL LIBCARGO C11 SMOKE TESTS PASSED\n");
    return 0;
}
