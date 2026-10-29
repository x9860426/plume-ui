#pragma once

#include "../render/RenderDevice.h"

namespace ui {

enum DynamicBgType {
    DYN_BG_NONE = 0,
    DYN_BG_POLAR_NIGHT = 1,
    DYN_BG_WATER_WAVE = 2,
    DYN_BG_HEXAGON = 3,
    DYN_BG_BUBBLE = 4,
    DYN_BG_STAR_VORTEX = 5,
    DYN_BG_AUDIO_SPECTRUM = 6,
    DYN_BG_WAVE_INTERFER = 7,
    DYN_BG_DIGITAL_RAIN = 8,
    DYN_BG_ENERGY_SHIELD = 9,
    DYN_BG_HONEYCOMB_MATRIX = 10,
    DYN_BG_RIPPLE_WAVE = 11,
    DYN_BG_SPECTRUM_WAVE = 12,
    DYN_BG_MATRIX_CODE = 13,
    DYN_BG_QUANTUM_FIELD = 14,
    DYN_BG_VORTEX_MATRIX = 15,
    DYN_BG_LAVA_MATRIX = 16,
    DYN_BG_DARK_MATTER = 17,
    DYN_BG_SPACETIME_GRID = 18,
    DYN_BG_PHOTON_MATRIX = 19,
    DYN_BG_PLASMA_MATRIX = 20,
    DYN_BG_NEON_PULSE = 21,
    DYN_BG_EMERALD_MATRIX = 22,
    DYN_BG_BAMBOO_SWAY = 23,
    DYN_BG_MECHANICAL_GEARS = 24,
    DYN_BG_PHANTOM_CHESS = 25,
    DYN_BG_STARDUST_SYMPHONY = 26,
    DYN_BG_SPACETIME_TAPESTRY = 27,
    DYN_BG_SPACETIME_BUTTERFLY = 28,
    DYN_BG_COUNT
};

struct DynamicBgConfig {
    int type;
    float speed;
    int density;
    int colorScheme;
    bool enabled;
};

extern DynamicBgConfig g_dynBg;

void updateDynamicBg(float deltaMs);
void drawDynamicBg(RenderDevice* dev, int w, int h);
void drawDynBgPreview(RenderDevice* dev, float x, float y, float w, float h, float winRatio);
void resetDynamicBg();

} // namespace ui