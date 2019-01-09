#include "IsModifiedChecker.h"

using namespace clang;

//------------------------------------------------------------------------------
// IsModifiedChecker
//
// Note: Does not find all cases that modify var; this just finds vars that are
//       modified by assignments, increment/decrement, dereferenced by address operator
//------------------------------------------------------------------------------

IsModifiedChecker::IsModifiedChecker(Stmt* body) : body(body) {
    this->TraverseStmt(body);
}

IsModifiedChecker::~IsModifiedChecker() {
    // nop
}

void IsModifiedChecker::registerVarIfAny(Expr* expr) {
    while (isa<ParenExpr>(expr)) {
        expr = cast<ParenExpr>(expr)->getSubExpr();
    }

    if (DeclRefExpr* declRef = dyn_cast<DeclRefExpr>(expr)) {
        if (VarDecl* var = dyn_cast<VarDecl>(declRef->getDecl()->getCanonicalDecl())) {
            modifiedVars.insert(var);
        }
    }
}

bool IsModifiedChecker::VisitUnaryOperator(UnaryOperator* unaryOp) {
    if (unaryOp->isIncrementDecrementOp() || unaryOp->getOpcode() == UO_AddrOf) {
        registerVarIfAny(unaryOp->getSubExpr());
    }

    return true;
}

bool IsModifiedChecker::VisitBinaryOperator(BinaryOperator* binaryOp) {
    if (binaryOp->isAssignmentOp() || binaryOp->isCompoundAssignmentOp()) {
        registerVarIfAny(binaryOp->getLHS());
    }

    return true;
}

bool IsModifiedChecker::isModified(VarDecl* var) {
    return modifiedVars.count(var) > 0;
}
