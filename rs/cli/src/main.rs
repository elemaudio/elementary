use std::sync::{Arc, Mutex};
use std::{env, io::Error};

use futures_util::{SinkExt, StreamExt, TryStreamExt};
use log::info;
use tokio::net::{TcpListener, TcpStream};

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
}

impl RuntimeWrapper {
    pub fn new() -> Self {
        Self {
            runtime: Arc::new(Mutex::new(ffi::new_runtime_instance(44100.0, 512))),
        }
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

    let runtime = RuntimeWrapper::new();
    let (mut write, mut read) = ws_stream.split();

    while let Ok(next) = read.try_next().await {
        if let Some(msg) = next {
            match msg.to_text() {
                Ok(text) => {
                    println!("Received a message from {}: {}", addr, text);
                    let result = runtime
                        .runtime
                        .lock()
                        .unwrap()
                        .as_mut()
                        .unwrap()
                        .apply_instructions(&text.to_string());
                    println!("Apply instructions result: {}", result);
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
