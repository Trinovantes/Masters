#pragma once

#include <set>
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"

namespace clang {

class IsModifiedChecker : public RecursiveASTVisitor<IsModifiedChecker> {
    Stmt* body;
    std::set<VarDecl*> modifiedVars;

    void registerVarIfAny(Expr* expr);

public:
    explicit IsModifiedChecker(Stmt* body);
    ~IsModifiedChecker();

    virtual bool VisitUnaryOperator(UnaryOperator* unaryOp);
    virtual bool VisitBinaryOperator(BinaryOperator* binaryOp);

    bool isModified(VarDecl* var);
};

} // namespace clang
