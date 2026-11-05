// Phase C v3: interventional deck value. f(run state) -> [P(win vs the act boss), HP fraction left
// after it, P(win vs a sampled hallway/elite fight of this act), HP fraction left after it, (g1+:)
// damage fraction dealt to the act boss, P(win) / HP left / damage fraction vs a next-act boss].
// Trained by research/vf/train_bf.py on fights the simulator played from logged run states.
// MODELS ARE DATA, NOT CODE: weights live in research/models/bf_<name>.txt (text: "dim h o trained"
// then W1[h*dim] B1[h] W2[o*h] B2[o]) and are chosen per run by the parameter bf_model=<n>
// (-> bf_g<n>.txt), so every recorded run stays reproducible after new models are trained.
#pragma once

#include "run_features.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace fair::bf {

struct Model {
    int dim = 0, h = 0, o = 0;
    bool trained = false;
    std::vector<float> W1, B1, W2, B2;
};

inline std::string modelPath(int n) {
    const char *dir = std::getenv("BF_MODEL_DIR");
    return std::string(dir ? dir : "/home/user/sts/research/models") + "/bf_g" + std::to_string(n) + ".txt";
}

// loaded once per process per model id; a missing/invalid file is an untrained model (hooks become no-ops)
inline const Model &model(int n) {
    static std::map<int, Model> cache;
    auto it = cache.find(n);
    if (it != cache.end()) return it->second;
    Model m;
    if (std::FILE *f = std::fopen(modelPath(n).c_str(), "r")) {
        int trained = 0;
        if (std::fscanf(f, "%d %d %d %d", &m.dim, &m.h, &m.o, &trained) == 4 && m.dim == rv::DIM && m.h > 0 && m.o > 0) {
            auto read = [&](std::vector<float> &v, size_t count) {
                v.resize(count);
                for (size_t i = 0; i < count; ++i) if (std::fscanf(f, "%f", &v[i]) != 1) return false;
                return true;
            };
            const bool ok = read(m.W1, (size_t)m.h * m.dim) && read(m.B1, m.h) && read(m.W2, (size_t)m.o * m.h) && read(m.B2, m.o);
            m.trained = ok && trained != 0;
            if (!ok) m = Model();
        }
        std::fclose(f);
    }
    return cache.emplace(n, std::move(m)).first->second;
}

inline void predictFeatures(const Model &m, const float *x, float *out) {
    std::vector<float> hbuf(m.h);
    for (int j = 0; j < m.h; ++j) {
        const float *w = m.W1.data() + (size_t)j * m.dim;
        float a = m.B1[j];
        for (int i = 0; i < m.dim; ++i) a += w[i] * x[i];
        hbuf[j] = a > 0.f ? a : 0.f;
    }
    for (int k = 0; k < m.o; ++k) {
        const float *w = m.W2.data() + (size_t)k * m.h;
        float z = m.B2[k];
        for (int j = 0; j < m.h; ++j) z += w[j] * hbuf[j];
        out[k] = 1.f / (1.f + std::exp(-z));
    }
}

inline void predict(const Model &m, const sts::GameContext &gc, float *out) {
    float x[rv::DIM];
    rv::extract(gc, x);
    predictFeatures(m, x, out);
}

}  // namespace fair::bf
