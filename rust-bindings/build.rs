extern crate bindgen;

use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=../mabi.h");
    println!("cargo:rerun-if-changed=wrapper.h");

    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .derive_default(true)
        .use_core()
        .size_t_is_usize(true)
        .whitelist_type("vm_memslot")
        .whitelist_type("vm_map_info")
        .whitelist_type("vm_info")
        .whitelist_var("IO_MEMFLOW_OPEN_VM")
        .whitelist_var("IO_MEMFLOW_VM_INFO")
        .whitelist_var("IO_MEMFLOW_MAP_VM")
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
