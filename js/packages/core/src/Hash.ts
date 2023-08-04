import shallowEqual from 'shallowequal';


export function updateNodeProps(renderer, hash, prevProps, nextProps) {
  for (let key in nextProps) {
    if (nextProps.hasOwnProperty(key)) {
      const value = nextProps[key];

      // We need to shallowEqual here to catch things like a `seq` node's sequence property
      // changing. Strict equality will always show two different arrays, even if their contents
      // describe the same sequence. Shallow equality solves that
      const shouldUpdate = !prevProps.hasOwnProperty(key) || !shallowEqual(prevProps[key], value);

      if (shouldUpdate) {
        // A quick helper for end users before we actually write the value to native
        const seemsInvalid = typeof value === 'undefined'
          || value === null
          || (typeof value === 'number' && isNaN(value))
          || (typeof value === 'number' && !isFinite(value));

        if (seemsInvalid) {
          console.warn(`Warning: applying a potentially erroneous property value. ${key}: ${value}`)
        }

        renderer.setProperty(hash, key, value);
        prevProps[key] = value;
      }
    }
  }
}
