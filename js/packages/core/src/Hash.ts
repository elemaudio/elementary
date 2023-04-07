import shallowEqual from 'shallowequal';

export function hashNumber(seed, n) {
  if (n <= 0)
    return seed;

  let hash = seed;

  hash = ((hash << 5) - hash) + n;
  hash |= 0;

  return hash < 0 ? hash * -2 : hash;
}

export function hashString(seed, s) {
  if (s.length === 0)
    return seed;

  let hash = seed;

  for (let i = 0; i < s.length; ++i) {
    hash = ((hash << 5) - hash) + s.charCodeAt(i);
    hash |= 0;
  }

  return hash < 0 ? hash * -2 : hash;
}

export function hashNode(kind, props, childHashes) {
  const typeHash = hashString(0, kind);
  const hasKeyProp = props.hasOwnProperty('key') && typeof props.key === 'string';

  const typePropsHash = hasKeyProp
    ? hashString(typeHash, props.key)
    : hashString(typeHash, JSON.stringify(props));

  return childHashes.reduce(function(acc, childHash) {
    return hashNumber(acc, childHash);
  }, typePropsHash);
}

export function hashMemoInputs(props, childHashes) {
  const hasMemoKeyProp = props.hasOwnProperty('memoKey') && typeof props.memoKey === 'string';

  const propsHash = hasMemoKeyProp
    ? hashString(0, props.memoKey)
    : hashString(0, JSON.stringify(props));

  return childHashes.reduce(function(acc, childHash) {
    return hashNumber(acc, childHash);
  }, propsHash);
}

export function hashToHexId(hash) {
  let strVal = hash.toString(16);

  while (strVal.length < 8)
    strVal = '0' + strVal;

  return strVal;
}

export function hexIdToHash(hexId) {
  return parseInt(hexId, 16);
}

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
      }
    }
  }
}
