// Proof-of-build smoke harness for the swarm-core Wasm bridge (CORE-9).
//
// Usage:
//   node smoke.mjs <path-to-swarm-core.js>
//
// Run via CTest:
//   ctest --test-dir build-wasm -V -R swarm-core-wasm-smoke
//
// Exit code 0 = all checks passed. Non-zero on first failure.

import { createRequire } from 'node:module';
import { resolve } from 'node:path';
import { existsSync } from 'node:fs';

const require = createRequire(import.meta.url);

const args = process.argv.slice(2);
if (args.length === 0) {
    console.error('Usage: node smoke.mjs <path-to-swarm-core.js>');
    process.exit(2);
}

const modulePath = resolve(args[0]);
if (!existsSync(modulePath)) {
    console.error(`Module not found: ${modulePath}`);
    process.exit(2);
}

const createSwarmCoreModule = require(modulePath);
const Module = await createSwarmCoreModule();

let failures = 0;
function check(cond, msg) {
    if (cond) {
        console.log(`PASS  ${msg}`);
    } else {
        console.error(`FAIL  ${msg}`);
        failures++;
    }
}

// ABI version matches the constant the core was compiled against.
const abi = Module._swarm_get_abi_version();
check(abi === 1, `abi_version === 1 (got ${abi})`);

// Capacity policy is exposed and pinned.
const defaultParticles = Module._swarm_get_default_particles();
const maxParticles     = Module._swarm_get_max_supported_particles();
check(defaultParticles === 16384,  `default_particles === 16384 (got ${defaultParticles})`);
check(maxParticles     === 131072, `max_supported_particles === 131072 (got ${maxParticles})`);

// init() rejects 0 and values above the hard cap.
check(Module._swarm_init(0)              === 0, 'init(0) rejected');
check(Module._swarm_init(maxParticles+1) === 0, `init(${maxParticles + 1}) rejected`);

// pre-init buffer accessors must be safe; they return null (offset 0).
check(Module._swarm_get_pos_x() === 0, 'pos_x offset is 0 before init');
check(Module._swarm_reset()     === 0, 'reset() rejected before init');
check(Module._swarm_update(0.0) === 0, 'update() rejected before init');

// Happy path.
check(Module._swarm_init(defaultParticles) === 1, `init(${defaultParticles}) succeeded`);
check(Module._swarm_get_particle_capacity() === defaultParticles, `capacity == ${defaultParticles}`);
check(Module._swarm_get_particle_count()    === 0,                'count starts at 0');

// 10 buffer offsets must be non-zero, 16-byte aligned, pairwise distinct.
const offsets = [
    ['pos_x',   Module._swarm_get_pos_x()],
    ['pos_y',   Module._swarm_get_pos_y()],
    ['vel_x',   Module._swarm_get_vel_x()],
    ['vel_y',   Module._swarm_get_vel_y()],
    ['color_r', Module._swarm_get_color_r()],
    ['color_g', Module._swarm_get_color_g()],
    ['color_b', Module._swarm_get_color_b()],
    ['color_a', Module._swarm_get_color_a()],
    ['life',    Module._swarm_get_life()],
    ['flags',   Module._swarm_get_flags()],
];

const seen = new Set();
for (const [name, off] of offsets) {
    check(off !== 0,        `${name} offset != 0 (got ${off})`);
    check((off & 0xF) === 0, `${name} offset 16-byte aligned (got ${off})`);
    check(!seen.has(off),   `${name} offset is unique`);
    seen.add(off);
}

// Typed-array view spans the full capacity without copying.
const posXView = new Float32Array(Module.HEAPU8.buffer, Module._swarm_get_pos_x(), defaultParticles);
check(posXView.length === defaultParticles, `Float32Array view length === ${defaultParticles}`);

// Pointer identity is stable across update (CORE-5/6 contract; CORE-9 must preserve it).
const posXBefore = Module._swarm_get_pos_x();
check(Module._swarm_update(0.016) === 1, 'update(0.016) returned 1');
const posXAfter  = Module._swarm_get_pos_x();
check(posXAfter === posXBefore, `pos_x offset stable across update (${posXBefore} == ${posXAfter})`);

// Reset preserves pointer identity, zeros count.
check(Module._swarm_reset() === 1, 'reset() returned 1');
check(Module._swarm_get_pos_x() === posXBefore, 'pos_x offset stable across reset');
check(Module._swarm_get_particle_count() === 0, 'count == 0 after reset');

// Shutdown is void; downstream queries should report zero capacity.
Module._swarm_shutdown();
check(Module._swarm_get_particle_capacity() === 0, 'capacity == 0 after shutdown');

if (failures > 0) {
    console.error(`\n${failures} check(s) failed.`);
    process.exit(1);
}
console.log('\nAll smoke checks passed.');
process.exit(0);
