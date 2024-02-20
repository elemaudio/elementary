pub struct Runtime {}

impl Runtime {}

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("elementary/src/include/Runtime.h");
        include!("elementary/src/include/RuntimeShim.h");

        type Runtime;

        fn runtime_new(sampleRate: f64, blockSize: usize) -> UniquePtr<Runtime>;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn instantiates_runtime() {
        let runtime = ffi::runtime_new(44100.0, 512);
    }
}

// TODO Remove once integrated with CLI
pub fn say_name() -> String {
    "Elementary".to_string()
}
