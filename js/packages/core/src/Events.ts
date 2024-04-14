import EventEmitter from 'eventemitter3';


type EventTypes = {
  capture: (data: { source?: string, data: Float32Array[] }) => void;
  error: (error: Error) => void;
  fft: (data: { source?: string, data: { real: Float32Array, imag: Float32Array } }) => void;
  load: () => void;
  meter: (data: { source?: string, min: number; max: number, }) => void;
  scope: (data: { source?: string, data: Float32Array[] }) => void;
  snapshot: (data: { source?: string, data: number }) => void;
};

export default class extends EventEmitter<EventTypes> {
  constructor() {
    super();
  }
}
