use cxx::{let_cxx_string, SharedPtr};

pub struct Runtime {
    runtime: SharedPtr<ffi::FloatRuntime>,
}

impl Runtime {
    fn new(sample_rate: f64, block_size: usize) -> Self {
        Self {
            runtime: ffi::make_float_runtime(sample_rate, block_size),
        }
    }

    fn update_shared_resource_map(&self, name: String, data: Box<Vec<f32>>) -> bool {
        let_cxx_string!(cxx_name = name);

        unsafe {
            ffi::update_shared_resource_map_float(
                self.runtime.clone(),
                &cxx_name,
                data.as_ptr(),
                data.len(),
            )
        }
    }
}

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("elementary/src/include/Main.cpp");

        type FloatRuntime;

        fn make_float_runtime(sampleRate: f64, blockSize: usize) -> SharedPtr<FloatRuntime>;

        unsafe fn update_shared_resource_map_float(
            runtime: SharedPtr<FloatRuntime>,
            name: &CxxString,
            data: *const f32,
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
        let result = runtime
            .update_shared_resource_map("test".to_string(), Box::new(vec![0.0, 1.0, 0.0, 1.0]));

        println!("Stored to shared resource map: {result}");

        assert!(result);
    }
}

// TODO Remove once integrated with CLI
pub fn say_name() -> String {
    "Elementary".to_string()
}
