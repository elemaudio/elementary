use cxx::UniquePtr;

pub struct Runtime {
    runtime: UniquePtr<ffi::Runtime>,
}

impl Runtime {
    fn new(sample_rate: f64, block_size: usize) -> Self {
        Self {
            runtime: ffi::runtime_new(sample_rate, block_size),
        }
    }

    // TODO: ffi::updateSharedResourceMap not exposed from module?
    // fn update_shared_resource_map(&self, name: String, data: f64, size: usize) -> bool {
    //     unsafe { ffi::updateSharedResourceMap(self, name, data, size) }
    // }
}

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("elementary/src/include/Runtime.h");
        include!("elementary/src/include/RuntimeShim.h");

        type Runtime;

        fn runtime_new(sampleRate: f64, blockSize: usize) -> UniquePtr<Runtime>;

        unsafe fn updateSharedResourceMap(
            self: Pin<&mut Runtime>,
            name: &CxxString,
            data: *const f64,
            size: usize,
        ) -> bool;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn instantiates_runtime() {
        let runtime = Runtime::new(44100.0, 512);
    }
}

// TODO Remove once integrated with CLI
pub fn say_name() -> String {
    "Elementary".to_string()
}
