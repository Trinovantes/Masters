#include <vector>
#include "SimpleStatementCostModel.h"
#include "Logger.h"

using namespace clang;

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

bool typesAreEqual(const QualType &q1, const QualType &q2) {
    return (q1.getCanonicalType().getAsString() != q2.getCanonicalType().getAsString());
}

//------------------------------------------------------------------------------
// SimpleStatementCostModel
//------------------------------------------------------------------------------

SimpleStatementCostModel::SimpleStatementCostModel() {
    // nop
}

SimpleStatementCostModel::~SimpleStatementCostModel() {
    // nop
}

float SimpleStatementCostModel::deleteCost(CaptedASTNode* n) const {
    return DEFAULT_COST;
}

float SimpleStatementCostModel::insertCost(CaptedASTNode* n) const {
    return DEFAULT_COST;
}

float SimpleStatementCostModel::renameCost(CaptedASTNode* n1, CaptedASTNode* n2) const {
    if (isCompatibleExternalVar(n1, n2)) {
        return NO_COST;
    }

    if (isCompatibleInternalVar(n1, n2)) {
        return NO_COST;
    }

    if (isCompatibleFunctionDecl(n1, n2)) {
        return NO_COST;
    }

    if (isCompatibleLoop(n1, n2)) {
        return NO_COST;
    }

    if (isCompatibleCondition(n1, n2)) {
        return NO_COST;
    }

    if (n1->getData()->toString() == n2->getData()->toString()) {
        return NO_COST;
    }

    return DEFAULT_COST;
}

//------------------------------------------------------------------------------
// Compatibility Checks
//------------------------------------------------------------------------------

VarStatement* SimpleStatementCostModel::tryFindVarStatement(CaptedASTNode* n) const {
    // If the current node is a VarStatement
    VarStatement* varStatement = dyn_cast<VarStatement>(n->getData());
    if (varStatement) {
        return varStatement;
    }

    // If the current node is a DeclRefStatement, try to find the VarStatement it references if it exists
    DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(n->getData());
    if (declRefStatement) {
        return declRefStatement->getVarStatement();
    }

    return nullptr;
}

bool SimpleStatementCostModel::isCompatibleExternalVar(CaptedASTNode* n1, CaptedASTNode* n2) const {
    DeclRefStatement* d1 = dyn_cast<DeclRefStatement>(n1->getData());
    DeclRefStatement* d2 = dyn_cast<DeclRefStatement>(n2->getData());

    if (!d1 || !d2) {
        return false;
    }

    VarDecl* v1 = dyn_cast<VarDecl>(d1->getDecl());
    VarDecl* v2 = dyn_cast<VarDecl>(d2->getDecl());

    if (!v1 || !v2) {
        return false;
    }

    if (v1->isLocalVarDeclOrParm() || v2->isLocalVarDeclOrParm()) {
        // Ignore local variables or parameters
        return false;
    }

    if (typesAreEqual(v1->getType(), v2->getType())) {
        return false;
    }

    if (v1->getNameAsString() != v2->getNameAsString()) {
        return false;
    }

    return true;
}

bool SimpleStatementCostModel::isCompatibleInternalVar(CaptedASTNode* n1, CaptedASTNode* n2) const {
    VarStatement* v1 = tryFindVarStatement(n1);
    VarStatement* v2 = tryFindVarStatement(n2);
    return isCompatibleInternalVar(v1, v2);
}

bool SimpleStatementCostModel::isCompatibleInternalVar(VarStatement* v1, VarStatement* v2) const {
    if (!v1 || !v2) {
        return false;
    }

    // We only care if these two variables share common read/write uses
    // We don't need to care about their use's parent functions

    const std::vector<std::string> &v1w = v1->getWriters();
    const std::vector<std::string> &v2w = v2->getWriters();
    const std::vector<std::string> &v1r = v1->getReaders();
    const std::vector<std::string> &v2r = v2->getReaders();

    if (v1w.size() == 0 && v2w.size() == 0 && v1r.size() == 0 && v2r.size() == 0) {
        return true;
    }

    for (std::string w1 : v1w) {
        for (std::string w2 : v2w) {
            if (w1 == w2) {
                return true;
            }
        }
    }

    for (std::string r1 : v1r) {
        for (std::string r2 : v2r) {
            if (r1 == r2) {
                return true;
            }
        }
    }

    return false;
}

bool SimpleStatementCostModel::isCompatibleFunctionDecl(CaptedASTNode* n1, CaptedASTNode* n2) const {
    DeclRefStatement* d1 = dyn_cast<DeclRefStatement>(n1->getData());
    DeclRefStatement* d2 = dyn_cast<DeclRefStatement>(n2->getData());

    if (!d1 || !d2) {
        return false;
    }

    // Checks if 2 function references (when functions are referenced by pointers) are compatible
    // e.g. passing a function pointer as a param
    FunctionDecl* f1 = dyn_cast<FunctionDecl>(d1->getDecl());
    FunctionDecl* f2 = dyn_cast<FunctionDecl>(d2->getDecl());

    if (!f1 || !f2) {
        return false;
    }

    if (typesAreEqual(f1->getReturnType(), f2->getReturnType())) {
        return false;
    }

    if (f1->getNameAsString() != f2->getNameAsString()) {
        return false;
    }

    if (f1->getNumParams() != f2->getNumParams()) {
        return false;
    }

    int n = f1->getNumParams();
    for (int i = 0; i < n; i++) {
        if (typesAreEqual(f1->getParamDecl(i)->getType(), f2->getParamDecl(i)->getType())) {
            return false;
        }
    }

    return true;
}

bool SimpleStatementCostModel::isCompatibleLoop(CaptedASTNode* n1, CaptedASTNode* n2) const {
    return isa<LoopStatement>(n1->getData()) && isa<LoopStatement>(n2->getData());
}

bool SimpleStatementCostModel::isCompatibleCondition(CaptedASTNode* n1, CaptedASTNode* n2) const {
    ConditionStatement* cond1 = dyn_cast<ConditionStatement>(n1->getData());
    ConditionStatement* cond2 = dyn_cast<ConditionStatement>(n2->getData());

    if (!cond1 || !cond2) {
        return false;
    }

    std::set<VarStatement*> foundVarStatements;
    bool hasCommonVar = false;

    n1->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
        DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(currentNode->getData());
        if (!declRefStatement) {
            return;
        }

        foundVarStatements.insert(declRefStatement->getVarStatement());
    });
    n2->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
        DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(currentNode->getData());
        if (!declRefStatement) {
            return;
        }

        for (VarStatement* v : foundVarStatements) {
            if (isCompatibleInternalVar(declRefStatement->getVarStatement(), v)) {
                hasCommonVar = true;
            }
        }
    });

    return hasCommonVar;
}
