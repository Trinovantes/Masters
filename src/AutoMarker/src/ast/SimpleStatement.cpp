#include "SimpleStatement.h"
#include <iostream>
#include <iomanip>

using namespace clang;

void clang::dumpCaptedASTNode(CaptedASTNode* root) {
    root->dfs([](CaptedASTNode* currentNode, int depth) -> void {
        std::cerr << std::setw(4) << depth << std::string(((depth + 1) * 2), ' ') << currentNode->getData()->toString() << std::endl;
    });
}

SimpleStatement* clang::cloneData(const SimpleStatement* original) {
    return original->clone();
}
