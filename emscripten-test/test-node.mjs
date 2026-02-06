import createModule from './test.mjs';

const Module = await createModule();
console.log('Add result:', Module._add(5, 3));
console.log('Greet:', Module.ccall('greet', 'string', [], []));
