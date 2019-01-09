#pragma once

#include <set>
#include <queue>
#include <string>
#include "SimpleStatement.h"

namespace clang {

class ASTPruner
{
    const std::set<std::string> &interestingFunctions;
    const std::set<std::string> ignoredFunctions = {
        "printf",
        "sprintf",
        "malloc",
        "calloc",
        "free",
        "exit",
    };

    std::set<VarDecl*> interestingVars;

    void findInterestingVars(CaptedASTNode* root);
    void findAliasedInterestingVars(CaptedASTNode* root);

    void registerVarsAsInteresting(CaptedASTNode* root);
    bool hasRefToInterestingVars(CaptedASTNode* root);

    bool isSafeToDelete(CaptedASTNode* root, bool keepChildren);
    void pruneNode(CaptedASTNode* root, bool keepChildren, std::function<bool(CaptedASTNode* currentNode)> shouldRemove);
    void pruneUselessStatements(CaptedASTNode* root);
    void pruneIgnoredFunctions(CaptedASTNode* root);
    void pruneUnusedAssignments(CaptedASTNode* root);
    void pruneUnusedVarDecl(CaptedASTNode* root);
    void pruneEmptyConstructs(CaptedASTNode* root);

public:
    ASTPruner(const std::set<std::string> &interestingFunctions);
    ~ASTPruner();

    void prune(CaptedASTNode* root);
    void dump();
};

}
