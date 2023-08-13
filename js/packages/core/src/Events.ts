import events from 'events';

type EventListener<E> = (event: E) => void

type Events = {
  "error": Error,
  "fft": { source?: string, data: { real: Float32Array, imag: Float32Array } };
  "load": void;
  "meter": { source?: string; min: number; max: number; };
  "scope": { source?: string, data: Float32Array[] };
  "snapshot": { source?: string, data: number };
}

export declare interface EventEmitter {
  addListener<K extends keyof Events>(eventName: K, listener: EventListener<Events[K]>): this;
  listenerCount<K extends keyof Events>(eventName: K, listener?: EventListener<Events[K]>): number;
  listeners<K extends keyof Events>(eventName: K): Function[];
  off<K extends keyof Events>(eventName: K, listener: EventListener<Events[K]>): this;
  on<K extends keyof Events>(eventName: K, listener: EventListener<Events[K]>): this;
  once<K extends keyof Events>(eventName: K, listener: EventListener<Events[K]>): this;
  prependListener<K extends keyof Events>(eventName: K, listener: EventListener<Events[K]>): this;
  prependOnceListener<K extends keyof Events>(eventName: K, listener: EventListener<Events[K]>): this;
  removeAllListeners<K extends keyof Events>(eventName?: K): this;
  removeListener<K extends keyof Events>(eventName: K, listener: EventListener<Events[K]>): this;
  rawListeners<K extends keyof Events>(eventName: K): Function[];
}

export class EventEmitter extends events.EventEmitter { }