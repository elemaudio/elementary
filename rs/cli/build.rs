fn main() {
    let build_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let runtime_include_dir = std::path::Path::new(&build_dir).join("../../runtime");

    cxx_build::bridge("src/main.rs")
        .include(runtime_include_dir.canonicalize().unwrap())
        .file("src/Bindings.cpp")
        .std("c++17")
        .compile("bindings");

    println!("cargo:rerun-if-changed=src/main.rs");
    println!("cargo:rerun-if-changed=src/Bindings.cpp");
    println!("cargo:rerun-if-changed=src/Bindings.h");
}
