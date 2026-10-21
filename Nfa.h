#pragma once
// =============================================================================
// Nfa.h — Phase 2: Thompson's Construction (AST → NFA)
// =============================================================================
// Converts the AST produced by the parser into a Non-deterministic Finite
// Automaton using Thompson's construction.  Each NFA fragment has a single
// start state and a single accept state.  States are identified by integer IDs. 
// Epsilon transitions use the sentinel '\0'.
// =============================================================================

#include "Ast.h"
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <stdexcept>
#include <queue>

namespace regex_engine {

// Sentinel for epsilon transitions
constexpr char EPSILON = '\0';

// ---- NFA State --------------------------------------------------------------
struct NfaState {
    int id;
    bool is_accept = false;
    // Transitions: character → set of target state IDs.
    // EPSILON ('\0') key is used for epsilon transitions.
    std::map<char, std::set<int>> transitions;

    explicit NfaState(int id) : id(id) {}
};

// ---- NFA Fragment -----------------------------------------------------------
struct NfaFragment {
    int start;   // start-state ID
    int accept;  // accept-state ID
};

// ---- NFA (complete automaton) -----------------------------------------------
class Nfa {
public:
    std::vector<std::unique_ptr<NfaState>> states;
    int start_state = -1;
    int accept_state = -1;

    // Create a new state and return its ID
    int newState() {
        int id = static_cast<int>(states.size());
        states.push_back(std::make_unique<NfaState>(id));
        return id;
    }

    // Add a transition
    void addTransition(int from, char on, int to) {
        states[from]->transitions[on].insert(to);
    }

    // ---- Thompson's construction from AST -----------------------------------
    static Nfa fromAst(const AstNode* root) {
        Nfa nfa;
        NfaFragment frag = nfa.build(root);
        nfa.start_state  = frag.start;
        nfa.accept_state = frag.accept;
        nfa.states[frag.accept]->is_accept = true;
        return nfa;
    }

    // ---- Compute ε-closure of a set of states --------------------------------
    std::set<int> epsilonClosure(const std::set<int>& stateSet) const {
        std::set<int> closure = stateSet;
        std::queue<int> worklist;
        for (int s : stateSet) worklist.push(s);

        while (!worklist.empty()) {
            int curr = worklist.front(); worklist.pop();
            auto it = states[curr]->transitions.find(EPSILON);
            if (it != states[curr]->transitions.end()) {
                for (int next : it->second) {
                    if (closure.insert(next).second) {
                        worklist.push(next);
                    }
                }
            }
        }
        return closure;
    }

    // ---- Compute move(stateSet, symbol) ---------------------------------------
    std::set<int> move(const std::set<int>& stateSet, char symbol) const {
        std::set<int> result;
        for (int s : stateSet) {
            auto it = states[s]->transitions.find(symbol);
            if (it != states[s]->transitions.end()) {
                result.insert(it->second.begin(), it->second.end());
            }
        }
        return result;
    }

    // Collect the alphabet (non-epsilon symbols) used in the NFA
    std::set<char> alphabet() const {
        std::set<char> alpha;
        for (auto& st : states) {
            for (auto& [sym, _] : st->transitions) {
                if (sym != EPSILON) alpha.insert(sym);
            }
        }
        return alpha;
    }

    // Pretty-print
    std::string dump() const {
        std::string out;
        out += "NFA: start=" + std::to_string(start_state)
             + " accept=" + std::to_string(accept_state) + "\n";
        for (auto& st : states) {
            for (auto& [sym, targets] : st->transitions) {
                for (int t : targets) {
                    out += "  S" + std::to_string(st->id) + " --";
                    if (sym == EPSILON) out += "[eps]";
                    else                out += std::string("['") + sym + "']";
                    out += "--> S" + std::to_string(t) + "\n";
                }
            }
        }
        return out;
    }

private:
    // Recursive Thompson's construction
    NfaFragment build(const AstNode* node) {
        if (!node) throw std::runtime_error("NFA: null AST node");

        switch (node->type) {
        case AstNodeType::Literal:
            return buildLiteral(node->value);
        case AstNodeType::Concat:
            return buildConcat(node->left.get(), node->right.get());
        case AstNodeType::Union:
            return buildUnion(node->left.get(), node->right.get());
        case AstNodeType::Star:
            return buildStar(node->left.get());
        case AstNodeType::Plus:
            return buildPlus(node->left.get());
        case AstNodeType::Optional:
            return buildOptional(node->left.get());
        }
        throw std::runtime_error("NFA: unknown AST node type");
    }

    // Literal: start --c--> accept
    NfaFragment buildLiteral(char c) {
        int s = newState();
        int a = newState();
        addTransition(s, c, a);
        return {s, a};
    }

    // Concat: A.start → ... → A.accept ε→ B.start → ... → B.accept
    NfaFragment buildConcat(const AstNode* left, const AstNode* right) {
        NfaFragment a = build(left);
        NfaFragment b = build(right);
        addTransition(a.accept, EPSILON, b.start);
        return {a.start, b.accept};
    }

    // Union:   new_start  ε→ A.start
    //          new_start  ε→ B.start
    //          A.accept   ε→ new_accept
    //          B.accept   ε→ new_accept
    NfaFragment buildUnion(const AstNode* left, const AstNode* right) {
        NfaFragment a = build(left);
        NfaFragment b = build(right);
        int s = newState();
        int acc = newState();
        addTransition(s,        EPSILON, a.start);
        addTransition(s,        EPSILON, b.start);
        addTransition(a.accept, EPSILON, acc);
        addTransition(b.accept, EPSILON, acc);
        return {s, acc};
    }

    // Kleene Star:  new_start ε→ A.start
    //               new_start ε→ new_accept
    //               A.accept  ε→ A.start
    //               A.accept  ε→ new_accept
    NfaFragment buildStar(const AstNode* operand) {
        NfaFragment a = build(operand);
        int s   = newState();
        int acc = newState();
        addTransition(s,        EPSILON, a.start);
        addTransition(s,        EPSILON, acc);
        addTransition(a.accept, EPSILON, a.start);
        addTransition(a.accept, EPSILON, acc);
        return {s, acc};
    }

    // Plus (one-or-more):  same as Star but no bypass from start → accept
    NfaFragment buildPlus(const AstNode* operand) {
        NfaFragment a = build(operand);
        int s   = newState();
        int acc = newState();
        addTransition(s,        EPSILON, a.start);
        // NO: addTransition(s, EPSILON, acc);  ← must match at least once
        addTransition(a.accept, EPSILON, a.start);
        addTransition(a.accept, EPSILON, acc);
        return {s, acc};
    }

    // Optional (zero-or-one):  start ε→ A.start, start ε→ accept,
    //                          A.accept ε→ accept  (no loop-back)
    NfaFragment buildOptional(const AstNode* operand) {
        NfaFragment a = build(operand);
        int s   = newState();
        int acc = newState();
        addTransition(s,        EPSILON, a.start);
        addTransition(s,        EPSILON, acc);
        addTransition(a.accept, EPSILON, acc);
        return {s, acc};
    }
};

} // namespace regex_engine
