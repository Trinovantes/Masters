#pragma once

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "Capted.h"
#include "SimpleStatement.h"

namespace clang {

class ASTMapper : public RecursiveASTVisitor<ASTMapper>, capted::InputParser<SimpleStatement> {
    CaptedASTNode* root;
    int numNegations;

    bool isNegated() const;
    static CaptedASTNode* createNegationNode(CaptedASTNode* child);

public:
    explicit ASTMapper(Stmt* stmt, int numNegations = 0);
    explicit ASTMapper(Decl* decl, int numNegations = 0);
    ~ASTMapper();

    virtual bool VisitReturnStmt(ReturnStmt* returnStmt);
    virtual bool VisitDeclStmt(DeclStmt* declStmt);
    virtual bool VisitDeclRefExpr(DeclRefExpr* varDecl);
    virtual bool VisitCallExpr(CallExpr* callExpr);
    virtual bool VisitUnaryOperator(UnaryOperator* unaryOp);
    virtual bool VisitBinaryOperator(BinaryOperator* binaryOp);

    virtual bool VisitIfStmt(IfStmt* ifStmt);
    virtual bool VisitWhileStmt(WhileStmt* whileStmt);
    virtual bool VisitDoStmt(DoStmt* doStmt);
    virtual bool VisitForStmt(ForStmt* forStmt);

    virtual bool VisitFunctionDecl(FunctionDecl* funcDecl);
    virtual bool VisitStmt(Stmt* stmt);

    virtual CaptedASTNode* getRoot() override;
    bool hasRoot();
};

} // namespace clang
