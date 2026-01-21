fn main() {
    cxx_build::bridge("src/lib.rs")
        .flag_if_supported("-std=c++20")
        .compile("elementary");

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/include/Main.cpp");
    println!("cargo:rerun-if-changed=src/include/Runtime.h");
    println!("cargo:rerun-if-changed=src/include/cxx.h");
    println!("cargo:rerun-if-changed=src/include/JSON.h");
}
