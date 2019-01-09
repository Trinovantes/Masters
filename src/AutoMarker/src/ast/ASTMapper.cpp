#include "ASTMapper.h"
#include "helpers/IsModifiedChecker.h"

using namespace clang;

//------------------------------------------------------------------------------
// ASTMapper
//------------------------------------------------------------------------------

ASTMapper::ASTMapper(Stmt* stmt, int numNegations) : root(nullptr), numNegations(numNegations) {
    this->TraverseStmt(stmt);
}

ASTMapper::ASTMapper(Decl* decl, int numNegations) : root(nullptr), numNegations(numNegations) {
    this->TraverseDecl(decl);
}

ASTMapper::~ASTMapper() {
    // nop
}

bool ASTMapper::isNegated() const {
    return numNegations % 2 == 1;
}

CaptedASTNode* ASTMapper::createNegationNode(CaptedASTNode* child) {
    CaptedASTNode* notNode = new CaptedASTNode(new OperatorStatement("LogicalNot", UO_LNot));
    notNode->addChild(child);
    return notNode;
}

bool ASTMapper::hasRoot() {
    return (root != nullptr);
}

CaptedASTNode* ASTMapper::getRoot() {
    assert(root);
    assert(!root->getParent());
    return root;
}

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

bool isSimpleForLoop(ForStmt* forStmt) {
    VarDecl* loopCounter = nullptr;

    // If this doesn't have an init, then it will only be a "simple" loop if it doesn't have an increment
    if (!forStmt->getInit()) {
        return !forStmt->getInc();
    }

    if (BinaryOperator* binaryOp = dyn_cast<BinaryOperator>(forStmt->getInit())) {
        // Init is an assignment
        if (binaryOp->isAssignmentOp() || binaryOp->isCompoundAssignmentOp()) {
            if (DeclRefExpr* declRef = dyn_cast<DeclRefExpr>(binaryOp->getLHS())) {
                // If it's any more complex than "i = N" or "i += N", then we won't handle it (just set to nullptr)
                loopCounter = dyn_cast<VarDecl>(declRef->getDecl()->getCanonicalDecl());
            }
        }
    } else if (DeclStmt* declStmt = dyn_cast<DeclStmt>(forStmt->getInit())) {
        // Init is a decl
        if (declStmt->isSingleDecl()) {
            // If it's more complex than a simple "int i = N", then we won't handle it
            loopCounter = dyn_cast<VarDecl>(declStmt->getSingleDecl()->getCanonicalDecl());
        }
    }

    if (!loopCounter) {
        // Couldn't find a simple init stmt
        return false;
    }

    // Verify all vars are not modified in body
    IsModifiedChecker isModifiedChecker(forStmt->getBody());
    if (isModifiedChecker.isModified(loopCounter)) {
        return false;
    }

    // Verify only increment/assignment to loopCounter
    if (BinaryOperator* binaryOp = dyn_cast<BinaryOperator>(forStmt->getInc())) {
        if (binaryOp->isAssignmentOp() || binaryOp->isCompoundAssignmentOp()) {
            return true;
        }
    } else if (UnaryOperator* unaryOp = dyn_cast<UnaryOperator>(forStmt->getInc())) {
        if (unaryOp->isIncrementDecrementOp()) {
            return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------------
// Visitors
//------------------------------------------------------------------------------

bool ASTMapper::VisitReturnStmt(ReturnStmt* returnStmt) {
    assert(!root);
    assert(numNegations == 0);

    root = new CaptedASTNode(new ReturnStatement());

    ASTMapper retValueMapper(returnStmt->getRetValue());
    if (retValueMapper.hasRoot()) {
        root->addChild(retValueMapper.getRoot());
    }

    return false;
}

bool ASTMapper::VisitDeclStmt(DeclStmt* declStmt) {
    assert(!root);
    assert(numNegations == 0);

    root = new CaptedASTNode(new UselessStatement(declStmt->getStmtClassName()));

    for (Decl* decl : declStmt->decls()) {
        VarDecl* varDecl = dyn_cast<VarDecl>(decl);
        if (!varDecl) {
            continue;
        }

        CaptedASTNode* varNode = new CaptedASTNode(new VarStatement(varDecl));
        root->addChild(varNode);

        if (varDecl->hasInit()) {
            // We convert "int a = foo()" (CallStatement child of VarStatement) into
            // "int a; a = foo()" (CallStatement followed by AssnStatement)
            //
            // We do this because ASTPruner looks at LHS of AssnStatement whose RHS is a call to our interested functions
            //
            // If "a" is supposed to be an interesting var because we are interested in foo(),
            // then having it as a child of VarStatement will hide it from ASTPruner::findInterstingVars
            ASTMapper initMapper(varDecl->getInit());
            CaptedASTNode* lhs = new CaptedASTNode(new DeclRefStatement(varDecl));
            CaptedASTNode* rhs = initMapper.getRoot();

            auto assnNode = new CaptedASTNode(new AssnStatement());
            assnNode->addChild(lhs);
            assnNode->addChild(rhs);

            root->addChild(assnNode);
        }
    }

    return false;
}

bool ASTMapper::VisitDeclRefExpr(DeclRefExpr* declRef) {
    assert(!root);

    root = new CaptedASTNode(new DeclRefStatement(declRef->getDecl()));

    if (isNegated()) {
        root = createNegationNode(root);
    }

    return false;
}

bool ASTMapper::VisitCallExpr(CallExpr* callExpr) {
    const FunctionDecl* callee = callExpr->getDirectCallee();
    assert(callee);

    CaptedASTNode* callNode = new CaptedASTNode(new CallStatement(callee->getQualifiedNameAsString()));

    if (isNegated()) {
        root = createNegationNode(callNode);
    } else {
        root = callNode;
    }

    for (const Stmt* arg : callExpr->arguments()) {
        ASTMapper mapper(const_cast<Stmt*>(arg));
        callNode->addChild(mapper.getRoot());
    }

    return false;
}

bool ASTMapper::VisitUnaryOperator(UnaryOperator* unaryOp) {
    assert(!root);

    if (unaryOp->getOpcode() == UO_LNot) { // Logical NOT
        ASTMapper childMapper(unaryOp->getSubExpr(), numNegations + 1);
        numNegations = childMapper.numNegations;
        root = childMapper.getRoot();
    } else {
        UnaryOperatorKind opcode = unaryOp->getOpcode();
        std::string className = unaryOp->getStmtClassName() + UnaryOperator::getOpcodeStr(opcode).str();

        root = new CaptedASTNode(new OperatorStatement(className, opcode));
        ASTMapper childMapper(unaryOp->getSubExpr(), numNegations);
        root->addChild(childMapper.getRoot());
    }

    return false;
}

bool ASTMapper::VisitBinaryOperator(BinaryOperator* binaryOp) {
    assert(!root);

    if (binaryOp->isAssignmentOp() || binaryOp->isCompoundAssignmentOp()) {
        root = new CaptedASTNode(new AssnStatement());
    } else {
        std::string className = binaryOp->getStmtClassName();
        BinaryOperatorKind opcode = binaryOp->getOpcode();

        if (isNegated()) {
            if (BinaryOperator::isComparisonOp(opcode)) {
                opcode = BinaryOperator::negateComparisonOp(opcode);
            } else if (opcode == BO_LAnd) {
                opcode = BO_LOr;
            } else if (opcode == BO_LOr) {
                opcode = BO_LAnd;
            }
        }

        className += BinaryOperator::getOpcodeStr(opcode).str();
        root = new CaptedASTNode(new OperatorStatement(className, opcode));
    }

    ASTMapper lhsMapper(binaryOp->getLHS(), numNegations);
    root->addChild(lhsMapper.getRoot());

    ASTMapper rhsMapper(binaryOp->getRHS(), numNegations);
    root->addChild(rhsMapper.getRoot());

    return false;
}

//------------------------------------------------------------------------------

bool ASTMapper::VisitIfStmt(IfStmt* ifStmt) {
    assert(!root);
    assert(numNegations == 0);

    root = new CaptedASTNode(new IfStatement());

    CaptedASTNode* condNode = new CaptedASTNode(new ConditionStatement());
    ASTMapper condMapper(ifStmt->getCond());
    condNode->addChild(condMapper.getRoot());
    root->addChild(condNode);

    CaptedASTNode* trueNode = new CaptedASTNode(new BlockStatement());
    ASTMapper trueBranchMapper(ifStmt->getThen());
    trueNode->addChild(trueBranchMapper.getRoot());

    CaptedASTNode* falseNode = new CaptedASTNode(new BlockStatement());
    ASTMapper falseBranchMapper(ifStmt->getElse());
    if (falseBranchMapper.hasRoot()) {
        falseNode->addChild(falseBranchMapper.getRoot());
    }

    if (condMapper.isNegated()) {
        root->addChild(falseNode);
        root->addChild(trueNode);
    } else {
        root->addChild(trueNode);
        root->addChild(falseNode);
    }

    return false;
}

bool ASTMapper::VisitWhileStmt(WhileStmt* whileStmt) {
    assert(!root);
    assert(numNegations == 0);

    root = new CaptedASTNode(new LoopStatement(SimpleStatement::ST_LOOP_WHILE));

    CaptedASTNode* condNode = new CaptedASTNode(new ConditionStatement());
    ASTMapper condMapper(whileStmt->getCond());
    condNode->addChild(condMapper.getRoot());
    root->addChild(condNode);

    CaptedASTNode* bodyNode = new CaptedASTNode(new BlockStatement());
    ASTMapper bodyMapper(whileStmt->getBody());
    bodyNode->addChild(bodyMapper.getRoot());
    root->addChild(bodyNode);

    return false;
}

bool ASTMapper::VisitDoStmt(DoStmt* doStmt) {
    assert(!root);
    assert(numNegations == 0);

    root = new CaptedASTNode(new LoopStatement(SimpleStatement::ST_LOOP_DO));

    CaptedASTNode* condNode = new CaptedASTNode(new ConditionStatement());
    ASTMapper condMapper(doStmt->getCond());
    condNode->addChild(condMapper.getRoot());
    root->addChild(condNode);

    CaptedASTNode* bodyNode = new CaptedASTNode(new BlockStatement());
    ASTMapper bodyMapper(doStmt->getBody());
    bodyNode->addChild(bodyMapper.getRoot());
    root->addChild(bodyNode);

    return false;
}

bool ASTMapper::VisitForStmt(ForStmt* forStmt) {
    assert(!root);
    assert(numNegations == 0);

    const bool notStdForLoop = !isSimpleForLoop(forStmt);
    CaptedASTNode* loopNode = new CaptedASTNode(new LoopStatement(SimpleStatement::ST_LOOP_FOR));
    root = loopNode;

    if (notStdForLoop) {
        root = new CaptedASTNode(new BlockStatement());

        // Not all loops have init e.g. for(;;)
        if (forStmt->getInit()) {
            ASTMapper initMapper(forStmt->getInit());
            root->addChild(initMapper.getRoot());
        }

        root->addChild(loopNode);
    }

    CaptedASTNode* condNode = new CaptedASTNode(new ConditionStatement());
    ASTMapper condMapper(forStmt->getCond());
    condNode->addChild(condMapper.getRoot());
    loopNode->addChild(condNode);

    CaptedASTNode* bodyNode = new CaptedASTNode(new BlockStatement());
    ASTMapper bodyMapper(forStmt->getBody());
    bodyNode->addChild(bodyMapper.getRoot());
    loopNode->addChild(bodyNode);

    if (notStdForLoop) {
        // Not all loops have inc
        if (forStmt->getInc()) {
            ASTMapper incMapper(forStmt->getInc());
            bodyNode->addChild(incMapper.getRoot());
        }
    }

    return false;
}

//------------------------------------------------------------------------------

bool ASTMapper::VisitFunctionDecl(FunctionDecl* funcDecl) {
    if (!funcDecl->hasBody()) {
        return true;
    }

    root = new CaptedASTNode(new FunctionStatement(funcDecl));

    for (ParmVarDecl* paramDecl : funcDecl->parameters()) {
        CaptedASTNode* varNode = new CaptedASTNode(new VarStatement(paramDecl));
        root->addChild(varNode);
    }

    ASTMapper bodyMapper(funcDecl->getBody());
    root->addChild(bodyMapper.getRoot());

    return false;
}

bool ASTMapper::VisitStmt(Stmt* stmt) {
    assert(!root);

    // RecursiveASTVisitor calls Visit*() on the parent classes first
    // e.g. VisitStmt is called before VisitCallExpr (CallExpr inherits from Stmt)
    if (isa<ReturnStmt>(stmt) ||
        isa<DeclStmt>(stmt) ||
        isa<DeclRefExpr>(stmt) ||
        isa<CallExpr>(stmt) ||
        isa<UnaryOperator>(stmt) ||
        isa<BinaryOperator>(stmt) ||
        isa<IfStmt>(stmt) ||
        isa<DoStmt>(stmt) ||
        isa<WhileStmt>(stmt) ||
        isa<ForStmt>(stmt)) {
        return true;
    }

    SimpleStatement* simplifiedNode = nullptr;
    if (isa<CompoundStmt>(stmt) || isa<ImplicitCastExpr>(stmt) || isa<ParenExpr>(stmt)) {
        // These nodes have no semantic value (i.e. they exist to make parsing possible)
        simplifiedNode = new UselessStatement(stmt->getStmtClassName());
    } else {
        simplifiedNode = new SimpleStatement(stmt->getStmtClassName());
    }

    root = new CaptedASTNode(simplifiedNode);

    for (Stmt* child : stmt->children()) {
        if (!child) {
            continue;
        }

        ASTMapper mapper(child, numNegations);
        root->addChild(mapper.getRoot());
    }

    return false;
}
