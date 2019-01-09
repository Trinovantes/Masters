#pragma once

#include "Capted.h"
#include "ast/SimpleStatement.h"

namespace clang {

class SimpleStatementCostModel : public capted::CostModel<SimpleStatement> {
    VarStatement* tryFindVarStatement(CaptedASTNode* n) const;
    bool isCompatibleExternalVar(CaptedASTNode* n1, CaptedASTNode* n2) const;
    bool isCompatibleInternalVar(CaptedASTNode* n1, CaptedASTNode* n2) const;
    bool isCompatibleInternalVar(VarStatement* v1, VarStatement* v2) const;
    bool isCompatibleFunctionDecl(CaptedASTNode* n1, CaptedASTNode* n2) const;
    bool isCompatibleLoop(CaptedASTNode* n1, CaptedASTNode* n2) const;
    bool isCompatibleCondition(CaptedASTNode* n1, CaptedASTNode* n2) const;

protected:
    static constexpr float DEFAULT_COST = 1.0;
    static constexpr float NO_COST = 0.0;

public:
    SimpleStatementCostModel();
    virtual ~SimpleStatementCostModel();

    virtual float deleteCost(CaptedASTNode* n) const override;
    virtual float insertCost(CaptedASTNode* n) const override;
    virtual float renameCost(CaptedASTNode* n1, CaptedASTNode* n2) const override;
};

}
