/**
 * Type-level test for metro event type.
 *
 * This test verifies that 'metro' is recognized as a valid event name
 * in EventTypes, so that core.on('metro', callback) compiles without
 * TypeScript errors.
 *
 * See: https://github.com/elemaudio/elementary/issues/73
 */
import EventEmitter from '../src/Events';

// This should compile without errors: metro is a valid event type
const emitter = new EventEmitter();

emitter.on('metro', (data) => {
  // The metro event payload should have an optional source field
  const source: string | undefined = data.source;
});
