// MLP move prior inference (Phase B). Applied only at shallow tree depth (root, depth 1) because
// it costs ~(204+16+4)*64 MACs per legal action — a few simulations' worth per node.
#pragma once

#include "vf_features.h"
#include "policy_features.h"
#include <cmath>

namespace fair::pfm {

#include "pfm_weights.inc"  // PFM_S_DIM, PFM_K, PFM_T, PFM_IN, PFM_H, PFM_N_TOK, PFM_TRAINED, PFM_E, PFM_EUP, PFM_W1, PFM_B1, PFM_W2, PFM_B2

static_assert(PFM_S_DIM == vf::DIM, "pfm_weights.inc was exported for a different state feature layout");
static_assert(PFM_N_TOK == pf::N_TOK, "pfm_weights.inc token vocabulary mismatch");

// writes softmax priors for the n legal actions into out[0..n)
inline void priors(const sts::BattleContext &bc, const sts::search::Action *acts, int n, float *out) {
    float x[PFM_IN];
    vf::extract(bc, x);  // first PFM_S_DIM entries
    float mx = -1e30f;
    for (int i = 0; i < n; ++i) {
        bool up = false;
        const int tok = pf::token(bc, acts[i], up);
        for (int k = 0; k < PFM_K; ++k) x[PFM_S_DIM + k] = PFM_E[tok * PFM_K + k] + (up ? PFM_EUP[k] : 0.f);
        pf::targetFeats(bc, acts[i], x + PFM_S_DIM + PFM_K);
        float lg = PFM_B2;
        for (int h = 0; h < PFM_H; ++h) {
            const float *w = PFM_W1 + h * PFM_IN;
            float a = PFM_B1[h];
            for (int j = 0; j < PFM_IN; ++j) a += w[j] * x[j];
            if (a > 0.f) lg += PFM_W2[h] * a;
        }
        out[i] = lg;
        mx = std::max(mx, lg);
    }
    float z = 0.f;
    for (int i = 0; i < n; ++i) { out[i] = std::exp(out[i] - mx); z += out[i]; }
    for (int i = 0; i < n; ++i) out[i] /= z;
}

}  // namespace fair::pfm
