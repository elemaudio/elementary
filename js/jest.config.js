/** @type {import('ts-jest').JestConfigWithTsJest} */
module.exports = {
  verbose: true,
  // preset: 'ts-jest/presets/js-with-ts',
  testEnvironment: 'node',
  transform: {
    '^.+\\.tsx?$': [
      'ts-jest/presets/js-with-ts',
      {
        isolatedModules: true,
      },
    ],
  }
};