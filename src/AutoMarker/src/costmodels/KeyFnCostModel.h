#pragma once

#include "SimpleStatementCostModel.h"
#include "Capted.h"

namespace clang {

class KeyFnCostModel : public SimpleStatementCostModel {
    const std::map<std::string, std::vector<float>> &keyFns;

    const std::vector<float>* getKeyFnCost(CaptedASTNode* n) const;

public:
    KeyFnCostModel(const std::map<std::string, std::vector<float>> &keyFns);
    virtual ~KeyFnCostModel();

    virtual float deleteCost(CaptedASTNode* n) const override;
    virtual float insertCost(CaptedASTNode* n) const override;
    virtual float renameCost(CaptedASTNode* n1, CaptedASTNode* n2) const override;
};

}
