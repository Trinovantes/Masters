#pragma once

#include <set>
#include "clang/AST/Stmt.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "Capted.h"

namespace clang {

// Simplification of llvm's Stmt and Decl
// - There is no distinction between the llvm types after simplification for simplicity sake
// - We use "Statement" to avoid confusion with llvm's Stmt type

class SimpleStatement {
public:
    enum StatementType {
        ST_OTHER,
        ST_USELESS,
        ST_RETURN,
        ST_OPERATOR,

        ST_VAR,
        ST_DECL_REF,
        ST_CALL,
        ST_ASSN,

        ST_IF,
        ST_CONDITION,
        ST_LOOP_START,
        ST_LOOP_FOR,
        ST_LOOP_WHILE,
        ST_LOOP_DO,
        ST_LOOP_END,

        ST_SCOPE_BEGIN,
        ST_BLOCK,
        ST_FUNCTION,
        ST_SCOPE_END,
    };

protected:
    const std::string className;
    const StatementType statementType;

public:
    SimpleStatement(std::string className, StatementType statementType = ST_OTHER)
        : className(className)
        , statementType(statementType) {
        assert(!className.empty());
    }

    virtual ~SimpleStatement() { }

    virtual std::string toString() const {
        return className;
    }

    StatementType getKind() const {
        return statementType;
    }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() == ST_OTHER;
    }

    virtual SimpleStatement* clone() const {
        return new SimpleStatement(className, statementType);
    }
};

typedef capted::Node<SimpleStatement> CaptedASTNode;

// For debugging
void dumpCaptedASTNode(CaptedASTNode* root);

// For Capted library
SimpleStatement* cloneData(const SimpleStatement* original);

//-------------------------------------
// UselessStatement
//-------------------------------------

class UselessStatement : public SimpleStatement {
    const std::string origName;

public:
    UselessStatement(std::string className)
        : SimpleStatement("UselessStatement-" + className, ST_USELESS)
        , origName(className)
        { }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() == ST_USELESS;
    }

    virtual UselessStatement* clone() const override {
        return new UselessStatement(origName);
    }
};

//-------------------------------------
// ReturnStatement
//-------------------------------------

class ReturnStatement : public SimpleStatement {
public:
    ReturnStatement()
        : SimpleStatement("ReturnStatement", ST_RETURN)
        { }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() == ST_RETURN;
    }

    virtual ReturnStatement* clone() const override {
        return new ReturnStatement();
    }
};

//-------------------------------------
// OperatorStatement
//-------------------------------------

class OperatorStatement : public SimpleStatement {
    const std::string origName;
    int opcode;

public:
    OperatorStatement(std::string className, int opcode)
        : SimpleStatement("OperatorStatement-" + className, ST_OPERATOR)
        , origName(className)
        , opcode(opcode)
        { }

    int getOpcode() {
        return opcode;
    }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() == ST_OPERATOR;
    }

    virtual OperatorStatement* clone() const override {
        return new OperatorStatement(origName, opcode);
    }
};

//=============================================================================

//-------------------------------------
// VarStatement
//-------------------------------------

class VarStatement : public SimpleStatement {
    enum UseType {
        UNKNOWN,
        READ,
        WRITE,
    };

    class VarUse {
    public:
        // If read:  var is used as a parameter (e.g. foo(var))
        // If write: var is used as a return (e.g. var = foo())

        const std::string ownerFn;
        const std::string userFn;
        const int operand;
        const UseType useType = UNKNOWN;

        VarUse(std::string ownerFn, std::string userFn, int operand, UseType useType)
            : ownerFn(ownerFn)
            , userFn(userFn)
            , operand(operand)
            , useType(useType)
            { }

        VarUse(const VarUse &varUse)
            : ownerFn(varUse.ownerFn)
            , userFn(varUse.userFn)
            , operand(varUse.operand)
            , useType(varUse.useType)
            { }

        std::string toString() const {
            std::stringstream ss;
            ss << "{";

            ss << ownerFn << "|";

            switch (useType) {
                case READ:  ss << "r"; break;
                case WRITE: ss << "w"; break;
                default:    assert(false);
            }
            ss << "|";

            ss << userFn;
            if (useType == READ) {
                ss << ":" << std::to_string(operand);
            }

            ss << "}";

            return ss.str();
        }

        friend bool operator<(const VarUse &lhs, const VarUse &rhs) {
            if (lhs.ownerFn != rhs.ownerFn) {
                return lhs.ownerFn < rhs.ownerFn;
            }

            if (lhs.userFn != rhs.userFn) {
                return lhs.userFn < rhs.userFn;
            }

            return lhs.operand < rhs.operand;
        }
    };

    VarDecl* canonicalDecl;
    std::string varName;
    std::set<VarUse> writes;
    std::set<VarUse> reads;

public:
    VarStatement(VarDecl* varDecl)
        : SimpleStatement("VarStatement", ST_VAR)
        , canonicalDecl(varDecl->getCanonicalDecl())
        , varName(canonicalDecl->getNameAsString())
        , writes({})
        , reads({})
        { }

    VarStatement(const VarStatement &varStatement)
        : SimpleStatement("VarStatement", ST_VAR)
        , canonicalDecl(varStatement.canonicalDecl)
        , varName(varStatement.varName)
        , writes(varStatement.writes)
        , reads(varStatement.reads)
        { }

    virtual ~VarStatement() {
        // nop
    }

    virtual std::string toString() const override {
        std::stringstream ss;
        ss << className << ":" << varName << " decl:" << canonicalDecl;

        for (const VarUse &use : reads) {
            ss << " " << use.toString();
        }

        for (const VarUse &use : writes) {
            ss << " " << use.toString();
        }

        return ss.str();
    }

    VarDecl* getDecl() {
        return canonicalDecl;
    }

    std::string getName() {
        return varName;
    }

    std::vector<std::string> getReaders() {
        std::vector<std::string> readers;

        for (const VarUse &use : reads) {
            assert(use.useType == READ);
            readers.push_back(use.userFn);
        }

        return readers;
    }

    std::vector<std::string> getWriters() {
        std::vector<std::string> writers;

        for (const VarUse &use : writes) {
            assert(use.useType == WRITE);
            writers.push_back(use.userFn);
        }

        return writers;
    }

    bool registerRead(std::string ownerFn, std::string readerFn, int operand) {
        std::pair<std::set<VarUse>::iterator, bool> res = reads.insert(VarUse(ownerFn, readerFn, operand, READ));
        return res.second;
    }

    bool registerWrite(std::string ownerFn, std::string writerFn) {
        std::pair<std::set<VarUse>::iterator, bool> res = writes.insert(VarUse(ownerFn, writerFn, 0, WRITE));
        return res.second;
    }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() == ST_VAR;
    }

    virtual VarStatement* clone() const override {
        return new VarStatement(*this);
    }
};

//-------------------------------------
// DeclRefStatement
//-------------------------------------

class DeclRefStatement : public SimpleStatement {
    VarStatement* varStatement;
    Decl* canonicalDecl;
    std::string targetName;

public:
    DeclRefStatement(NamedDecl* namedDeclRef)
        : SimpleStatement("DeclRefStatement", ST_DECL_REF)
        , varStatement(nullptr)
        , canonicalDecl(namedDeclRef->getCanonicalDecl())
        , targetName(namedDeclRef->getNameAsString())
        { }

    DeclRefStatement(const DeclRefStatement &declRefStatement)
        : SimpleStatement("DeclRefStatement", ST_DECL_REF)
        , varStatement(nullptr)
        , canonicalDecl(declRefStatement.canonicalDecl)
        , targetName(declRefStatement.targetName)
        { }

    Decl* getDecl() {
        return canonicalDecl;
    }

    void setVarStatement(VarStatement* varStatement) {
        // Should be called from postCreateTree()
        assert(this->varStatement == nullptr || this->varStatement == varStatement);
        assert(isa<VarDecl>(canonicalDecl));
        this->varStatement = varStatement;
    }

    VarStatement* getVarStatement() {
        return varStatement;
    }

    std::string getName() {
        return targetName;
    }

    virtual std::string toString() const override {
        std::stringstream ss;
        ss << className << ":" << targetName << " decl:" << canonicalDecl;
        return ss.str();
    }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() == ST_DECL_REF;
    }

    virtual DeclRefStatement* clone() const override {
        return new DeclRefStatement(*this);
    }
};

//-------------------------------------
// CallStatement
//-------------------------------------

class CallStatement : public SimpleStatement {
    std::string targetFn;

public:
    CallStatement(std::string targetFn)
        : SimpleStatement("CallStatement", ST_CALL)
        , targetFn(targetFn)
        { }

    std::string getTargetFn() const {
        return targetFn;
    }

    virtual std::string toString() const override {
        return className + " " + targetFn;
    }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() == ST_CALL;
    }

    virtual CallStatement* clone() const override {
        return new CallStatement(targetFn);
    }
};

//-------------------------------------
// AssnStatement
//-------------------------------------

class AssnStatement : public SimpleStatement {
public:
    AssnStatement()
        : SimpleStatement("AssnStatement", ST_ASSN)
        { }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() == ST_ASSN;
    }

    virtual AssnStatement* clone() const override {
        return new AssnStatement();
    }
};

//=============================================================================

//-------------------------------------
// IfStatement
//-------------------------------------

class IfStatement : public SimpleStatement {
public:
    IfStatement()
        : SimpleStatement("IfStatement", ST_IF)
        { }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() == ST_IF;
    }

    virtual IfStatement* clone() const override {
        return new IfStatement();
    }
};

//-------------------------------------
// ConditionStatement
//-------------------------------------

class ConditionStatement : public SimpleStatement {
public:
    ConditionStatement()
        : SimpleStatement("ConditionStatement", ST_CONDITION)
        { }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() == ST_CONDITION;
    }

    virtual ConditionStatement* clone() const override {
        return new ConditionStatement();
    }
};

//-------------------------------------
// LoopStatement
//-------------------------------------

class LoopStatement : public SimpleStatement {
public:
    LoopStatement(StatementType loopType)
        : SimpleStatement("LoopStatement", loopType)
    {
        assert(ST_LOOP_START < loopType && loopType < ST_LOOP_END);
    }

    virtual std::string toString() const override {
        std::string loopType;
        switch (statementType) {
            case ST_LOOP_FOR:   loopType = "FOR";   break;
            case ST_LOOP_DO:    loopType = "DO";    break;
            case ST_LOOP_WHILE: loopType = "WHILE"; break;
            default: loopType = "UNKNOWN LOOP";
        }

        return className + " " + loopType;
    }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() > ST_LOOP_START &&
               node->getKind() < ST_LOOP_END;
    }

    virtual LoopStatement* clone() const override {
        return new LoopStatement(statementType);
    }
};

//=============================================================================

//-------------------------------------
// BlockStatement
//-------------------------------------

class BlockStatement : public SimpleStatement {
public:
    BlockStatement()
        : SimpleStatement("BlockStatement", ST_BLOCK)
        { }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() > ST_SCOPE_BEGIN &&
               node->getKind() < ST_SCOPE_END;
    }

    virtual BlockStatement* clone() const override {
        return new BlockStatement();
    }
};

//-------------------------------------
// FunctionStatement
//-------------------------------------

class FunctionStatement : public SimpleStatement {
    FunctionDecl* fnDecl;

public:
    FunctionStatement(FunctionDecl* fnDecl)
        : SimpleStatement("FunctionStatement-" + fnDecl->getNameInfo().getAsString(), ST_FUNCTION)
        , fnDecl(fnDecl)
        { }

    FunctionStatement(const FunctionStatement &fnNode)
        : SimpleStatement("FunctionStatement-" + fnNode.getName(), ST_FUNCTION)
        , fnDecl(fnNode.fnDecl)
        { }

    FunctionDecl* getDecl() {
        return fnDecl;
    }

    std::string getName() const {
        return fnDecl->getNameInfo().getAsString();
    }

    static bool classof(const SimpleStatement* node) {
        return node->getKind() == ST_FUNCTION;
    }

    virtual FunctionStatement* clone() const override {
        return new FunctionStatement(*this);
    }
};

} // namespace clang
