#pragma once
// =============================================================================
// Dfa.h — Phase 3 & 4: Subset Construction (NFA→DFA) + Hopcroft Minimization
// =============================================================================
// Phase 3: Powerset / subset construction converts an NFA into a DFA by
//          treating sets of NFA states as single DFA states.
// Phase 4: Hopcroft's algorithm minimizes the DFA by merging equivalent states.
// =============================================================================

#include "Nfa.h"
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <string>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <functional>

namespace regex_engine {

// ---- DFA State --------------------------------------------------------------
struct DfaState {
    int id;
    bool is_accept = false;
    std::map<char, int> transitions;  // symbol → target DFA state ID
};

// ---- DFA --------------------------------------------------------------------
class Dfa {
public:
    std::vector<DfaState> states;
    int start_state = 0;
    std::set<char> alpha;

    // ========================================================================
    // Phase 3 — Subset Construction
    // ========================================================================
    static Dfa fromNfa(const Nfa& nfa) {
        Dfa dfa;
        dfa.alpha = nfa.alphabet();

        // Map from NFA state-set → DFA state ID
        std::map<std::set<int>, int> stateMap;
        std::queue<std::set<int>> worklist;

        // Initial DFA state = ε-closure({nfa.start_state})
        std::set<int> startSet = nfa.epsilonClosure({nfa.start_state});
        int startId = dfa.addState(startSet, nfa);
        stateMap[startSet] = startId;
        dfa.start_state = startId;
        worklist.push(startSet);

        while (!worklist.empty()) {
            std::set<int> current = worklist.front();
            worklist.pop();
            int currentId = stateMap[current];

            for (char c : dfa.alpha) {
                std::set<int> moved = nfa.move(current, c);
                if (moved.empty()) continue;

                std::set<int> next = nfa.epsilonClosure(moved);
                if (next.empty()) continue;

                int nextId;
                auto it = stateMap.find(next);
                if (it == stateMap.end()) {
                    nextId = dfa.addState(next, nfa);
                    stateMap[next] = nextId;
                    worklist.push(next);
                } else {
                    nextId = it->second;
                }
                dfa.states[currentId].transitions[c] = nextId;
            }
        }
        return dfa;
    }

    // ========================================================================
    // Phase 4 — Hopcroft's DFA Minimization
    // ========================================================================
    Dfa minimize() const {
        // 1. Collect accepting and non-accepting sets
        std::set<int> accepting, nonAccepting;
        for (auto& s : states) {
            if (s.is_accept) accepting.insert(s.id);
            else             nonAccepting.insert(s.id);
        }

        // Initial partition
        std::vector<std::set<int>> partitions;
        if (!accepting.empty())    partitions.push_back(accepting);
        if (!nonAccepting.empty()) partitions.push_back(nonAccepting);

        // Worklist
        std::queue<std::set<int>> W;
        if (!accepting.empty())    W.push(accepting);
        if (!nonAccepting.empty()) W.push(nonAccepting);

        // Iteratively refine
        while (!W.empty()) {
            std::set<int> A = W.front(); W.pop();

            for (char c : alpha) {
                // X = set of states that transition into A on symbol c
                std::set<int> X;
                for (auto& s : states) {
                    auto it = s.transitions.find(c);
                    if (it != s.transitions.end() && A.count(it->second)) {
                        X.insert(s.id);
                    }
                }
                if (X.empty()) continue;

                std::vector<std::set<int>> newPartitions;
                for (auto& Y : partitions) {
                    std::set<int> inter, diff;
                    for (int y : Y) {
                        if (X.count(y)) inter.insert(y);
                        else            diff.insert(y);
                    }

                    if (!inter.empty() && !diff.empty()) {
                        newPartitions.push_back(inter);
                        newPartitions.push_back(diff);

                        // Update worklist
                        // Check if Y is in W
                        // (simplified: always add the smaller one)
                        if (inter.size() <= diff.size()) {
                            W.push(inter);
                        } else {
                            W.push(diff);
                        }
                    } else {
                        newPartitions.push_back(Y);
                    }
                }
                partitions = std::move(newPartitions);
            }
        }

        // Build minimized DFA from partitions
        Dfa minDfa;
        minDfa.alpha = alpha;

        // Map: old state ID → partition index
        std::map<int, int> stateToPartition;
        for (int i = 0; i < (int)partitions.size(); ++i) {
            for (int s : partitions[i]) {
                stateToPartition[s] = i;
            }
        }

        // Create minimized states
        for (int i = 0; i < (int)partitions.size(); ++i) {
            DfaState ds;
            ds.id = i;
            ds.is_accept = false;
            for (int s : partitions[i]) {
                if (states[s].is_accept) { ds.is_accept = true; break; }
            }
            // Transitions: pick any representative from the partition
            int rep = *partitions[i].begin();
            for (auto& [sym, target] : states[rep].transitions) {
                ds.transitions[sym] = stateToPartition[target];
            }
            minDfa.states.push_back(ds);
        }
        minDfa.start_state = stateToPartition[start_state];

        return minDfa;
    }

    // ========================================================================
    // Matching Engine — O(n) time, O(1) space per character, zero backtracking
    // ========================================================================
    bool match(const std::string& text) const {
        int curr = start_state;
        for (char c : text) {
            auto it = states[curr].transitions.find(c);
            if (it == states[curr].transitions.end()) return false;
            curr = it->second;
        }
        return states[curr].is_accept;
    }

    // ---- Pretty-print -------------------------------------------------------
    std::string dump() const {
        std::ostringstream oss;
        oss << "DFA: start=S" << start_state
            << "  states=" << states.size() << "\n";
        for (auto& s : states) {
            oss << "  S" << s.id;
            if (s.is_accept) oss << " [ACCEPT]";
            oss << "\n";
            for (auto& [sym, tgt] : s.transitions) {
                oss << "    '" << sym << "' -> S" << tgt << "\n";
            }
        }
        return oss.str();
    }

    // Number of states
    size_t stateCount() const { return states.size(); }

private:
    int addState(const std::set<int>& nfaStates, const Nfa& nfa) {
        DfaState ds;
        ds.id = static_cast<int>(states.size());
        ds.is_accept = nfaStates.count(nfa.accept_state) > 0;
        states.push_back(ds);
        return ds.id;
    }
};

} // namespace regex_engine
