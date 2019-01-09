#include "PostCreateTree.h"

using namespace clang;

// e.g. x = foo()
// register x written by foo
void registerWritesByAssn(CaptedASTNode* root, std::string ownerFn) {
    bool madeChange;
    do {
        madeChange = false;

        root->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
            if (!isa<AssnStatement>(currentNode->getData())) {
                return;
            }

            CaptedASTNode* lhs = currentNode->getChildren().front();
            CaptedASTNode* rhs = currentNode->getChildren().back();
            std::vector<std::string> rhsWriters;

            rhs->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
                // vars
                if (DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(currentNode->getData())) {
                    VarStatement* varStatement = declRefStatement->getVarStatement();
                    if (!varStatement) {
                        return;
                    }

                    std::vector<std::string> writers = varStatement->getWriters();
                    rhsWriters.insert(rhsWriters.end(), writers.begin(), writers.end());
                }

                // return values from function calls
                if (CallStatement* callStatement = dyn_cast<CallStatement>(currentNode->getData())) {
                    rhsWriters.push_back(callStatement->getTargetFn());
                }
            });

            lhs->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
                DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(currentNode->getData());
                if (!declRefStatement) {
                    return;
                }

                VarStatement* varStatement = declRefStatement->getVarStatement();
                if (!varStatement) {
                    return;
                }

                for (std::string writer : rhsWriters) {
                    madeChange |= varStatement->registerWrite(ownerFn, writer);
                }
            });
        });
    } while (madeChange);
}

// e.g. foo(&x)
// register x written by foo
// register x read by foo
void registerWritesByRef(CaptedASTNode* root, std::string ownerFn) {
    root->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
        CallStatement* callStatement = dyn_cast<CallStatement>(currentNode->getData());
        if (!callStatement) {
            return;
        }

        std::string calledFn = callStatement->getTargetFn();
        int argCounter = 0;

        for (CaptedASTNode* arg : currentNode->getChildren()) {
            assert(arg);

            arg->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
                OperatorStatement* opStatement = dyn_cast<OperatorStatement>(currentNode->getData());
                if (!opStatement) {
                    return;
                }

                if (opStatement->getOpcode() != UO_AddrOf) {
                    return;
                }

                assert(currentNode->getNumChildren() == 1);
                currentNode->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
                    DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(currentNode->getData());
                    if (!declRefStatement) {
                        return;
                    }

                    VarStatement* varStatement = declRefStatement->getVarStatement();
                    if (!varStatement) {
                        return;
                    }

                    varStatement->registerRead(ownerFn, calledFn, argCounter);
                    varStatement->registerWrite(ownerFn, calledFn);
                });
            });

            argCounter++;
        }
    });
}

// foo(x, y, z)
// register x, y, z read by foo
void registerReads(CaptedASTNode* root, std::string ownerFn) {
    // Register reads
    root->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
        CallStatement* callStatement = dyn_cast<CallStatement>(currentNode->getData());
        if (!callStatement) {
            return;
        }

        std::string calledFn = callStatement->getTargetFn();
        int argCounter = 0;
        for (CaptedASTNode* arg : currentNode->getChildren()) {
            assert(arg);
            arg->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
                DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(currentNode->getData());
                if (!declRefStatement) {
                    return;
                }

                VarStatement* varStatement = declRefStatement->getVarStatement();
                if (!varStatement) {
                    return;
                }

                varStatement->registerRead(ownerFn, calledFn, argCounter);
            });

            argCounter++;
        }
    });
}

void clang::postCreateTree(CaptedASTNode* root) {
    FunctionStatement* fnNode = cast<FunctionStatement>(root->getData());
    std::string ownerFn = fnNode->getName();
    std::map<VarDecl*, VarStatement*> varDeclToVarStatement;

    // Build VarDecl (clang) to VarStatement (our datatype) map
    root->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
        VarStatement* varStatement = dyn_cast<VarStatement>(currentNode->getData());
        if (!varStatement) {
            return;
        }

        VarDecl* varDecl = varStatement->getDecl();
        assert(varDeclToVarStatement.count(varDecl) == 0);
        varDeclToVarStatement.insert(std::make_pair(varDecl, varStatement));
    });

    // Register the VarStatement to DeclRefStatement that reference them
    root->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
        DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(currentNode->getData());
        if (!declRefStatement) {
            return;
        }

        VarDecl* varDecl = dyn_cast<VarDecl>(declRefStatement->getDecl());
        if (!varDecl) {
            return;
        }

        auto it = varDeclToVarStatement.find(varDecl);
        if (it == varDeclToVarStatement.end()) {
            return;
        }

        VarStatement* varStatement = it->second;
        declRefStatement->setVarStatement(varStatement);
    });

    registerWritesByAssn(root, ownerFn);
    registerWritesByRef(root, ownerFn);
    registerReads(root, ownerFn);
}
