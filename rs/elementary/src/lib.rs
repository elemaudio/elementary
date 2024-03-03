use cxx::{let_cxx_string, SharedPtr};

pub struct Runtime {
    runtime: SharedPtr<ffi::FloatRuntime>,
}

// TODO Remove unused once using in CLI example
#[allow(unused)]
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

    fn get_shared_resources_map_keys(&self) -> Vec<String> {
        ffi::get_shared_resource_map_keys_float(self.runtime.clone())
    }

    fn prune_shared_resource_map(&self) {
        ffi::prune_shared_resource_map_float(self.runtime.clone())
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

        fn get_shared_resource_map_keys_float(runtime: SharedPtr<FloatRuntime>) -> Vec<String>;

        fn prune_shared_resource_map_float(runtime: SharedPtr<FloatRuntime>);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn updates_shared_resource_map() {
        let runtime = Runtime::new(44100.0, 512);
        let stored = runtime
            .update_shared_resource_map("test".to_string(), Box::new(vec![0.0, 1.0, 0.0, 1.0]));
        let keys = runtime.get_shared_resources_map_keys();

        assert!(stored);
        assert!(keys.contains(&"test".to_string()));
    }

    #[test]
    fn prunes_shared_resource_map() {
        let runtime = Runtime::new(44100.0, 512);
        let stored = runtime
            .update_shared_resource_map("test".to_string(), Box::new(vec![0.0, 1.0, 0.0, 1.0]));

        runtime.prune_shared_resource_map();
        let keys = runtime.get_shared_resources_map_keys();

        assert!(stored);
        assert!(keys.is_empty());
    }
}

// TODO Remove once integrated with CLI
pub fn say_name() -> String {
    "Elementary".to_string()
}
