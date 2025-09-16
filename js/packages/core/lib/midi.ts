import {
  createNode,
  resolve,
  ElemNode,
  NodeRepr_t,
  unpack,
} from "../nodeUtils";

/**
 * A simple identity function for realtime block events.
 *
 * Expects no props and no children, though it may accept children as
 * a way to merge and propagate other event streams.

 * @returns {NodeRepr_t}
 */
export function midinotein(...args: Array<ElemNode>): NodeRepr_t {
  return createNode("midinotein", {}, args.map(resolve));
}

/**
 * Polyphonic note allocation node with LRU voice allocation and stealing.
 *
 * This uses MPE style voice allocation, remapping the incoming note events
 * onto channels 0-15, corresponding to (up to) 16 voices.
 *
 * @param {Object} props
 * @param {string} [props.key] - An optional unique identifier for the node
 * @param {number} [props.voices] - Number of voices to allocate (defaults to 16)
 * @param {...ElemNode} children - Child nodes to process
 * @returns {NodeRepr_t}
 */
export function midinoteallocate(
  props: { key?: string; voices?: number },
  ...children: Array<ElemNode>
): NodeRepr_t {
  return createNode("midinoteallocate", props, children.map(resolve));
}

/**
 * Emits audio signals reflecting the frequency and velocity information
 * of the incoming MIDI events.
 *
 * Unpacks the incoming MIDI note event stream into two separate outputs:
 * - Channel 0: Frequency (Hz)
 * - Channel 1: Velocity (normalized 0-1)
 *
 * @param {Object} props
 * @param {string} [props.key] - An optional unique identifier for the node
 * @param {number} [props.channel] - Filter incoming events, react only to those that match the channel number
 * @param {...ElemNode} children - MIDI note event stream(s) to unpack
 * @returns {Array<NodeRepr_t>} An array of two nodes: [frequency, velocity]
 */
export function midinoteunpack(
  props: { key?: string; channel?: number },
  ...children: Array<ElemNode>
): Array<NodeRepr_t> {
  return unpack(createNode("midinoteunpack", props, children.map(resolve)), 2);
}

/**
 * Shifts MIDI note values by a specified amount.
 *
 * Transposes incoming MIDI note events by adding or subtracting a fixed offset
 * to the note number. Useful for octave shifting or transposition effects.
 *
 * @param {Object} props
 * @param {string} [props.key] - An optional unique identifier for the node
 * @param {number} props.steps - Number of steps (semitones) to shift the MIDI note numbers
 * @param {...ElemNode} children - MIDI note event stream(s) to shift
 * @returns {NodeRepr_t}
 */
export function midinoteshift(
  props: { key?: string; steps: number },
  ...children: Array<ElemNode>
): NodeRepr_t {
  return createNode("midinoteshift", props, children.map(resolve));
}
