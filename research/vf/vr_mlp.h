// Phase R: runtime-loaded run value V(run state) ≈ E[final score], score = min(floor, 51)/51 + win.
// Same text format as sv_mlp.h ("dim nlayers trained", then per layer "in out", weights out x in, biases),
// over the 666 run features of run_features.h. Files: research/models/vr_<id>.txt (VR_MODEL_DIR overrides).
#pragma once

#include "run_features.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace fair::vr {

struct Layer { int in = 0, out = 0; std::vector<float> W, B; };
struct Model { int dim = 0; bool trained = false; std::vector<Layer> layers; };

inline std::string modelPath(int n) {
    const char *dir = std::getenv("VR_MODEL_DIR");
    return std::string(dir ? dir : "/home/user/sts/research/models") + "/vr_" + std::to_string(n) + ".txt";
}

inline const Model &model(int n) {
    static std::map<int, Model> cache;
    auto it = cache.find(n);
    if (it != cache.end()) return it->second;
    Model m;
    if (std::FILE *f = std::fopen(modelPath(n).c_str(), "r")) {
        int nl = 0, trained = 0;
        if (std::fscanf(f, "%d %d %d", &m.dim, &nl, &trained) == 3 && (m.dim == rv::DIM || m.dim == rv::DIM2) && nl > 0 && nl < 16) {
            bool ok = true;
            for (int l = 0; l < nl && ok; ++l) {
                Layer L;
                ok = std::fscanf(f, "%d %d", &L.in, &L.out) == 2 && L.in > 0 && L.out > 0 && L.in <= 8192 && L.out <= 8192;
                if (!ok) break;
                L.W.resize(static_cast<size_t>(L.in) * L.out); L.B.resize(L.out);
                for (float &w : L.W) ok = ok && std::fscanf(f, "%f", &w) == 1;
                for (float &b : L.B) ok = ok && std::fscanf(f, "%f", &b) == 1;
                m.layers.push_back(std::move(L));
            }
            m.trained = ok && trained == 1 && !m.layers.empty() && m.layers.front().in == m.dim && m.layers.back().out == 1;
        }
        std::fclose(f);
    }
    return cache.emplace(n, std::move(m)).first->second;
}

inline float predict(const Model &m, const float *x) {
    std::vector<float> a(x, x + m.dim), b;
    for (size_t l = 0; l < m.layers.size(); ++l) {
        const Layer &L = m.layers[l];
        b.assign(L.out, 0.f);
        for (int o = 0; o < L.out; ++o) {
            const float *w = L.W.data() + static_cast<size_t>(o) * L.in;
            float acc = L.B[o];
            for (int i = 0; i < L.in; ++i) acc += w[i] * a[i];
            b[o] = (l + 1 < m.layers.size() && acc < 0.f) ? 0.f : acc;
        }
        a.swap(b);
    }
    return a[0];
}

// Phase F14: keys mode of the running agent (set by FairAgent from param keys); used only by DIM2 (features v2) models
inline float g_keysMode = 0.f;

inline float value(const Model &m, const sts::GameContext &gc) {
    float x[rv::DIM2];
    rv::extract(gc, x);
    if (m.dim == rv::DIM2) rv::extractKeys(gc, g_keysMode, x + rv::DIM);
    return predict(m, x);
}

}  // namespace fair::vr
