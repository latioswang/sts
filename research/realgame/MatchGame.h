// Match and Keep! (the gremlin's card-matching event) has no simulator actions, so live mode's fallback picks its cards
// by rule. Cards not seen yet are listed as "card<N>", cards seen in an earlier try by name. The card turned up first in
// a try leaves the list until the try ends, so an odd count means the second pick, and the first card's name is hidden.
//
// Rule: a known pair first. Otherwise turn up a card not seen yet, because two new cards per try teach the most. On the
// last try nothing is left to learn, so a known card goes first and a card not seen yet second, which matches more often.
// Known curses are never picked on purpose. Simulated over the event's 6 pairs and 5 tries, this keeps about 1.2 good
// pairs, 0.3 Bash pairs and 0.05 curse pairs a visit; always choosing the first card keeps about 0.1 pairs in all.
#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace realgame {

constexpr int kMatchGameTries = 5;

inline bool isUnseenMatchCard(const std::string &label) {
    return label.size() > 4 && label.compare(0, 4, "card") == 0 &&
           std::all_of(label.begin() + 4, label.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

inline bool isCurseName(const std::string &label) {
    static const std::vector<std::string> curses{"ascender's bane", "clumsy", "curse of the bell", "decay", "doubt",
                                                 "injury", "necronomicurse", "normality", "pain", "parasite", "pride",
                                                 "regret", "shame", "writhe"};
    return std::find(curses.begin(), curses.end(), label) != curses.end();
}

struct MatchGameMemory {
    std::string firstPick;  // name of the card picked first in the current try; empty when it had not been seen
    int tries = 0;          // tries started
    std::vector<std::string> lastFirstPickLabels;  // the screen of the last first pick, to count a screen asked twice once
};

// labels: the event's choice list, in order and in lower case. Returns the index to choose.
inline int matchGamePick(const std::vector<std::string> &labels, MatchGameMemory &memory) {
    const int n = static_cast<int>(labels.size());
    const auto wanted = [&](int i) { return !isUnseenMatchCard(labels[i]) && !isCurseName(labels[i]); };
    int unseen = -1;
    for (int i = 0; i < n && unseen < 0; ++i) {
        if (isUnseenMatchCard(labels[i])) unseen = i;
    }
    if (n % 2 == 1) {  // second pick of a try
        for (int i = 0; i < n && !memory.firstPick.empty(); ++i) {
            if (labels[i] == memory.firstPick) return i;
        }
        if (unseen >= 0) return unseen;
        for (int i = 0; i < n; ++i) {
            if (!isCurseName(labels[i])) return i;
        }
        return 0;
    }
    // Every try changes the grid (a pair leaves, or the cards turned up get their names), so an unchanged grid is the same
    // pick asked again, for example after a refused command.
    if (labels != memory.lastFirstPickLabels) ++memory.tries;
    memory.lastFirstPickLabels = labels;
    memory.firstPick.clear();
    for (int i = 0; i < n; ++i) {
        if (wanted(i) && std::count(labels.begin(), labels.end(), labels[i]) >= 2) {
            memory.firstPick = labels[i];
            return i;
        }
    }
    if (memory.tries >= kMatchGameTries || unseen < 0) {
        for (int i = 0; i < n; ++i) {
            if (wanted(i)) {
                memory.firstPick = labels[i];
                return i;
            }
        }
    }
    return unseen >= 0 ? unseen : 0;
}

}  // namespace realgame
