#include "Solution.h"
#include "Logger.h"
#include "ast/ASTMapper.h"
#include "helpers/PostCreateTree.h"

#include "clang/AST/Decl.h"

using namespace clang;

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

void gcNodes(std::map<std::string, CaptedASTNode*> map) {
    for (auto &it : map) {
        delete it.second;
    }
}

//------------------------------------------------------------------------------
// Solution
//------------------------------------------------------------------------------

Solution::Solution(TranslationUnitDecl* translationUnitDecl) : astContext(translationUnitDecl->getASTContext()) {
    this->TraverseDecl(translationUnitDecl);
}

Solution::~Solution() {
    gcNodes(functions);
}

bool Solution::VisitFunctionDecl(FunctionDecl* fnDecl) {
    if (fnDecl->isThisDeclarationADefinition()) {
        std::string fnName = fnDecl->getNameInfo().getAsString();
        assert(fnDecl->isMain() == (fnName == "main"));
        assert(functions.find(fnName) == functions.end());

        ASTMapper mapper(fnDecl);
        CaptedASTNode* root = mapper.getRoot();

        // Since ASTMapper is constructed recursively, we can't call this function inside it
        postCreateTree(root);

        // Finally attach this function to this solution
        functions.insert(std::make_pair(fnName, root));
    }

    return true;
}

std::map<std::string, CaptedASTNode*>::const_iterator Solution::begin() const {
    return functions.begin();
}

std::map<std::string, CaptedASTNode*>::const_iterator Solution::end() const {
    return functions.end();
}

std::string Solution::getFileName() {
    SourceManager &srcManager = astContext.getSourceManager();
    FunctionStatement* main = cast<FunctionStatement>(getFunction("main")->getData());
    return srcManager.getFilename(main->getDecl()->getLocStart()).str();
}

CaptedASTNode* Solution::getFunction(std::string fnName) {
    return functions[fnName];
}

std::set<std::string> Solution::getFunctionNames() {
    std::set<std::string> names;

    for (auto &it : functions) {
        names.insert(it.first);
    }

    return names;
}

//------------------------------------------------------------------------------
// Pruning
//
// ASTPruner relies on VarStatement set of readers/writers created by postCreateTree()
//
// Prunning must be done after inlining for two reasons:
// - Inlining sometimes misbehaves if done after pruning because pruneFunction()
//   removes a lot of semantically useless blocks; however those blocks are
//   synatically important for inlining
// - Pruning must be done after inlining reguardless to clean up the syntatic blocks
//   created from inlining
//------------------------------------------------------------------------------

void Solution::pruneFunctions(const std::set<std::string> &keyFns) {
    for (auto &it : functions) {
        CaptedASTNode* root = it.second;
        ASTPruner astPruner(keyFns);
        astPruner.prune(root);
    }
}

//------------------------------------------------------------------------------
// Inline unexpected student functions
//
// This must be done after construction because it requires all reference solutions
// to be created first (thus this cannot be called inside VisitFunctionDecl()
//------------------------------------------------------------------------------

void verifyReferenceSolutions(const std::vector<Solution*> &referenceSols) {
    assert(referenceSols.size() > 0);

    std::set<std::string> referenceFnNames = referenceSols[0]->getFunctionNames();

    for (size_t i = 1; i < referenceSols.size(); i++) {
        std::set<std::string> fnNames = referenceSols[i]->getFunctionNames();
        assert(referenceFnNames == fnNames);
    }
}

void replaceParamRefWithArgExpr(std::map<ParmVarDecl*, CaptedASTNode*> &paramToCallerArgExpr, CaptedASTNode* root) {
    // Make a copy since replaceParamRefWithArgExpr() will modify the container
    // This is also why we can't use CaptedASTNode::dfs()
    std::list<CaptedASTNode*> childrenCopy = root->getChildren();

    if (DeclRefStatement* declRefStatement = dyn_cast<DeclRefStatement>(root->getData())) {
        assert(childrenCopy.size() == 0); // DeclRefStatement should not have any children

        if (ParmVarDecl* paramDecl = dyn_cast<ParmVarDecl>(declRefStatement->getDecl())) {
            CaptedASTNode* origExpr = paramToCallerArgExpr[paramDecl];
            CaptedASTNode* clonedExpr = origExpr->clone();

            root->getParent()->replaceChild(root, clonedExpr);
        }
    }

    for (CaptedASTNode* child : childrenCopy) {
        replaceParamRefWithArgExpr(paramToCallerArgExpr, child);
    }
}

void replaceFunctionCall(CaptedASTNode* callSite, CaptedASTNode* functionRoot) {
    CaptedASTNode* clonedRetNode = nullptr;
    CaptedASTNode* clonedFnNode = functionRoot->clone();
    postCreateTree(clonedFnNode);
    const int numParams = cast<FunctionStatement>(clonedFnNode->getData())->getDecl()->getNumParams();

    // Find return statement
    clonedFnNode->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
        if (!isa<ReturnStatement>(currentNode->getData())) {
            return;
        }

        if (clonedRetNode != nullptr) {
            std::stringstream ss;
            std::string fnName = cast<CallStatement>(callSite->getData())->getTargetFn();
            ss << "Failed to inline student function '" << fnName << "'' due to multiple return statements";
            Logger::abortError(ss.str());
        }

        clonedRetNode = currentNode;
    });

    // Replace function args with parameters
    {
        std::map<ParmVarDecl*, CaptedASTNode*> paramToCallerArgExpr;
        std::list<CaptedASTNode*>::iterator argIter = callSite->getChildren().begin();
        std::list<CaptedASTNode*>::iterator paramIter = clonedFnNode->getChildren().begin();

        for (int i = 0; i < numParams; i++, argIter++, paramIter++) {
            CaptedASTNode* argExprNode = *argIter;
            CaptedASTNode* paramNode = *paramIter;
            ParmVarDecl* param = cast<ParmVarDecl>(cast<VarStatement>(paramNode->getData())->getDecl());
            paramToCallerArgExpr.insert(std::make_pair(param, argExprNode));
        }

        replaceParamRefWithArgExpr(paramToCallerArgExpr, clonedFnNode);
        postCreateTree(clonedFnNode);
    }

    // Remove function's params
    for (int i = 0; i < numParams; i++) {
        // A function node's first N children are its N params
        CaptedASTNode* param = clonedFnNode->getChildren().front();
        clonedFnNode->getChildren().pop_front();
        delete param;
    }

    // Finally replace callSite
    if (clonedRetNode) {
        // Inline the function in the closest parent block
        std::list<CaptedASTNode*>::iterator dest = callSite->getMyIter();
        CaptedASTNode* parent = callSite->getParent();
        while (!(isa<BlockStatement>(parent->getData()) || isa<UselessStatement>(parent->getData()))) {
            dest = parent->getMyIter();
            parent = parent->getParent();
        }

        parent->insertChild(dest, clonedFnNode);

        // If we have a ret value, then inline it where the callSite is
        assert(clonedRetNode->getNumChildren() == 1);
        CaptedASTNode* clonedRetValNode = clonedRetNode->getIthChild(0);
        clonedRetValNode->detachFromParent();

        clonedRetNode->detachFromParent();
        delete clonedRetNode;

        callSite->getParent()->replaceChild(callSite, clonedRetValNode);
        delete callSite;
    } else {
        // Otherwise, inline the function where the callSite is
        callSite->getParent()->replaceChild(callSite, clonedFnNode);
        delete callSite;
    }
}

void Solution::inlineUnexpectedStudentFunctions(const std::vector<Solution*> &referenceSols) {
    assert(referenceSols.size() > 0);
    verifyReferenceSolutions(referenceSols);

    // Maps function name to dest fn
    std::map<std::string, CaptedASTNode*> fnsToInline;

    {
        // All solutions have same functions
        std::set<std::string> solFnNames = referenceSols[0]->getFunctionNames();

        // Use iter instead of FOR loop because we are potentially removing while iterating
        // (i.e. ConcurrentModificationException)
        auto fnIter = functions.begin();
        while (fnIter != functions.end()) {
            std::string studentFnName = fnIter->first;
            CaptedASTNode* studentFnNode = fnIter->second;

            if (solFnNames.count(studentFnName) == 0) {
                // Student's function is not found in reference solution so it needs to be inlined
                fnsToInline.insert(std::make_pair(studentFnName, studentFnNode));
                fnIter = functions.erase(fnIter);
            } else {
                fnIter++;
            }
        }
    }

    int numExpansions = 0;
    bool madeChange;
    do {
        madeChange = false;

        // Maps callSite to dest fn (not 1-to-1 mapping, multiple callSites can map to same dest fn)
        std::map<CaptedASTNode*, CaptedASTNode*> callSitesToInline;

        for (auto fnIter : functions) {
            CaptedASTNode* studentFnNode = fnIter.second;
            studentFnNode->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
                CallStatement* callStatement = dyn_cast<CallStatement>(currentNode->getData());
                if (!callStatement) {
                    return;
                }

                auto it = fnsToInline.find(callStatement->getTargetFn());
                if (it == fnsToInline.end()) {
                    return;
                }

                callSitesToInline.insert(std::make_pair(currentNode, it->second));
            });
        }

        for (auto &it : callSitesToInline) {
            CaptedASTNode* callSiteNode = it.first;
            CaptedASTNode* fnRoot = it.second;
            replaceFunctionCall(callSiteNode, fnRoot);
            madeChange = true;
        }

        numExpansions++;
        if (numExpansions > MAX_INLINE_EXPANSIONS) {
            Logger::abortError("Exceeded maximum number of inline expansions");
        }
    } while (madeChange);

    gcNodes(fnsToInline);
}
