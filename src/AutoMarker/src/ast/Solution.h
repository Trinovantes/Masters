#pragma once

#include <iterator>
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Decl.h"
#include "ASTPruner.h"
#include "Capted.h"
#include "SimpleStatement.h"

namespace clang {

class Solution : public RecursiveASTVisitor<Solution> {
    static const int MAX_INLINE_EXPANSIONS = 10;

    ASTContext &astContext;
    std::map<std::string, CaptedASTNode*> functions;

public:
    Solution(TranslationUnitDecl* translationUnitDecl);
    virtual ~Solution();
    virtual bool VisitFunctionDecl(FunctionDecl* funcDecl);

    std::map<std::string, CaptedASTNode*>::const_iterator begin() const;
    std::map<std::string, CaptedASTNode*>::const_iterator end() const;

    void pruneFunctions(const std::set<std::string> &keyFns);
    void inlineUnexpectedStudentFunctions(const std::vector<Solution*> &referenceSols);

    std::string getFileName();
    CaptedASTNode* getFunction(std::string name);
    std::set<std::string> getFunctionNames();
};

}
