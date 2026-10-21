#pragma once
// =============================================================================
// Ast.h — Abstract Syntax Tree for Regular Expressions
// =============================================================================
// Defines the AST node types used to represent a parsed regex expression.
// Leaf nodes are Literal characters; internal nodes are operators (Concat,
// Union, Star, Plus, Optional).
// =============================================================================

#include <memory>
#include <string>
#include <sstream>

namespace regex_engine {

// ---- AST Node Types ---------------------------------------------------------
enum class AstNodeType {
    Literal,    // A single character
    Concat,     // Concatenation (A · B)
    Union,      // Alternation  (A | B)
    Star,       // Kleene star  (A*)
    Plus,       // One-or-more  (A+)
    Optional    // Zero-or-one  (A?)
};

// ---- AST Node ---------------------------------------------------------------
struct AstNode {
    AstNodeType type;
    char value;  // Only meaningful for Literal nodes

    std::unique_ptr<AstNode> left;   // First operand
    std::unique_ptr<AstNode> right;  // Second operand (Concat, Union only)

    // Construct a leaf (literal) node
    static std::unique_ptr<AstNode> makeLiteral(char c) {
        auto n = std::make_unique<AstNode>();
        n->type  = AstNodeType::Literal;
        n->value = c;
        return n;
    }

    // Construct a unary operator node (Star, Plus, Optional)
    static std::unique_ptr<AstNode> makeUnary(AstNodeType t,
                                               std::unique_ptr<AstNode> operand) {
        auto n = std::make_unique<AstNode>();
        n->type = t;
        n->value = 0;
        n->left = std::move(operand);
        return n;
    }

    // Construct a binary operator node (Concat, Union)
    static std::unique_ptr<AstNode> makeBinary(AstNodeType t,
                                                std::unique_ptr<AstNode> lhs,
                                                std::unique_ptr<AstNode> rhs) {
        auto n = std::make_unique<AstNode>();
        n->type  = t;
        n->value = 0;
        n->left  = std::move(lhs);
        n->right = std::move(rhs);
        return n;
    }

    // Pretty-print the AST (for debugging)
    std::string dump(int depth = 0) const {
        std::ostringstream oss;
        std::string indent(depth * 2, ' ');
        switch (type) {
            case AstNodeType::Literal:
                oss << indent << "Literal('" << value << "')\n";
                break;
            case AstNodeType::Concat:
                oss << indent << "Concat\n";
                if (left)  oss << left->dump(depth + 1);
                if (right) oss << right->dump(depth + 1);
                break;
            case AstNodeType::Union:
                oss << indent << "Union\n";
                if (left)  oss << left->dump(depth + 1);
                if (right) oss << right->dump(depth + 1);
                break;
            case AstNodeType::Star:
                oss << indent << "Star\n";
                if (left) oss << left->dump(depth + 1);
                break;
            case AstNodeType::Plus:
                oss << indent << "Plus\n";
                if (left) oss << left->dump(depth + 1);
                break;
            case AstNodeType::Optional:
                oss << indent << "Optional\n";
                if (left) oss << left->dump(depth + 1);
                break;
        }
        return oss.str();
    }
};

} // namespace regex_engine
