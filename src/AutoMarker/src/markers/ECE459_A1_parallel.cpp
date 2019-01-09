#include "ECE459_A1_parallel.h"
#include "Logger.h"

using namespace clang;
using namespace clang::ece459;

//------------------------------------------------------------------------------
// A1parallel
//------------------------------------------------------------------------------

A1parallel::A1parallel()
    : Marker("A1parallel", {
        {"read_png_file",     {0, 0}},
        {"write_png_file",    {1, 5}},
        {"paint_destination", {1, 5}},

        {"pthread_create", {100, 100}},
        {"pthread_join",   {100, 100}},
    }) {
    // nop
}

A1parallel::~A1parallel() {
    // nop
}

//------------------------------------------------------------------------------
// Analysis
//------------------------------------------------------------------------------

CaptedASTNode* findPthreadFn(Solution* sol) {
    static const std::string PTHREAD_CALLER = "pthread_create";
    static const int PTHREAD_FN_IDX = 2;
    CaptedASTNode* pthreadFnNode = nullptr;

    for (auto &solIter : *sol) {
        CaptedASTNode* fn = solIter.second;
        fn->dfs([&](CaptedASTNode* currentNode, int depth) -> void {
            CallStatement* callStatement = dyn_cast<CallStatement>(currentNode->getData());
            if (!callStatement) {
                return;
            }

            if (callStatement->getTargetFn() != PTHREAD_CALLER) {
                return;
            }

            CaptedASTNode* pthreadFnRefNode = currentNode->getIthChild(PTHREAD_FN_IDX);
            if (isa<OperatorStatement>(pthreadFnRefNode->getData())) {
                pthreadFnRefNode = pthreadFnRefNode->getChildren().front();
            }

            DeclRefStatement* declRefStatement = cast<DeclRefStatement>(pthreadFnRefNode->getData());
            assert(isa<FunctionDecl>(declRefStatement->getDecl()));
            pthreadFnNode = sol->getFunction(declRefStatement->getName());
        });
    }

    if (!pthreadFnNode) {
        Logger::abortError("Failed to find pthread function in " + sol->getFileName());
    }

    return pthreadFnNode;
}

void A1parallel::markAssignment() {
    assert(studentSols.size() == 1);
    assert(referenceSols.size() >= 1);
    std::set<std::string> interestingFunctions = getInterestingFunctions();

    studentSols[0]->pruneFunctions(interestingFunctions);
    CaptedASTNode* studentPthreadFn = findPthreadFn(studentSols[0]);

    // Iterate over each solution
    for (Solution* referenceSol : referenceSols) {
        referenceSol->pruneFunctions(interestingFunctions);
        CaptedASTNode* referencePthreadFn = findPthreadFn(referenceSol);

        calculateASTDiff(studentPthreadFn, referencePthreadFn, studentSols[0]->getFileName(), referenceSol->getFileName());

        // Iterate over each student function
        for (auto &studentIter : *(studentSols[0])) {
            std::string fnName = studentIter.first;
            CaptedASTNode* studentFnAST = studentIter.second;
            CaptedASTNode* referenceFnAST = referenceSol->getFunction(fnName);

            if (!referenceFnAST) {
                // We don't inline for this assignment because every student/reference solution
                // may potentially have different function name passed to pthread_create.
                // As a result, it'll be impossible to know which function should be inlined
                // or kept. Therefore, we simply ignore student functions that don't have a
                // match in the reference solution.
                continue;
            }

            calculateASTDiff(studentFnAST, referenceFnAST, studentSols[0]->getFileName(), referenceSol->getFileName());
        }
    }
}
