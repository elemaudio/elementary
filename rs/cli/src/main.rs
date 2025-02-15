use elem::engine;

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::{BuildStreamError, PlayStreamError};
use futures_util::{SinkExt, StreamExt, TryStreamExt};
use ringbuf::{traits::*, HeapRb};
use std::env;
use std::sync::{Arc, Mutex};
use thiserror::Error;
use tokio::net::{TcpListener, TcpStream};
use tracing::{error, info};
use tracing_subscriber;

#[derive(Error, Debug)]
pub enum ElementaryCliError {
    #[error("No input device available")]
    NoInputDevice,
    #[error("No output device available")]
    NoOutputDevice,
    #[error(transparent)]
    ThreadError(#[from] ThreadError),
    #[error("Could not construct device stream: {0}")]
    DeviceStreamConstructionFailed(#[from] BuildStreamError),
    #[error("Could not play device stream: {0}")]
    DeviceStreamPlayFailed(#[from] PlayStreamError),
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

    // Config parsing: input device, output device, bitrate, etc
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

    let config: cpal::StreamConfig = supported_config.into();

    // Establish a ring buffer to pump data from input to output The delay (implemented via the
    // ring buffer) acts as a safety margin to absorb timing mismatches between the input and
    // output streams. This ensures that there is always enough data in the buffer for the output
    // stream to consume, even if the input and output streams are slightly out of sync. This
    // prevents underflow (buffer running out of data) or overflow (buffer filling up too quickly),
    // both of which can cause audible artifacts.
    let latency_ms: f32 = 1000.0;
    let latency_frames = (latency_ms / 1_000.0) * config.sample_rate.0 as f32;
    let in_out_ring = HeapRb::new(latency_frames as usize);

    let (mut producer, mut consumer) = in_out_ring.split();

    // Start the Elem engine and derive handles to it
    let (engine_main, engine_proc) = engine::new_engine(44100.0, 512);

    // Closure that indicates what to do when we get data on the default audio input.
    // In our case, we push it to the `in_out_ring`, which is a ring buffer.
    // If we can't push data to the ring buffer, then something's gone wrong
    let input_data_fn = move |data: &[f32], _: &cpal::InputCallbackInfo| {
        let mut output_fell_behind = false;
        for &sample in data {
            if producer.try_push(sample).is_err() {
                output_fell_behind = true;
            }
        }
        if output_fell_behind {
            error!("Output stream fell behind: try increasing latency");
            // TODO: should we do something more substantial here?
        }
    };

    // Closure periodically invoked by the default audio output.
    // Whatever data we write to the `data` buffer will be 'shipped' to the output device.
    let output_data_fn = move |data: &mut [f32], _: &cpal::OutputCallbackInfo| {
        let num_channels = config.channels as usize;
        // TODO: how should I think about getting input data from the ring buffer here?
        // should I pass it to the `input_data` in `engine_proc`?
        //
        // let mut input_fell_behind = None;
        //
        // for sample in data {
        //     *sample = match consumer.pop() {
        //         Ok(s) => s,
        //         Err(err) => {
        //             input_fell_behind = Some(err);
        //             0.0
        //         }
        //     };
        // }
        // if let Some(err) = input_fell_behind {
        //     eprintln!(
        //         "input stream fell behind: {:?}: try increasing latency",
        //         err
        //     );
        // }
        for samples in data.chunks_mut(num_channels) {
            engine_proc.process(
                samples.as_ptr(),
                samples.as_mut_ptr(),
                num_channels,
                samples.len(),
                std::ptr::null_mut::<()>(),
            );
        }
    };

    let err_fn = move |_err| {};

    // Hook up Elem engine with the output device
    let input_stream = input_device.build_input_stream(&config, input_data_fn, err_fn, None)?;
    info!("Input stream established.");
    let output_stream = output_device.build_output_stream(&config, output_data_fn, err_fn, None)?;
    info!("Output stream established.");

    // Necessary as streams will not automatically play on some platforms
    input_stream.play()?;
    output_stream.play()?;

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
        Err(e) => unreachable!("One of the event poller or TCP listener threads panicked... should always return an error?"),
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
