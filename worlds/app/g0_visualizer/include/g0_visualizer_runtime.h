#ifndef G0_VISUALIZER_RUNTIME_H
#define G0_VISUALIZER_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "g0_visualizer_assets.h"
#include "g0_world_config.h"
#include "minisnn_worlds_terrain.h"
#include "wf0_fish.h"

#define G0_VISUALIZER_TILE_PIXELS 64
#define G0_VISUALIZER_ENTITY_PIXELS 32
#define G0_VISUALIZER_TICKS_PER_SECOND 4U
#define G0_VISUALIZER_SPEED_COUNT 6U

typedef enum
{
    G0_VISUALIZER_LAYER_BASE_TERRAIN = 0,
    G0_VISUALIZER_LAYER_TERRAIN_OBJECTS,
    G0_VISUALIZER_LAYER_WORLD_ENTITIES,
    G0_VISUALIZER_LAYER_WATER_OVERLAY,
    G0_VISUALIZER_LAYER_UI,
    G0_VISUALIZER_LAYER_COUNT
} G0VisualizerLayer;

typedef enum
{
    G0_VISUALIZER_FACING_NORTH = 0,
    G0_VISUALIZER_FACING_EAST,
    G0_VISUALIZER_FACING_SOUTH,
    G0_VISUALIZER_FACING_WEST
} G0VisualizerFacing;

typedef struct G0VisualizerRuntime G0VisualizerRuntime;

typedef struct
{
    int screen_x;
    int screen_y;
} G0VisualizerScreenPoint;

/* Historical five-by-five fixture used by G0-A/G0-B tests. */
G0VisualizerRuntime *g0_visualizer_runtime_create(int blocked_first_move);
/* Interactive G0-C default: a configurable twenty-by-fifteen sandbox. */
G0VisualizerRuntime *g0_visualizer_runtime_create_sandbox(void);
G0VisualizerRuntime *g0_visualizer_runtime_create_from_config(const G0WorldConfig *config);
void g0_visualizer_runtime_destroy(G0VisualizerRuntime **runtime_ptr);
int g0_visualizer_runtime_reset(G0VisualizerRuntime *runtime);

/* Candidate build and commit are atomic: a failed apply leaves active state intact. */
int g0_visualizer_runtime_apply_config(G0VisualizerRuntime *runtime);
int g0_visualizer_runtime_new_world(G0VisualizerRuntime *runtime,
                                    uint32_t width,
                                    uint32_t height);
/* Transactional editor resize preserves the blueprint intersection. */
int g0_visualizer_runtime_resize_world(G0VisualizerRuntime *runtime,
                                       uint32_t width,
                                       uint32_t height);
int g0_visualizer_runtime_prepare_edit(G0VisualizerRuntime *runtime);
int g0_visualizer_runtime_edit_tile(G0VisualizerRuntime *runtime,
                                    G0WorldTool tool,
                                    uint32_t tile_x,
                                    uint32_t tile_y);
const G0WorldConfig *g0_visualizer_runtime_config(const G0VisualizerRuntime *runtime);

/* advance respects pause; step_once always advances exactly one real WF0 tick. */
int g0_visualizer_runtime_advance(G0VisualizerRuntime *runtime);
int g0_visualizer_runtime_step_once(G0VisualizerRuntime *runtime);
/* Scheduler input is elapsed wall time; it never reads or modifies renderer state. */
int g0_visualizer_runtime_advance_elapsed(
    G0VisualizerRuntime *runtime,
    uint32_t elapsed_milliseconds);
void g0_visualizer_runtime_toggle_paused(G0VisualizerRuntime *runtime);
void g0_visualizer_runtime_set_paused(G0VisualizerRuntime *runtime, int paused);
void g0_visualizer_runtime_toggle_grid(G0VisualizerRuntime *runtime);
void g0_visualizer_runtime_toggle_debug(G0VisualizerRuntime *runtime);
void g0_visualizer_runtime_set_grid_enabled(G0VisualizerRuntime *runtime, int enabled);
void g0_visualizer_runtime_set_debug_enabled(G0VisualizerRuntime *runtime, int enabled);
int g0_visualizer_runtime_speed_increase(G0VisualizerRuntime *runtime);
int g0_visualizer_runtime_speed_decrease(G0VisualizerRuntime *runtime);
int g0_visualizer_runtime_set_speed_index(
    G0VisualizerRuntime *runtime,
    unsigned int speed_index);
unsigned int g0_visualizer_runtime_speed_index(const G0VisualizerRuntime *runtime);
const char *g0_visualizer_runtime_speed_name(const G0VisualizerRuntime *runtime);

int g0_visualizer_runtime_is_paused(const G0VisualizerRuntime *runtime);
int g0_visualizer_runtime_grid_enabled(const G0VisualizerRuntime *runtime);
int g0_visualizer_runtime_debug_enabled(const G0VisualizerRuntime *runtime);
unsigned int g0_visualizer_runtime_ticks_per_second(const G0VisualizerRuntime *runtime);
/* Product-level episode tick starts at zero after construction/reset. */
uint64_t g0_visualizer_runtime_episode_tick(const G0VisualizerRuntime *runtime);
G0VisualizerFacing g0_visualizer_runtime_facing(const G0VisualizerRuntime *runtime);
const char *g0_visualizer_facing_name(G0VisualizerFacing facing);
/* Returns previous_facing for non-cardinal deltas. */
G0VisualizerFacing g0_visualizer_facing_from_move_delta(
    G0VisualizerFacing previous_facing,
    MiniSNNWorldsKernelPosition move_delta);
const MiniSNNWorldsTerrain *g0_visualizer_runtime_terrain(
    const G0VisualizerRuntime *runtime);
const WF0FishWorldState *g0_visualizer_runtime_state(
    const G0VisualizerRuntime *runtime);
const WF0FishTickRecord *g0_visualizer_runtime_last_record(
    const G0VisualizerRuntime *runtime);
size_t g0_visualizer_runtime_food_count(const G0VisualizerRuntime *runtime);
int g0_visualizer_runtime_food_position_at(
    const G0VisualizerRuntime *runtime,
    size_t canonical_index,
    MiniSNNWorldsKernelPosition *out_position);
int g0_visualizer_runtime_food_visible(const G0VisualizerRuntime *runtime);
int g0_visualizer_runtime_blocked_first_move(const G0VisualizerRuntime *runtime);

/* The output is ordered, immutable rendering contract. */
size_t g0_visualizer_layer_order(
    G0VisualizerLayer *out_layers,
    size_t capacity);
/* Scaled geometry used by the interactive camera. tile_pixels must be positive. */
int g0_visualizer_tile_to_screen_scaled(
    uint32_t tile_x,
    uint32_t tile_y,
    int origin_x,
    int origin_y,
    int tile_pixels,
    G0VisualizerScreenPoint *out_point);
/* Historical fixed 64 px geometry retained for G0-A/G0-B callers. */
int g0_visualizer_tile_to_screen(
    uint32_t tile_x,
    uint32_t tile_y,
    int origin_x,
    int origin_y,
    G0VisualizerScreenPoint *out_point);
int g0_visualizer_screen_to_tile(
    uint32_t width,
    uint32_t height,
    int origin_x,
    int origin_y,
    int tile_pixels,
    int screen_x,
    int screen_y,
    uint32_t *out_tile_x,
    uint32_t *out_tile_y);
int g0_visualizer_world_to_screen_scaled(
    const G0VisualizerRuntime *runtime,
    MiniSNNWorldsKernelPosition world_position,
    int origin_x,
    int origin_y,
    int tile_pixels,
    G0VisualizerScreenPoint *out_point);
/* Historical fixed 64 px geometry retained for G0-A/G0-B callers. */
int g0_visualizer_world_to_screen(
    const G0VisualizerRuntime *runtime,
    MiniSNNWorldsKernelPosition world_position,
    int origin_x,
    int origin_y,
    G0VisualizerScreenPoint *out_point);
const char *g0_visualizer_action_name(MiniSNNWorldsDomainActionType action);

#endif