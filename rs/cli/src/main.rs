use std::collections::{BTreeMap, HashSet, VecDeque};
use std::sync::{Arc, Mutex};
use std::{env, io::Error};

use futures_util::{SinkExt, StreamExt, TryStreamExt};
use log::info;
use serde::{Deserialize, Serialize};
use serde_json::json;
use tokio::net::{TcpListener, TcpStream};

#[derive(Serialize, Deserialize)]
struct NodeRepr {
    hash: i32,
    kind: String,
    props: serde_json::Map<String, serde_json::Value>,
    output_channel: u32,
    children: Vec<NodeRepr>,
}

struct ShallowNodeRepr {
    hash: i32,
    kind: String,
    props: serde_json::Map<String, serde_json::Value>,
    output_channel: u32,
    children: Vec<i32>,
}

fn shallow_clone(node: &NodeRepr) -> ShallowNodeRepr {
    ShallowNodeRepr {
        hash: node.hash,
        kind: node.kind.clone(),
        props: node.props.clone(),
        output_channel: node.output_channel,
        children: node.children.iter().map(|n| n.hash).collect::<Vec<i32>>(),
    }
}

#[derive(Serialize, Deserialize)]
struct Directive {
    graph: Option<Vec<NodeRepr>>,
}

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("cli/src/Bindings.h");

        type RuntimeBindings;

        fn new_runtime_instance(sample_rate: f64, block_size: usize) -> UniquePtr<RuntimeBindings>;
        fn apply_instructions(self: Pin<&mut RuntimeBindings>, batch: &String) -> i32;
    }
}

pub struct RuntimeWrapper {
    runtime: Arc<Mutex<cxx::UniquePtr<ffi::RuntimeBindings>>>,
    node_map: BTreeMap<i32, ShallowNodeRepr>,
}

impl RuntimeWrapper {
    pub fn new() -> Self {
        Self {
            runtime: Arc::new(Mutex::new(ffi::new_runtime_instance(44100.0, 512))),
            node_map: BTreeMap::new(),
        }
    }

    pub fn reconcile(&mut self, roots: &Vec<NodeRepr>) -> serde_json::Value {
        let mut visited: HashSet<i32> = HashSet::new();
        let mut queue: VecDeque<&NodeRepr> = VecDeque::new();
        let mut instructions = serde_json::Value::Array(vec![]);

        for root in roots.iter() {
            // TODO: ref?
            queue.push_back(root);
        }

        while !queue.is_empty() {
            let next = queue.pop_front().unwrap();

            if visited.contains(&next.hash) {
                continue;
            }

            // Mount
            if !self.node_map.contains_key(&next.hash) {
                // Create node
                instructions
                    .as_array_mut()
                    .unwrap()
                    .push(json!([0, next.hash, next.kind]));

                // Append child
                for child in next.children.iter() {
                    instructions.as_array_mut().unwrap().push(json!([
                        2,
                        next.hash,
                        child.hash,
                        child.output_channel
                    ]));
                }

                self.node_map.insert(next.hash, shallow_clone(&next));
            }

            // Props
            for (name, value) in &next.props {
                // TODO: Only add the instruction if the prop value != existing prop value
                instructions
                    .as_array_mut()
                    .unwrap()
                    .push(json!([3, next.hash, name, value]));
            }

            for child in next.children.iter() {
                queue.push_back(child);
            }

            visited.insert(next.hash);
        }

        // Activate roots
        instructions.as_array_mut().unwrap().push(json!([
            4,
            roots.iter().map(|n| n.hash).collect::<Vec<i32>>()
        ]));

        // Commit
        instructions.as_array_mut().unwrap().push(json!([5]));

        // Sort so that creates land before appends, etc
        instructions
            .as_array_mut()
            .unwrap()
            .sort_by(|a, b| a[0].as_i64().cmp(&b[0].as_i64()));

        instructions
    }
}

// Basically the cxx::UniquePtr type wraps a C-style opaque pointer and
// because of that cannot guarantee the ability to move the UniquePtr across
// threads, which we may need here in Tokio land because we're not sure which
// thread pool thread we'll be on when we receive the next websocket message.
//
// To get around that, I've made this wrapper class with access secured behind
// a mutex, which truthfully I think is probably unnecessary but that gave me
// the opportunity to add this unsafe impl Send which convinces the compiler that
// we'll be ok. I think there's probably a cleaner way, but this is good enough for
// now, I want to get to the fun stuff.
unsafe impl Send for RuntimeWrapper {}

#[tokio::main]
async fn main() -> Result<(), Error> {
    let _ = env_logger::try_init();
    let addr = env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:8080".to_string());

    // Create the event loop and TCP listener we'll accept connections on.
    let try_socket = TcpListener::bind(&addr).await;
    let listener = try_socket.expect("Failed to bind");
    info!("Listening on: {}", addr);

    while let Ok((stream, _)) = listener.accept().await {
        tokio::spawn(accept_connection(stream));
    }

    Ok(())
}

async fn accept_connection(stream: TcpStream) {
    let addr = stream
        .peer_addr()
        .expect("connected streams should have a peer address");
    info!("Peer address: {}", addr);

    let ws_stream = tokio_tungstenite::accept_async(stream)
        .await
        .expect("Error during the websocket handshake occurred");

    info!("New WebSocket connection: {}", addr);

    let mut runtime = RuntimeWrapper::new();
    let (mut write, mut read) = ws_stream.split();

    while let Ok(next) = read.try_next().await {
        if let Some(msg) = next {
            match msg.to_text() {
                Ok(text) => {
                    println!("Received a message from {}: {}", addr, text);
                    let directive: Directive =
                        serde_json::from_str(text).unwrap_or(Directive { graph: None });

                    if let Some(graph) = directive.graph {
                        let instructions = runtime.reconcile(&graph);
                        let result = runtime
                            .runtime
                            .lock()
                            .unwrap()
                            .as_mut()
                            .unwrap()
                            .apply_instructions(&instructions.to_string());

                        println!("Apply instructions result: {}", result);
                    }

                    // TODO: Properly handle the write failure case
                    write.send(msg).await.unwrap()
                }
                Err(e) => {
                    println!("Received a non-text message from {}: {}", addr, e);
                    write.send("No thanks".into()).await.unwrap()
                }
            }
        }
    }

    println!("Connection closed to peer {}", addr);
}
