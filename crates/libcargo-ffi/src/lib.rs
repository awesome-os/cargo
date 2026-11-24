//! Awesome-OS `libcargo` C11 FFI (`B8G_ABI_MAGIC_V3`).
//!
//! Exposes Cargo's manifest/workspace loader and unified CAS root resolution
//! as a native C-ABI dynamic library (`libcargo.dll` / `libcargo.so`) so
//! `holo-compiler` can invoke Cargo in-process with zero shell-outs.

use std::ffi::{CStr, CString, c_char};
use std::path::{Path, PathBuf};
use std::sync::OnceLock;

/// Canonical B8G ABI v3 magic (`0x33564742`, ASCII `"BGV3"`).
pub const B8G_ABI_MAGIC_V3: u32 = 0x3356_4742;

const ERR_INVALID_ARG: i32 = -1;
const ERR_INTERNAL: i32 = -2;
const ERR_BUF_SMALL: i32 = -3;

/// 64-byte aligned ABI header matching `include/libcargo.h`.
#[repr(C, align(64))]
pub struct B8gCargoAbiHeader {
    pub magic: u32,
    pub abi_version: u32,
    pub flags: u64,
    pub reserved: [u8; 48],
}

static VERSION_CSTR: OnceLock<CString> = OnceLock::new();

fn write_str_to_buf(s: &str, out_buf: *mut c_char, out_cap: usize) -> i32 {
    if out_buf.is_null() || out_cap == 0 {
        return ERR_INVALID_ARG;
    }
    let bytes = s.as_bytes();
    if bytes.len() + 1 > out_cap {
        return ERR_BUF_SMALL;
    }
    // SAFETY: Caller guarantees `out_buf` is valid for `out_cap` writable bytes.
    unsafe {
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), out_buf.cast::<u8>(), bytes.len());
        *out_buf.add(bytes.len()) = 0;
    }
    i32::try_from(bytes.len()).unwrap_or(ERR_INTERNAL)
}

fn resolve_cas_root_path() -> PathBuf {
    if let Some(ghost) = std::env::var_os("B8G_GHOST_CACHE_DIR") {
        if !ghost.is_empty() {
            return PathBuf::from(ghost).join("cargo");
        }
    }
    if let Some(cargo_home) = std::env::var_os("CARGO_HOME") {
        if !cargo_home.is_empty() {
            return PathBuf::from(cargo_home).join("cas");
        }
    }
    if let Ok(cargo_home) = home::cargo_home() {
        return cargo_home.join("cas");
    }
    PathBuf::from(".b8g-cas").join("cargo")
}

/// Returns `B8G_ABI_MAGIC_V3` (`0x33564742`).
#[unsafe(no_mangle)]
pub extern "C" fn b8g_cargo_abi_magic() -> u32 {
    B8G_ABI_MAGIC_V3
}

/// Returns a static NUL-terminated Cargo version string.
#[unsafe(no_mangle)]
pub extern "C" fn b8g_cargo_version() -> *const c_char {
    VERSION_CSTR
        .get_or_init(|| {
            let v = format!("{}+awesome-os-cas", cargo::version());
            CString::new(v).unwrap_or_else(|_| c"unknown+awesome-os-cas".to_owned())
        })
        .as_ptr()
}

/// Resolves the unified Awesome-OS Cargo CAS root directory.
///
/// Honors `B8G_GHOST_CACHE_DIR` -> `<B8G_GHOST_CACHE_DIR>/cargo`.
#[unsafe(no_mangle)]
pub extern "C" fn b8g_cargo_resolve_cas_root(out_buf: *mut c_char, out_cap: usize) -> i32 {
    let result = std::panic::catch_unwind(|| {
        let root = resolve_cas_root_path();
        let s = root.to_string_lossy();
        write_str_to_buf(&s, out_buf, out_cap)
    });
    result.unwrap_or(ERR_INTERNAL)
}

/// Invokes Cargo's in-process manifest/workspace loader without spawning a subprocess
/// and writes format-version 1 metadata JSON into `out_buf`.
#[unsafe(no_mangle)]
pub extern "C" fn b8g_cargo_metadata_json(
    manifest_path: *const c_char,
    out_buf: *mut c_char,
    out_cap: usize,
) -> i32 {
    if manifest_path.is_null() || out_buf.is_null() || out_cap == 0 {
        return ERR_INVALID_ARG;
    }

    let result = std::panic::catch_unwind(|| {
        // SAFETY: Caller guarantees `manifest_path` is a valid NUL-terminated C string.
        let c_path = unsafe { CStr::from_ptr(manifest_path) };
        let Ok(path_str) = c_path.to_str() else {
            return ERR_INVALID_ARG;
        };
        if path_str.is_empty() {
            return ERR_INVALID_ARG;
        }

        let mut path = PathBuf::from(path_str);
        if path.is_dir() {
            path = path.join("Cargo.toml");
        }
        if !path.is_absolute() {
            match std::env::current_dir() {
                Ok(cwd) => path = cwd.join(path),
                Err(_) => return ERR_INTERNAL,
            }
        }
        let abs_path = dunce_like_clean(&path);

        let gctx = match cargo::GlobalContext::default() {
            Ok(g) => g,
            Err(_) => return ERR_INTERNAL,
        };

        let ws = match cargo::workspace::Workspace::new(&abs_path, &gctx) {
            Ok(w) => w,
            Err(_) => return ERR_INTERNAL,
        };

        let opts = cargo::ops::OutputMetadataOptions {
            cli_features: cargo::resolver::features::CliFeatures::new_all(false),
            no_deps: true,
            version: 1,
            filter_platforms: Vec::new(),
        };

        let info = match cargo::ops::output_metadata(&ws, &opts) {
            Ok(i) => i,
            Err(_) => return ERR_INTERNAL,
        };

        let json = match serde_json::to_string(&info) {
            Ok(j) => j,
            Err(_) => return ERR_INTERNAL,
        };

        write_str_to_buf(&json, out_buf, out_cap)
    });

    result.unwrap_or(ERR_INTERNAL)
}

fn dunce_like_clean(path: &Path) -> PathBuf {
    let canonical = std::fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf());
    #[cfg(windows)]
    {
        if let Some(s) = canonical.to_str() {
            if let Some(stripped) = s.strip_prefix(r"\\?\") {
                return PathBuf::from(stripped);
            }
        }
    }
    canonical
}
