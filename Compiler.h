#pragma once
// =============================================================================
// Compiler.h — Top-level Regex Compiler
// =============================================================================
// Ties together all four phases: Parse → NFA → DFA → Minimized DFA.
// Provides a single `Regex` class for end-user consumption.
// =============================================================================

#include "Parser.h"
#include "Nfa.h"
#include "Dfa.h"
#include <string>
#include <chrono>
#include <iostream>
#include <iomanip>

namespace regex_engine {

// ---- Compilation Statistics -------------------------------------------------
struct CompileStats {
    double parse_us   = 0;  // microseconds for parsing
    double nfa_us     = 0;  // microseconds for Thompson's construction
    double dfa_us     = 0;  // microseconds for subset construction
    double min_us     = 0;  // microseconds for Hopcroft minimization
    double total_us   = 0;

    int nfa_states     = 0;
    int dfa_states     = 0;
    int min_dfa_states = 0;

    void print() const {
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "  Parse:        " << parse_us << " us\n";
        std::cout << "  NFA build:    " << nfa_us   << " us  ("
                  << nfa_states << " states)\n";
        std::cout << "  DFA build:    " << dfa_us   << " us  ("
                  << dfa_states << " states)\n";
        std::cout << "  DFA minimize: " << min_us   << " us  ("
                  << min_dfa_states << " states)\n";
        std::cout << "  Total:        " << total_us  << " us\n";
    }
};

// ---- Regex (compiled, ready-to-match) ---------------------------------------
class Regex {
public:
    // Compile a regex pattern.  Throws std::runtime_error on invalid pattern.
    explicit Regex(const std::string& pattern)
        : pattern_(pattern)
    {
        using clk = std::chrono::high_resolution_clock;

        auto t0 = clk::now();

        // Phase 1 — Parse
        auto ast = Parser::parse(pattern);
        auto t1 = clk::now();

        // Phase 2 — Thompson's NFA
        Nfa nfa = Nfa::fromAst(ast.get());
        auto t2 = clk::now();

        // Phase 3 — Subset construction
        Dfa rawDfa = Dfa::fromNfa(nfa);
        auto t3 = clk::now();

        // Phase 4 — Hopcroft minimization
        dfa_ = rawDfa.minimize();
        auto t4 = clk::now();

        // Record stats
        auto us = [](auto a, auto b) {
            return std::chrono::duration<double, std::micro>(b - a).count();
        };
        stats_.parse_us       = us(t0, t1);
        stats_.nfa_us         = us(t1, t2);
        stats_.dfa_us         = us(t2, t3);
        stats_.min_us         = us(t3, t4);
        stats_.total_us       = us(t0, t4);
        stats_.nfa_states     = static_cast<int>(nfa.states.size());
        stats_.dfa_states     = static_cast<int>(rawDfa.stateCount());
        stats_.min_dfa_states = static_cast<int>(dfa_.stateCount());
    }

    // ---- Match (O(n) time, O(1) space) --------------------------------------
    bool match(const std::string& text) const {
        return dfa_.match(text);
    }

    // ---- Accessors ----------------------------------------------------------
    const std::string&  pattern()  const { return pattern_; }
    const CompileStats& stats()    const { return stats_; }
    const Dfa&          dfa()      const { return dfa_; }

    // Dump compiled DFA
    std::string dump() const { return dfa_.dump(); }

private:
    std::string   pattern_;
    Dfa           dfa_;
    CompileStats  stats_;
};

} // namespace regex_engine
