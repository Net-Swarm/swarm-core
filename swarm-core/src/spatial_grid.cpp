#include "spatial_grid.h"

#include <cstddef>  // offsetof for the field-order static_assert
#include <cstdint>  // UINT32_MAX for the overflow check on gridCols * gridRows

namespace swarm::core {

// Pin field placement so the broad-phase ticket can rely on the documented
// scalar order (cellCount precedes the dimension fields). Layout reordering
// would silently shift offsets observed by integration tests; the offset check
// fails the build instead.
static_assert(offsetof(SpatialGrid, cellStartOrHead) == 0,
              "cellStartOrHead must be the first pointer field (CORE-7)");

void zero(SpatialGrid& g) {
    g.cellStartOrHead = nullptr;
    g.cellCountOrNext = nullptr;
    g.particleCell    = nullptr;
    g.particleNext    = nullptr;
    g.cellCount       = 0u;
    g.gridCols        = 0u;
    g.gridRows        = 0u;
    g.maxParticles    = 0u;
}

bool validate(const SpatialGridConfig& cfg, u32* outCellCount) {
    if (cfg.gridCols == 0u || cfg.gridRows == 0u) {
        return false;
    }
    if (cfg.gridCols > SWARM_CORE_MAX_GRID_COLS) {
        return false;
    }
    if (cfg.gridRows > SWARM_CORE_MAX_GRID_ROWS) {
        return false;
    }
    // Detect u32 overflow before the multiply. gridRows is non-zero here.
    if (cfg.gridCols > UINT32_MAX / cfg.gridRows) {
        return false;
    }
    const u32 cellCount = cfg.gridCols * cfg.gridRows;
    if (cellCount > SWARM_CORE_MAX_GRID_CELLS) {
        return false;
    }
    if (outCellCount != nullptr) {
        *outCellCount = cellCount;
    }
    return true;
}

} // namespace swarm::core
