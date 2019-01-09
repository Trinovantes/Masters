#include <vector>
#include "KeyFnCostModel.h"
#include "markers/Marker.h"

using namespace clang;

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const std::vector<float>* KeyFnCostModel::getKeyFnCost(CaptedASTNode* n) const {
    CallStatement* callStatement = dyn_cast<CallStatement>(n->getData());
    if (!callStatement) {
        return nullptr;
    }

    auto it = keyFns.find(callStatement->getTargetFn());
    if (it == keyFns.end()) {
        return nullptr;
    }

    return &(it->second);
}

//------------------------------------------------------------------------------
// KeyFnCostModel
//------------------------------------------------------------------------------

KeyFnCostModel::KeyFnCostModel(const std::map<std::string, std::vector<float>> &keyFns)
    : SimpleStatementCostModel()
    , keyFns(keyFns) {
    // nop
}

KeyFnCostModel::~KeyFnCostModel() {
    // nop
}

float KeyFnCostModel::deleteCost(CaptedASTNode* n) const {
    const std::vector<float>* cost = getKeyFnCost(n);
    if (cost) {
        return cost->at(Marker::KeyFnCost::DELETION);
    }

    return SimpleStatementCostModel::deleteCost(n);
}

float KeyFnCostModel::insertCost(CaptedASTNode* n) const {
    const std::vector<float>* cost = getKeyFnCost(n);
    if (cost) {
        return cost->at(Marker::KeyFnCost::INSERTION);
    }

    return SimpleStatementCostModel::insertCost(n);
}

float KeyFnCostModel::renameCost(CaptedASTNode* n1, CaptedASTNode* n2) const {
    const std::vector<float>* srcCost = getKeyFnCost(n1);
    const std::vector<float>* destCost = getKeyFnCost(n2);

    if ((srcCost != nullptr) xor (destCost != nullptr)) {
        float totalCost = DEFAULT_COST;

        if (srcCost) {
            totalCost += srcCost->at(Marker::KeyFnCost::DELETION);
        }

        if (destCost) {
            totalCost += destCost->at(Marker::KeyFnCost::INSERTION);
        }

        return totalCost;
    }

    return SimpleStatementCostModel::renameCost(n1, n2);
}
