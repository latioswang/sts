// Phase C v3: interventional deck value. f(run state) -> [P(win vs the act boss), HP fraction left
// after it, P(win vs a sampled hallway/elite fight of this act), HP fraction left after it].
// Trained by research/vf/train_bf.py on fights the simulator played from logged run states (with and
// without each candidate card / upgrade / removal — same fight seeds for every option of a decision).
// Inputs are the observable run features (run_features.h); nothing hidden is used at decision time.
#pragma once

#include "run_features.h"
#include <cmath>

namespace fair::bf {

#include "bf_weights.inc"  // BF_W_DIM, BF_H, BF_O, BF_TRAINED, BF_W1, BF_B1, BF_W2, BF_B2

static_assert(BF_W_DIM == rv::DIM, "bf_weights.inc was exported for a different feature layout");

inline void predictFeatures(const float *x, float *out) {
    float h[BF_H];
    for (int j = 0; j < BF_H; ++j) {
        const float *w = BF_W1 + j * BF_W_DIM;
        float a = BF_B1[j];
        for (int i = 0; i < BF_W_DIM; ++i) a += w[i] * x[i];
        h[j] = a > 0.f ? a : 0.f;
    }
    for (int o = 0; o < BF_O; ++o) {
        const float *w = BF_W2 + o * BF_H;
        float z = BF_B2[o];
        for (int j = 0; j < BF_H; ++j) z += w[j] * h[j];
        out[o] = 1.f / (1.f + std::exp(-z));
    }
}

inline void predict(const sts::GameContext &gc, float *out) {
    float x[rv::DIM];
    rv::extract(gc, x);
    predictFeatures(x, out);
}

}  // namespace fair::bf
