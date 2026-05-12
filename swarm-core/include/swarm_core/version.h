#ifndef SWARM_CORE_VERSION_H
#define SWARM_CORE_VERSION_H

// Public ABI version for swarm-core. See Docs/CORE_EXTRACTION_ARCHITECTURE.md §9
// for the bump policy. Shells should compare this against swarm_core_abi_version()
// at runtime and refuse to run on mismatch.
#define SWARM_CORE_ABI_VERSION 1u

#endif // SWARM_CORE_VERSION_H
