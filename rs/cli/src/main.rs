use elem::engine;

use std::env;
use std::sync::{Arc, Mutex};

use futures_util::{SinkExt, StreamExt, TryStreamExt};
use tokio::net::{TcpListener, TcpStream};

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use tracing::{error, info};
use tracing_subscriber;

use thiserror::Error;

#[derive(Error, Debug)]
pub enum ElementaryCliError {
    #[error("No input device available")]
    NoInputDevice,
    #[error("No output device available")]
    NoOutputDevice,
    #[error(transparent)]
    ThreadError(#[from] ThreadError),
}

#[derive(Error, Debug)]
pub enum ThreadError {
    #[error("Event loop error: {0}")]
    EventLoop(String),
    #[error("Event poller error: {0}")]
    EventPoller(String),
    #[error("TCP listener error: {0}")]
    TCPListener(String),
}

fn main() -> Result<(), ElementaryCliError> {
    tracing_subscriber::fmt()
        .with_max_level(tracing::Level::INFO)
        .init();

    let addr = env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:8080".to_string());

    // Start the audio device
    let host = cpal::default_host();
    let output_device = host
        .default_output_device()
        .ok_or(ElementaryCliError::NoOutputDevice)?;

    if let Ok(output_device_name) = output_device.name() {
        info!("Default output device found: {}", output_device_name);
    }

    let input_device = host
        .default_input_device()
        .ok_or(ElementaryCliError::NoInputDevice)?;

    if let Ok(input_device_name) = input_device.name() {
        info!("Default input device found: {}", input_device_name);
    }

    let mut supported_configs_range = output_device
        .supported_output_configs()
        .expect("error while querying configs");
    let supported_config = supported_configs_range
        .next()
        .expect("no supported config?!")
        .with_max_sample_rate();

    // Start the Elem engine
    let (engine_main, engine_proc) = engine::new_engine(44100.0, 512);

    // Hook up Elem engine with the device
    let config: cpal::StreamConfig = supported_config.into();
    let _stream = output_device.build_output_stream(
        &config,
        move |data: &mut [f32], _| {
            let num_channels = config.channels as usize;
            for samples in data.chunks_mut(num_channels) {
                engine_proc.process(
                    samples.as_ptr(),
                    samples.as_mut_ptr(),
                    num_channels,
                    samples.len(),
                    std::ptr::null_mut::<()>(),
                );
            }
        },
        move |err| {},
        None, // None=blocking, Some(Duration)=timeout
    );

    tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .map_err(|e| ThreadError::EventLoop(e.to_string()))?
        .block_on(run_event_loop_main(addr, engine_main))
        .map_err(|e| e.into())
}

async fn run_event_loop_main(
    addr: String,
    engine_main: engine::MainHandle,
) -> Result<(), ThreadError> {
    let shared_engine_main = Arc::new(Mutex::new(engine_main));

    // If either of the threads fails, we stop the program
    let res = tokio::try_join!(
        tokio::spawn(run_event_poller(shared_engine_main.clone())),
        tokio::spawn(run_tcp_listener(addr, shared_engine_main.clone())),
    );

    match res {
        Ok((first, second)) => first.and(second),
        Err(e) => todo!("One of the tasks panicked... should always return an error"),
    }
}

async fn run_event_poller(engine_main: Arc<Mutex<engine::MainHandle>>) -> Result<(), ThreadError> {
    let mut interval =
        tokio::time::interval(tokio::time::Duration::from_millis((1000.0 / 30.0) as u64));

    loop {
        interval.tick().await;

        if let Ok(result) = engine_main.lock().unwrap().process_queued_events() {
            if let Some(events) = result.as_array() {
                for evt in events.iter() {
                    println!("[Event] {}", evt.to_string());
                }
            }
        }
    }
}

async fn run_tcp_listener(
    addr: String,
    engine_main: Arc<Mutex<engine::MainHandle>>,
) -> Result<(), ThreadError> {
    // Create the TCP listener we'll accept connections on
    let try_socket = TcpListener::bind(&addr).await;
    let listener = try_socket.expect("Failed to bind");
    info!("Listening on: {}", addr);

    while let Ok((stream, _)) = listener.accept().await {
        tokio::spawn(accept_connection(stream, engine_main.clone()));
    }

    Ok(())
}

async fn accept_connection(stream: TcpStream, engine_main: Arc<Mutex<engine::MainHandle>>) {
    let addr = stream
        .peer_addr()
        .expect("connected streams should have a peer address");
    info!("Peer address: {}", addr);

    let ws_stream = tokio_tungstenite::accept_async(stream)
        .await
        .expect("Error during the websocket handshake occurred");

    info!("New WebSocket connection: {}", addr);

    let (mut write, mut read) = ws_stream.split();

    while let Ok(next) = read.try_next().await {
        if let Some(msg) = next {
            match msg.to_text() {
                Ok(text) => {
                    println!("Received a message from {}: {}", addr, text);
                    let directive: server::UnresolvedDirective =
                        serde_json::from_str(text).unwrap_or_default();
                    let resolved = server::resolve_directive(directive).await;

                    {
                        let mut main = engine_main.lock().unwrap();
                        let _ = main.render(resolved);
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
