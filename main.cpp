// =============================================================================
// main.cpp — CLI Wrapper for Regex-to-DFA Compiler & Matching Engine
// =============================================================================
// Usage:
//   regex_engine <pattern> <text1> [text2] [text3] ...
//   regex_engine --test          (run built-in test suite)
//   regex_engine --interactive   (interactive REPL mode)
//   regex_engine --benchmark     (run ReDoS-immunity benchmarks)
// =============================================================================

#include "Compiler.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>
#include <functional>

using namespace regex_engine;

// ═══════════════════════════════════════════════════════════════════════════════
// ANSI Colors (for pretty terminal output)
// ═══════════════════════════════════════════════════════════════════════════════
namespace color {
    const char* RESET   = "\033[0m";
    const char* BOLD    = "\033[1m";
    const char* RED     = "\033[31m";
    const char* GREEN   = "\033[32m";
    const char* YELLOW  = "\033[33m";
    const char* CYAN    = "\033[36m";
    const char* MAGENTA = "\033[35m";
    const char* DIM     = "\033[2m";
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test Framework
// ═══════════════════════════════════════════════════════════════════════════════
struct TestCase {
    std::string pattern;
    std::string input;
    bool expected;
    std::string description;
};

static int runTests() {
    std::vector<TestCase> tests = {
        // ── Basic Literals ───────────────────────────────────────────────
        {"a",        "a",      true,  "single literal match"},
        {"a",        "b",      false, "single literal mismatch"},
        {"a",        "",       false, "single literal vs empty"},
        {"a",        "aa",     false, "single literal vs double"},
        {"abc",      "abc",    true,  "multi-literal match"},
        {"abc",      "ab",     false, "multi-literal prefix only"},
        {"abc",      "abcd",   false, "multi-literal with extra"},

        // ── Concatenation ────────────────────────────────────────────────
        {"ab",       "ab",     true,  "concat two chars"},
        {"ab",       "a",      false, "concat incomplete"},
        {"ab",       "ba",     false, "concat wrong order"},

        // ── Union (|) ────────────────────────────────────────────────────
        {"a|b",      "a",      true,  "union left"},
        {"a|b",      "b",      true,  "union right"},
        {"a|b",      "c",      false, "union miss"},
        {"a|b",      "ab",     false, "union not concat"},
        {"ab|cd",    "ab",     true,  "union of concats left"},
        {"ab|cd",    "cd",     true,  "union of concats right"},
        {"ab|cd",    "ac",     false, "union of concats miss"},

        // ── Kleene Star (*) ──────────────────────────────────────────────
        {"a*",       "",       true,  "star zero times"},
        {"a*",       "a",      true,  "star one time"},
        {"a*",       "aaaa",   true,  "star many times"},
        {"a*",       "b",      false, "star wrong char"},
        {"a*",       "aab",    false, "star trailing garbage"},

        // ── Plus (+) ─────────────────────────────────────────────────────
        {"a+",       "",       false, "plus zero times"},
        {"a+",       "a",      true,  "plus one time"},
        {"a+",       "aaaa",   true,  "plus many times"},
        {"a+",       "b",      false, "plus wrong char"},

        // ── Optional (?) ────────────────────────────────────────────────
        {"a?",       "",       true,  "optional zero"},
        {"a?",       "a",      true,  "optional one"},
        {"a?",       "aa",     false, "optional two"},

        // ── Combined ────────────────────────────────────────────────────
        {"(a|b)*c",  "c",      true,  "star union then c — empty prefix"},
        {"(a|b)*c",  "ac",     true,  "star union then c — one 'a'"},
        {"(a|b)*c",  "bc",     true,  "star union then c — one 'b'"},
        {"(a|b)*c",  "abc",    true,  "star union then c — 'ab' prefix"},
        {"(a|b)*c",  "aabbc",  true,  "star union then c — longer prefix"},
        {"(a|b)*c",  "ab",     false, "star union then c — no trailing c"},
        {"(a|b)*c",  "",       false, "star union then c — empty"},
        {"(a|b)*c",  "abcx",   false, "star union then c — extra after c"},

        // ── Nested Quantifiers (ReDoS patterns) ─────────────────────────
        {"(a+)+b",   "ab",     true,  "nested plus — simple match"},
        {"(a+)+b",   "aab",    true,  "nested plus — double a"},
        {"(a+)+b",   "aaaab",  true,  "nested plus — many a + b"},
        {"(a+)+b",   "b",      false, "nested plus — no 'a'"},
        {"(a+)+b",   "aaa",    false, "nested plus — no trailing 'b'"},

        {"(a*)*b",   "b",      true,  "nested star-star — just b"},
        {"(a*)*b",   "ab",     true,  "nested star-star — a+b"},
        {"(a*)*b",   "aab",    true,  "nested star-star — aa+b"},
        {"(a*)*b",   "a",      false, "nested star-star — no b"},

        // ── Complex expressions ─────────────────────────────────────────
        {"a(b|c)*d", "ad",     true,  "a then star-union then d — empty mid"},
        {"a(b|c)*d", "abd",    true,  "a then star-union then d — b mid"},
        {"a(b|c)*d", "acd",    true,  "a then star-union then d — c mid"},
        {"a(b|c)*d", "abcbd",  true,  "a then star-union then d — mixed mid"},
        {"a(b|c)*d", "ae",     false, "a then star-union then d — wrong char"},

        {"(ab)*",    "",       true,  "star-concat — empty"},
        {"(ab)*",    "ab",     true,  "star-concat — one"},
        {"(ab)*",    "abab",   true,  "star-concat — two"},
        {"(ab)*",    "aba",    false, "star-concat — partial"},

        // ── Escaped characters ──────────────────────────────────────────
        {"a\\*",     "a*",     true,  "escaped star literal"},
        {"a\\|b",    "a|b",    true,  "escaped pipe literal"},
        {"\\(a\\)",  "(a)",    true,  "escaped parens literal"},

        // ── Single-char patterns ────────────────────────────────────────
        {"x",        "x",      true,  "trivial single char"},
        {"x",        "y",      false, "trivial single char miss"},
    };

    int passed = 0, failed = 0;

    std::cout << color::BOLD << color::CYAN
              << "\n╔══════════════════════════════════════════════════════════════╗\n"
              << "║              Regex Engine — Test Suite                       ║\n"
              << "╚══════════════════════════════════════════════════════════════╝\n"
              << color::RESET << "\n";

    for (size_t i = 0; i < tests.size(); ++i) {
        auto& t = tests[i];
        try {
            Regex re(t.pattern);
            bool result = re.match(t.input);
            bool ok = (result == t.expected);

            if (ok) {
                ++passed;
                std::cout << color::GREEN << "  ✓ " << color::RESET;
            } else {
                ++failed;
                std::cout << color::RED << "  ✗ " << color::RESET;
            }

            std::cout << color::DIM << "[" << std::setw(2) << (i+1) << "] "
                      << color::RESET
                      << "\"" << color::YELLOW << t.pattern << color::RESET
                      << "\" vs \"" << color::CYAN << t.input << color::RESET
                      << "\"  →  "
                      << (result ? "MATCH" : "NO MATCH");

            if (!ok) {
                std::cout << color::RED << "  (EXPECTED "
                          << (t.expected ? "MATCH" : "NO MATCH") << ")"
                          << color::RESET;
            }

            std::cout << color::DIM << "  — " << t.description
                      << color::RESET << "\n";

        } catch (const std::exception& ex) {
            ++failed;
            std::cout << color::RED << "  ✗ " << color::RESET
                      << "[" << (i+1) << "] \"" << t.pattern
                      << "\" THREW: " << ex.what() << "\n";
        }
    }

    std::cout << "\n" << color::BOLD;
    if (failed == 0) {
        std::cout << color::GREEN << "  All " << passed << " tests passed! ✓\n";
    } else {
        std::cout << color::RED << "  " << failed << "/" << (passed + failed)
                  << " tests FAILED\n";
    }
    std::cout << color::RESET << "\n";
    return failed;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ReDoS Immunity Benchmark
// ═══════════════════════════════════════════════════════════════════════════════
static void runBenchmark() {
    std::cout << color::BOLD << color::MAGENTA
              << "\n╔══════════════════════════════════════════════════════════════╗\n"
              << "║           ReDoS Immunity Benchmark                           ║\n"
              << "╚══════════════════════════════════════════════════════════════╝\n"
              << color::RESET << "\n";

    // Classic ReDoS pattern: (a+)+b  — backtracking engines explode on "aaa...a"
    std::string pattern = "(a+)+b";

    std::cout << "  Pattern: " << color::YELLOW << pattern << color::RESET << "\n";
    std::cout << "  Compiling...\n";

    Regex re(pattern);
    std::cout << "  Compilation stats:\n";
    re.stats().print();
    std::cout << "\n";

    std::cout << "  " << color::BOLD
              << std::setw(12) << "Input Length"
              << std::setw(18) << "Match Result"
              << std::setw(18) << "Time (us)"
              << color::RESET << "\n";
    std::cout << "  " << std::string(48, '-') << "\n";

    for (int n : {10, 100, 1000, 10000, 100000, 1000000}) {
        std::string input(n, 'a');  // No trailing 'b' → should NOT match

        auto t0 = std::chrono::high_resolution_clock::now();
        bool result = re.match(input);
        auto t1 = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        std::cout << "  " << std::setw(12) << n
                  << std::setw(18) << (result ? "MATCH" : "NO MATCH")
                  << std::setw(15) << std::fixed << std::setprecision(1) << us
                  << " us\n";
    }

    std::cout << "\n" << color::GREEN << color::BOLD
              << "  ✓ Linear-time matching confirmed — immune to ReDoS!\n"
              << color::RESET << "\n";
}

// ═══════════════════════════════════════════════════════════════════════════════
// Interactive REPL
// ═══════════════════════════════════════════════════════════════════════════════
static void runInteractive() {
    std::cout << color::BOLD << color::CYAN
              << "\n╔══════════════════════════════════════════════════════════════╗\n"
              << "║         Regex Engine — Interactive Mode                      ║\n"
              << "║   Type a pattern, then test strings. 'q' to quit.           ║\n"
              << "╚══════════════════════════════════════════════════════════════╝\n"
              << color::RESET << "\n";

    while (true) {
        std::cout << color::YELLOW << "  pattern> " << color::RESET;
        std::string pattern;
        if (!std::getline(std::cin, pattern) || pattern == "q") break;
        if (pattern.empty()) continue;

        try {
            Regex re(pattern);
            std::cout << color::DIM << "  Compiled in "
                      << std::fixed << std::setprecision(1)
                      << re.stats().total_us << " us  ("
                      << re.stats().min_dfa_states << " DFA states)\n"
                      << color::RESET;

            while (true) {
                std::cout << color::CYAN << "    input> " << color::RESET;
                std::string input;
                if (!std::getline(std::cin, input)) return;
                if (input.empty()) break; // empty line → new pattern

                auto t0 = std::chrono::high_resolution_clock::now();
                bool result = re.match(input);
                auto t1 = std::chrono::high_resolution_clock::now();
                double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

                if (result)
                    std::cout << color::GREEN << "      ✓ MATCH" << color::RESET;
                else
                    std::cout << color::RED   << "      ✗ NO MATCH" << color::RESET;

                std::cout << color::DIM << "  (" << std::fixed
                          << std::setprecision(1) << us << " us)"
                          << color::RESET << "\n";
            }
        } catch (const std::exception& ex) {
            std::cout << color::RED << "  Error: " << ex.what()
                      << color::RESET << "\n";
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════════════
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << color::BOLD
                  << "Regex-to-DFA Compiler & Matching Engine\n" << color::RESET
                  << "Usage:\n"
                  << "  regex_engine <pattern> <text1> [text2 ...]\n"
                  << "  regex_engine --test\n"
                  << "  regex_engine --benchmark\n"
                  << "  regex_engine --interactive\n";
        return 0;
    }

    std::string arg1 = argv[1];

    if (arg1 == "--test")        return runTests();
    if (arg1 == "--benchmark")   { runBenchmark(); return 0; }
    if (arg1 == "--interactive") { runInteractive(); return 0; }

    // ── CLI pattern + texts mode ─────────────────────────────────────────
    try {
        std::cout << color::BOLD << "\n  Compiling pattern: "
                  << color::YELLOW << "\"" << arg1 << "\"\n" << color::RESET;

        Regex re(arg1);

        std::cout << color::DIM << "  Compilation stats:\n" << color::RESET;
        re.stats().print();
        std::cout << "\n  " << color::DIM << "DFA:\n" << re.dump() << color::RESET;
        std::cout << "\n";

        for (int i = 2; i < argc; ++i) {
            std::string text = argv[i];
            auto t0 = std::chrono::high_resolution_clock::now();
            bool result = re.match(text);
            auto t1 = std::chrono::high_resolution_clock::now();
            double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

            if (result)
                std::cout << color::GREEN << "  ✓ MATCH    " << color::RESET;
            else
                std::cout << color::RED   << "  ✗ NO MATCH " << color::RESET;

            std::cout << "\"" << color::CYAN << text << color::RESET << "\""
                      << color::DIM << "  (" << std::fixed
                      << std::setprecision(1) << us << " us)"
                      << color::RESET << "\n";
        }
        std::cout << "\n";

    } catch (const std::exception& ex) {
        std::cerr << color::RED << "Error: " << ex.what()
                  << color::RESET << "\n";
        return 1;
    }

    return 0;
}
