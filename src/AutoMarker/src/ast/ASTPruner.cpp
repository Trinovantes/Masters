#include "ASTPruner.h"
#include "llvm/Support/Casting.h"

using namespace clang;

//------------------------------------------------------------------------------
// ASTPruner
//------------------------------------------------------------------------------

ASTPruner::ASTPruner(const std::set<std::string> &interestingFunctions)
    : interestingFunctions(interestingFunctions) {
    // nop
}

ASTPruner::~ASTPruner() {
    // nop
}

bool ASTPruner::isSafeToDelete(CaptedASTNode* root, bool keepChildren) {
    // Cannot be VarStatement and be in interestingVars
    if (VarStatement* varStatement = dyn_cast<VarStatement>(root->getData())) {
        return (interestingVars.count(varStatement->getDecl()) == 0);
    }

    // Cannot reference an interesting var
    if (DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(root->getData())) {
        if (VarDecl* varDecl = dyn_cast<VarDecl>(declRefStatement->getDecl())) {
            return (interestingVars.count(varDecl) == 0);
        }
    }

    // Cannot delete children if they contain interestingVars
    // unless parent is a CallStatement to ignoredFunctions
    if (!keepChildren) {
        if (CallStatement* callStatement = dyn_cast<CallStatement>(root->getData())) {
            if (ignoredFunctions.count(callStatement->getTargetFn()) > 0) {
                return true;
            }
        }

        return !hasRefToInterestingVars(root);
    }

    // Everything else is safe to delete
    return true;
}

void ASTPruner::pruneNode(CaptedASTNode* root, bool keepChildren, std::function<bool(CaptedASTNode* currentNode)> shouldRemove) {
    assert(root);

    // Make a copy since pruneNode() will modify the container
    // This is also why we can't use CaptedASTNode::dfs()
    std::list<CaptedASTNode*> childrenCopy = root->getChildren();

    CaptedASTNode* grandparent = root->getParent();
    if (grandparent && shouldRemove(root)) {
        // If we want to keep children, then they'll be adopted by my parent in my place
        if (keepChildren) {
            auto myIter = root->getMyIter();

            // Reverse iter because insertChild pushes last child to the top last (we want the original first child to top)
            for (auto it = childrenCopy.rbegin(); it != childrenCopy.rend(); ++it) {
                CaptedASTNode* child = *it;
                child->detachFromParent();
                myIter = grandparent->insertChild(myIter, child);
            }
        }

        // This is extermely slow check so we comment it out for production
        // assert(isSafeToDelete(root, keepChildren));

        // Remove myself from tree
        root->detachFromParent();
        delete root;
    }

    // If we kept the children, then we need to recursively go prune the children
    if (keepChildren) {
        for (CaptedASTNode* child : childrenCopy) {
            pruneNode(child, keepChildren, shouldRemove);
        }
    }
}

//------------------------------------------------------------------------------
// Interesting Variables
//------------------------------------------------------------------------------

void ASTPruner::findInterestingVars(CaptedASTNode* root) {
    root->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
        VarStatement* varStatement = dyn_cast<VarStatement>(currentNode->getData());
        if (!varStatement) {
            return;
        }

        for (std::string reader : varStatement->getReaders()) {
            if (interestingFunctions.count(reader) > 0) {
                interestingVars.insert(varStatement->getDecl());
            }
        }

        for (std::string writer : varStatement->getWriters()) {
            if (interestingFunctions.count(writer) > 0) {
                interestingVars.insert(varStatement->getDecl());
            }
        }
    });
}

void ASTPruner::findAliasedInterestingVars(CaptedASTNode* root) {
    // Loop until we're in a steady state
    size_t prevSize;
    do {
        prevSize = interestingVars.size();

        root->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
            if (!isa<AssnStatement>(currentNode->getData())) {
                return;
            }

            CaptedASTNode* lhs = currentNode->getChildren().front();
            CaptedASTNode* rhs = currentNode->getChildren().back();
            if (hasRefToInterestingVars(rhs)) {
                registerVarsAsInteresting(lhs);
            }
        });
    } while (interestingVars.size() > prevSize);
}

void ASTPruner::registerVarsAsInteresting(CaptedASTNode* root) {
    root->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
        DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(currentNode->getData());
        if (!declRefStatement) {
            return;
        }

        VarDecl* varDecl = dyn_cast<VarDecl>(declRefStatement->getDecl());
        if (!varDecl) {
            return;
        }

        interestingVars.insert(varDecl);
    });
}

bool ASTPruner::hasRefToInterestingVars(CaptedASTNode* root) {
    int numInterestingVars = 0;

    root->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
        DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(currentNode->getData());
        if (!declRefStatement) {
            return;
        }

        VarDecl* varDecl = dyn_cast<VarDecl>(declRefStatement->getDecl());
        if (!varDecl) {
            return;
        }

        numInterestingVars += interestingVars.count(varDecl);
    });

    return (numInterestingVars > 0);
}

//------------------------------------------------------------------------------
// Prunning
//------------------------------------------------------------------------------

void ASTPruner::pruneUselessStatements(CaptedASTNode* root) {
    const bool KEEP_CHILDREN = true;
    pruneNode(root, KEEP_CHILDREN, [&](CaptedASTNode* currentNode) -> bool {
        return isa<UselessStatement>(currentNode->getData());
    });
}

void ASTPruner::pruneIgnoredFunctions(CaptedASTNode* root) {
    const bool KEEP_CHILDREN = false;
    pruneNode(root, KEEP_CHILDREN, [&](CaptedASTNode* currentNode) -> bool {
        if (CallStatement* callStatement = dyn_cast<CallStatement>(currentNode->getData())) {
            if (ignoredFunctions.count(callStatement->getTargetFn())) {
                return true;
            }
        }

        return false;
    });
}

void ASTPruner::pruneUnusedAssignments(CaptedASTNode* root) {
    const bool KEEP_CHILDREN = false;
    pruneNode(root, KEEP_CHILDREN, [&](CaptedASTNode* currentNode) -> bool {
        if (!isa<AssnStatement>(currentNode->getData())) {
            return false;
        }

        // Remove if this doesn't have interesting vars
        return !hasRefToInterestingVars(currentNode);
    });
}

void ASTPruner::pruneUnusedVarDecl(CaptedASTNode* root) {
    const bool KEEP_CHILDREN = false;
    std::set<VarStatement*> deletedVarStatements;

    pruneNode(root, KEEP_CHILDREN, [&](CaptedASTNode* currentNode) -> bool {
        VarStatement* varStatement = dyn_cast<VarStatement>(currentNode->getData());
        if (!varStatement) {
            return false;
        }

        // Remove if this variable is not in our "interesting" set
        bool shouldRemove = (interestingVars.count(varStatement->getDecl()) == 0);
        if (shouldRemove) {
            deletedVarStatements.insert(varStatement);
        }

        return shouldRemove;
    });

    pruneNode(root, KEEP_CHILDREN, [&](CaptedASTNode* currentNode) -> bool {
        DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(currentNode->getData());
        if (!declRefStatement) {
            return false;
        }

        // Delete this DeclRefStatement if it references a deleted VarStatement
        return (deletedVarStatements.count(declRefStatement->getVarStatement()) > 0);
    });
}

void ASTPruner::pruneEmptyConstructs(CaptedASTNode* root) {
    // Loop until we're in a steady state
    bool madeChange;
    do {
        madeChange = false;
        {
            // BlockStatements are unique such that they can be pruned if they have at most 1 child OR they have no siblings
            // therefore, they need to give their children (if any) to their parents
            const bool KEEP_CHILDREN = true;
            pruneNode(root, KEEP_CHILDREN, [&](CaptedASTNode* currentNode) -> bool {
                bool shouldRemove = false;

                if (isa<BlockStatement>(currentNode->getData())) {
                    shouldRemove = (currentNode->getNumChildren() <= 1) ||
                                   (currentNode->getParent() && currentNode->getParent()->getNumChildren() == 1);
                }

                madeChange |= shouldRemove;
                return shouldRemove;
            });
        }
        {
            // Other constructs can only be pruned if they have 0 children or 0 grandchildren
            const bool KEEP_CHILDREN = false;
            pruneNode(root, KEEP_CHILDREN, [&](CaptedASTNode* currentNode) -> bool {
                bool shouldRemove = false;

                if (isa<ConditionStatement>(currentNode->getData())) {
                    shouldRemove = (currentNode->getNumChildren() == 0);
                } else if (isa<IfStatement>(currentNode->getData()) || isa<LoopStatement>(currentNode->getData())) {
                    shouldRemove = true;
                    for (CaptedASTNode* child : currentNode->getChildren()) {
                        shouldRemove &= (child->getNumChildren() == 0);
                    }
                }

                madeChange |= shouldRemove;
                return shouldRemove;
            });
        }
    } while (madeChange);
}

//------------------------------------------------------------------------------
// Entrance
//------------------------------------------------------------------------------

void ASTPruner::prune(CaptedASTNode* root) {
    // Reset state before starting
    interestingVars.clear();

    findInterestingVars(root);
    findAliasedInterestingVars(root);

    pruneUselessStatements(root);
    pruneIgnoredFunctions(root);
    pruneUnusedAssignments(root);
    pruneUnusedVarDecl(root);
    pruneEmptyConstructs(root);
}

void ASTPruner::dump() {
    #include <iostream>
    using std::cerr;
    using std::endl;

    cerr << "ASTPruner::interestingVars:" << interestingVars.size() << endl;
    for (auto &var : interestingVars) {
        cerr << var << ": " << var->getNameAsString() << endl;
    }
    cerr << endl;
}
