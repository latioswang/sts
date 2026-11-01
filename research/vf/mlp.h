// Tiny MLP inference for the learned combat value (weights exported by research/vf/train_vf.py).
// DIM -> H (ReLU) -> 1. Cost ≈ DIM*H MACs, negligible next to one engine action.
#pragma once

#include "vf_features.h"
#include <cmath>

namespace fair::vf {

#include "vf_weights.inc"   // defines VF_W_DIM, VF_H, W1[VF_H*VF_W_DIM], B1[VF_H], W2[VF_H], B2, VF_TRAINED (0/1)

static_assert(VF_W_DIM == DIM, "vf_weights.inc was exported for a different feature layout");

inline float predict(const float *x) {
    float out = B2;
    for (int h = 0; h < VF_H; ++h) {
        const float *w = W1 + h * VF_W_DIM;
        float a = B1[h];
        for (int i = 0; i < VF_W_DIM; ++i) a += w[i] * x[i];
        if (a > 0.f) out += W2[h] * a;
    }
    return out;
}

inline float value(const sts::BattleContext &bc) {
    float x[DIM];
    extract(bc, x);
    return predict(x);
}

}  // namespace fair::vf
