import { el } from '@elemaudio/core';

import { createBenchmarkScenario } from './benchmark-utils.js';

createBenchmarkScenario('RendererBenchmarkSineNoKey', (i) => el.cycle(440 + i));
