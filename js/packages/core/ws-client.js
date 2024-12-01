import {el, Renderer, formatRoots} from "./dist/index.js";

// Create WebSocket connection.
const socket = new WebSocket("ws://localhost:8080");

// Connection opened
socket.addEventListener("open", (event) => {
  // Once the connection is opened we generate a new graph every 500ms
  setInterval(() => {
    let i = 1 + Math.round((Math.random() * 8));
    let f = 110 * i;

    socket.send(JSON.stringify({
      graph: formatRoots([el.cycle(el.const({key: 'a', value: f})), el.cycle(el.const({key: 'b', value: f * 1.01}))]),
    }));
  }, 500);
});

// Listen for messages
socket.addEventListener("message", (event) => {
  console.log("Message from server ", event.data);
});
