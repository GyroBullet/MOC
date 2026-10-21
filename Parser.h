#pragma once
// =============================================================================
// Parser.h — Phase 1: Lexical Analysis, Preprocessing & Parsing
// =============================================================================
// 1. Inserts explicit concatenation operators ('·' internally coded as
//    CONCAT_OP) between adjacent tokens where concatenation is implied.
// 2. Converts the infix expression to postfix using the Shunting-Yard
//    algorithm, respecting precedence: * / + / ? > · > |
// 3. Builds an AST from the postfix token stream.
// =============================================================================

#include "Ast.h"
#include <string>
#include <vector>
#include <stack>
#include <stdexcept>
#include <set>

namespace regex_engine {

// Internal sentinel character for explicit concatenation operator
constexpr char CONCAT_OP = '\x01';

// ---- Token ------------------------------------------------------------------
struct Token {
    enum Kind { Char, Op };
    Kind kind;
    char value;  // literal char or operator symbol
};

class Parser {
public:
    // Parse a regex string and return its AST root.
    static std::unique_ptr<AstNode> parse(const std::string& pattern) {
        auto tokens = tokenize(insertConcat(pattern));
        auto postfix = shuntingYard(tokens);
        return buildAst(postfix);
    }

private:
    // ---- Step 1: Insert explicit concatenation operators --------------------
    // Must be escape-aware: a backslash followed by any char is a single
    // literal unit and must not be split or misinterpreted as an operator.
    static std::string insertConcat(const std::string& regex) {
        std::string result;

        // Helper: returns true if the character (or escape-pair ending) at
        //         position i can END a sub-expression (i.e. be followed by ·).
        auto canEnd = [&](size_t i) -> bool {
            char c = regex[i];
            if (c == '\\') return false; // backslash alone is escape-prefix
            return isLiteral(c) || c == ')' || c == '*' || c == '+' || c == '?';
        };

        // Helper: returns true if the character (or escape-pair starting) at
        //         position i can START a sub-expression (preceded by ·).
        auto canStart = [&](size_t i) -> bool {
            char c = regex[i];
            if (c == '\\') return true;  // escaped pair starts a literal
            return isLiteral(c) || c == '(';
        };

        for (size_t i = 0; i < regex.size(); ++i) {
            char c = regex[i];

            if (c == '\\' && i + 1 < regex.size()) {
                // Escape pair: emit both characters
                // Check if we need a concat BEFORE this pair
                // (handled by the previous iteration's lookahead)
                result += c;
                ++i;
                result += regex[i];
                // Now check if a concat is needed AFTER this pair
                if (i + 1 < regex.size() && canStart(i + 1)) {
                    result += CONCAT_OP;
                }
            } else {
                result += c;
                if (i + 1 < regex.size() && canEnd(i) && canStart(i + 1)) {
                    result += CONCAT_OP;
                }
            }
        }
        return result;
    }

    // ---- Step 2: Tokenize ---------------------------------------------------
    static std::vector<Token> tokenize(const std::string& s) {
        std::vector<Token> tokens;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '\\' && i + 1 < s.size()) {
                // Escaped character → treat next char as literal
                ++i;
                tokens.push_back({Token::Char, s[i]});
            } else if (c == '(' || c == ')' || c == '|' || c == '*'
                       || c == '+' || c == '?' || c == CONCAT_OP) {
                tokens.push_back({Token::Op, c});
            } else {
                tokens.push_back({Token::Char, c});
            }
        }
        return tokens;
    }

    // ---- Step 3: Shunting-Yard (infix → postfix) ----------------------------
    static std::vector<Token> shuntingYard(const std::vector<Token>& tokens) {
        std::vector<Token> output;
        std::stack<Token> ops;

        for (auto& tok : tokens) {
            if (tok.kind == Token::Char) {
                output.push_back(tok);
            } else if (tok.value == '(') {
                ops.push(tok);
            } else if (tok.value == ')') {
                while (!ops.empty() && ops.top().value != '(') {
                    output.push_back(ops.top());
                    ops.pop();
                }
                if (ops.empty())
                    throw std::runtime_error("Parser: mismatched parentheses");
                ops.pop(); // remove '('
            } else {
                // Operator
                int prec = precedence(tok.value);
                while (!ops.empty() && ops.top().value != '('
                       && precedence(ops.top().value) >= prec) {
                    output.push_back(ops.top());
                    ops.pop();
                }
                ops.push(tok);
            }
        }
        while (!ops.empty()) {
            if (ops.top().value == '(')
                throw std::runtime_error("Parser: mismatched parentheses");
            output.push_back(ops.top());
            ops.pop();
        }
        return output;
    }

    // ---- Step 4: Postfix → AST ----------------------------------------------
    static std::unique_ptr<AstNode> buildAst(const std::vector<Token>& postfix) {
        std::stack<std::unique_ptr<AstNode>> st;

        for (auto& tok : postfix) {
            if (tok.kind == Token::Char) {
                st.push(AstNode::makeLiteral(tok.value));
            } else if (tok.value == '*') {
                if (st.empty())
                    throw std::runtime_error("Parser: invalid regex (star)");
                auto operand = std::move(st.top()); st.pop();
                st.push(AstNode::makeUnary(AstNodeType::Star, std::move(operand)));
            } else if (tok.value == '+') {
                if (st.empty())
                    throw std::runtime_error("Parser: invalid regex (plus)");
                auto operand = std::move(st.top()); st.pop();
                st.push(AstNode::makeUnary(AstNodeType::Plus, std::move(operand)));
            } else if (tok.value == '?') {
                if (st.empty())
                    throw std::runtime_error("Parser: invalid regex (optional)");
                auto operand = std::move(st.top()); st.pop();
                st.push(AstNode::makeUnary(AstNodeType::Optional, std::move(operand)));
            } else if (tok.value == CONCAT_OP) {
                if (st.size() < 2)
                    throw std::runtime_error("Parser: invalid regex (concat)");
                auto rhs = std::move(st.top()); st.pop();
                auto lhs = std::move(st.top()); st.pop();
                st.push(AstNode::makeBinary(AstNodeType::Concat,
                                            std::move(lhs), std::move(rhs)));
            } else if (tok.value == '|') {
                if (st.size() < 2)
                    throw std::runtime_error("Parser: invalid regex (union)");
                auto rhs = std::move(st.top()); st.pop();
                auto lhs = std::move(st.top()); st.pop();
                st.push(AstNode::makeBinary(AstNodeType::Union,
                                            std::move(lhs), std::move(rhs)));
            }
        }

        if (st.size() != 1)
            throw std::runtime_error("Parser: invalid regex expression");
        return std::move(st.top());
    }

    // ---- Helpers ------------------------------------------------------------
    static int precedence(char op) {
        switch (op) {
            case '*': case '+': case '?': return 3;
            case CONCAT_OP:               return 2;
            case '|':                      return 1;
            default:                       return 0;
        }
    }

    static bool isLiteral(char c) {
        static const std::set<char> specials = {'(', ')', '|', '*', '+', '?', CONCAT_OP};
        return specials.find(c) == specials.end();
    }
};

} // namespace regex_engine
