use crate::node::{NodeRepr, ShallowNodeRepr};
use crate::reconcile::reconcile;
use crate::std::prelude::*;
use std::cell::UnsafeCell;
use std::collections::{BTreeMap, HashMap};
use std::sync::Arc;

pub trait FloatType: 'static {}
impl FloatType for f32 {}
impl FloatType for f64 {}

#[derive(Default)]
pub struct AudioBuffer<T> {
    pub data: Vec<T>,
    pub channels: usize,
    pub frames: usize,
}

impl<T> AudioBuffer<T>
where
    T: FloatType + Clone + Default,
{
    pub fn new(channels: usize, frames: usize) -> Self {
        Self {
            data: vec![Default::default(); channels * frames],
            channels,
            frames,
        }
    }
}

pub struct Directive {
    pub graph: Option<Vec<NodeRepr>>,
    pub resources: Option<HashMap<String, AudioBuffer<f32>>>,
}

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("elem/src/Bindings.h");

        type RuntimeBindings;

        fn new_runtime_instance(sample_rate: f64, block_size: usize) -> UniquePtr<RuntimeBindings>;
        fn add_shared_resource(
            self: Pin<&mut RuntimeBindings>,
            name: &String,
            channels: usize,
            frames: usize,
            data: &[f32],
        ) -> i32;
        fn apply_instructions(self: Pin<&mut RuntimeBindings>, batch: &String) -> i32;
        fn process_queued_events(self: Pin<&mut RuntimeBindings>) -> String;

        type c_void;
        unsafe fn process(
            self: Pin<&mut RuntimeBindings>,
            input_data: *const f32,
            output_data: *mut f32,
            num_channels: usize,
            num_frames: usize,
            user_data: *mut c_void,
        ) -> ();
    }
}

struct EngineInternal {
    inner: UnsafeCell<cxx::UniquePtr<ffi::RuntimeBindings>>,
}

unsafe impl Send for EngineInternal {}
unsafe impl Sync for EngineInternal {}

impl EngineInternal {
    fn add_shared_resource(
        &self,
        name: &String,
        channels: usize,
        frames: usize,
        data: &[f32],
    ) -> i32 {
        unsafe {
            self.inner
                .get()
                .as_mut()
                .unwrap()
                .as_mut()
                .unwrap()
                .add_shared_resource(name, channels, frames, data)
        }
    }

    fn apply_instructions(&self, instructions: &serde_json::Value) -> Result<i32, &str> {
        unsafe {
            let result = self
                .inner
                .get()
                .as_mut()
                .unwrap()
                .as_mut()
                .unwrap()
                .apply_instructions(&instructions.to_string());

            Ok(result)
        }
    }

    fn process_queued_events(&self) -> Result<serde_json::Value, &str> {
        unsafe {
            let batch = self
                .inner
                .get()
                .as_mut()
                .unwrap()
                .as_mut()
                .unwrap()
                .process_queued_events();

            Ok(serde_json::from_str(&batch).unwrap())
        }
    }
}

pub struct ProcessHandle {
    inner: Arc<EngineInternal>,
}

impl ProcessHandle {
    fn new(inner: Arc<EngineInternal>) -> Self {
        Self { inner }
    }

    pub fn process<T>(
        &self,
        input_data: *const f32,
        output_data: *mut f32,
        num_channels: usize,
        num_frames: usize,
        user_data: *mut T,
    ) {
        unsafe {
            self.inner
                .inner
                .get()
                .as_mut()
                .unwrap()
                .as_mut()
                .unwrap()
                .process(
                    input_data,
                    output_data,
                    num_channels,
                    num_frames,
                    user_data as *mut ffi::c_void,
                );
        }
    }
}

pub struct MainHandle {
    inner: Arc<EngineInternal>,
    node_map: BTreeMap<i32, ShallowNodeRepr>,
}

impl MainHandle {
    fn new(inner: Arc<EngineInternal>) -> Self {
        Self {
            inner,
            node_map: BTreeMap::new(),
        }
    }

    pub fn render(&mut self, directive: Directive) -> Result<i32, &str> {
        if let Some(resources) = directive.resources {
            for (k, v) in resources.into_iter() {
                let rc =
                    self.inner
                        .add_shared_resource(&k, v.channels, v.frames, v.data.as_slice());
                println!("Add resource result: {}", rc);
            }
        }

        if let Some(graph) = directive.graph {
            let instructions = reconcile(&mut self.node_map, &graph);
            let serialized = serde_json::to_value(instructions).unwrap();
            let result = self.inner.apply_instructions(&serialized);
            println!("Apply instructions result: {}", result.unwrap_or(-1));

            result
        } else {
            Ok(0)
        }
    }

    pub fn process_queued_events(&mut self) -> Result<serde_json::Value, &str> {
        self.inner.process_queued_events()
    }
}

pub fn new_engine(sample_rate: f64, block_size: usize) -> (MainHandle, ProcessHandle) {
    let cell = UnsafeCell::new(ffi::new_runtime_instance(sample_rate, block_size));
    let arc = Arc::new(EngineInternal { inner: cell });

    let mut main = MainHandle::new(arc.clone());
    let proc = ProcessHandle::new(arc.clone());

    let cycle = root(sin(mul2(
        constant!({key: None, value: 2.0 * std::f64::consts::PI}),
        phasor(constant!({key: None, value: 110.0})),
    )));
    let roots = vec![cycle];

    let _ = main.render(Directive {
        graph: Some(roots),
        resources: None,
    });

    (main, proc)
}
