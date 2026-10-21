# 🔧 Regex-to-DFA Compiler & Matching Engine

A complete, high-performance regex compiler and matching engine built from scratch in **modern C++17**, based on automata theory. Zero external dependencies — no `std::regex`, PCRE, or Boost.

## ✨ Features

- **O(n) linear-time matching** — processes each character exactly once
- **O(1) space per character** — no stack growth during matching
- **ReDoS immune** — DFA-based execution eliminates catastrophic backtracking
- **4-phase automata pipeline** built from the ground up:

```
Regex String → Parser → AST → Thompson's NFA → Subset Construction → DFA → Hopcroft Minimization → Minimized DFA → O(n) Matcher
```

## 📐 Architecture

| Phase | Algorithm | File |
|-------|-----------|------|
| **1 — Parsing** | Concat insertion + Shunting-Yard + AST construction | `Parser.h` |
| **2 — NFA** | Thompson's Construction (recursive post-order AST traversal) | `Nfa.h` |
| **3 — DFA** | Subset / Powerset Construction (ε-closure + Move) | `Dfa.h` |
| **4 — Minimize** | Hopcroft's DFA Minimization (partition refinement) | `Dfa.h` |
| **Matcher** | Zero-backtracking DFA walker | `Dfa.h` |

## 🗂️ File Structure

```
├── Ast.h          # AST node types (Literal, Concat, Union, Star, Plus, Optional)
├── Parser.h       # Phase 1: Lexical analysis & parsing
├── Nfa.h          # Phase 2: Thompson's NFA construction
├── Dfa.h          # Phase 3+4: Subset construction + Hopcroft minimization
├── Compiler.h     # Top-level Regex class (chains all phases)
├── main.cpp       # CLI driver (match, test, benchmark, interactive)
├── CMakeLists.txt # CMake build configuration
└── README.md
```

## 🚀 Build

**MSVC (Windows):**
```bash
cl /std:c++17 /EHsc /O2 /Fe:regex_engine.exe main.cpp
```

**GCC / Clang (Linux/macOS):**
```bash
g++ -std=c++17 -O2 -o regex_engine main.cpp
```

**CMake:**
```bash
cmake -B build && cmake --build build --config Release
```

## 📖 Usage

### Match a pattern against strings
```bash
./regex_engine "(a|b)*c" c ac abc ab
```

### Run test suite (60 tests)
```bash
./regex_engine --test
```

### ReDoS immunity benchmark
```bash
./regex_engine --benchmark
```
Tests the classic attack pattern `(a+)+b` against inputs up to 1,000,000 characters — completes in linear time.

### Interactive REPL
```bash
./regex_engine --interactive
```

## 🧪 Supported Regex Syntax

| Syntax | Meaning |
|--------|---------|
| `a` | Literal character |
| `ab` | Concatenation |
| `a\|b` | Union (alternation) |
| `a*` | Kleene star (zero or more) |
| `a+` | One or more |
| `a?` | Zero or one (optional) |
| `(...)` | Grouping |
| `\*` `\|` `\(` `\)` | Escaped metacharacters |

## 🛡️ ReDoS Immunity

Traditional backtracking engines (Python `re`, Java `Pattern`, JavaScript `RegExp`) suffer from **exponential blowup** on patterns like `(a+)+b` with adversarial inputs. This engine compiles to a DFA, guaranteeing:

- **Constant time per character** regardless of pattern complexity
- **No backtracking, recursion, or stack usage** during matching
- **Provably safe** against all ReDoS attack vectors

## 📄 License

MIT
