// Inference for V_run = P(win | run state) (Phase C). Weights exported by research/vf/train_run.py.
#pragma once

#include "run_features.h"
#include <cmath>

namespace fair::rv {

#include "rv_weights.inc"  // RV_W_DIM, RV_H, RV_TRAINED, RV_W1, RV_B1, RV_W2, RV_B2 (output is a logit)

static_assert(RV_W_DIM == DIM, "rv_weights.inc was exported for a different feature layout");

inline float logit(const float *x) {
    float out = RV_B2;
    for (int h = 0; h < RV_H; ++h) {
        const float *w = RV_W1 + h * RV_W_DIM;
        float a = RV_B1[h];
        for (int i = 0; i < RV_W_DIM; ++i) a += w[i] * x[i];
        if (a > 0.f) out += RV_W2[h] * a;
    }
    return out;
}

inline float pWin(const sts::GameContext &gc) {
    float x[DIM];
    extract(gc, x);
    return 1.f / (1.f + std::exp(-logit(x)));
}

}  // namespace fair::rv
