#include "ECE459_A1_nbio.h"
#include "Logger.h"

using namespace clang;
using namespace clang::ece459;

//------------------------------------------------------------------------------
// A1nbio
//------------------------------------------------------------------------------

A1nbio::A1nbio()
    : Marker("A1nbio", {
        {"debug_deadbeef", {10, 10}},

        {"read_png_file",     {0, 0}},
        {"write_png_file",    {1, 5}},
        {"paint_destination", {1, 5}},

        {"curl_easy_setopt",         {10, 10}},
        {"curl_multi_add_handle",    {10, 10}},
        {"curl_multi_cleanup",       {10, 10}},
        {"curl_multi_fdset",         {10, 10}},
        {"curl_multi_info_read",     {10, 10}},
        {"curl_multi_init",          {10, 10}},
        {"curl_multi_perform",       {10, 10}},
        {"curl_multi_remove_handle", {10, 10}},
        {"curl_multi_timeout",       {10, 10}},
        {"curl_multi_wait",          {10, 10}},
        {"select",                   {10, 10}},
    }) {
    // nop
}

A1nbio::~A1nbio() {
    // nop
}

//------------------------------------------------------------------------------
// Analysis
//------------------------------------------------------------------------------

void A1nbio::markAssignment() {
    assert(studentSols.size() == 1);
    assert(referenceSols.size() >= 1);
    std::set<std::string> interestingFunctions = getInterestingFunctions();

    // Ideally students shouldn't need more than the main() and provided helper functions.
    // However, if they do add their own helper functions for reducing repeated code, then
    // we'll inline them so that they have the same resulting set of functions as the
    // reference solutions

    studentSols[0]->inlineUnexpectedStudentFunctions(referenceSols);
    studentSols[0]->pruneFunctions(interestingFunctions);

    // CaptedASTNode* root = studentSols[0]->getFunction("main");
    // dumpCaptedASTNode(root);

    // Iterate over each solution
    for (Solution* referenceSol : referenceSols) {
        referenceSol->pruneFunctions(interestingFunctions);

        // Iterate over each student function
        for (auto &studentIter : *(studentSols[0])) {
            std::string fnName = studentIter.first;
            CaptedASTNode* studentFn = studentIter.second;
            CaptedASTNode* referenceFn = referenceSol->getFunction(fnName);

            if (!referenceFn) {
                Logger::abortError("Student has a function '" + fnName + "' that doesn't exist in reference solution");
            }

            calculateASTDiff(studentFn, referenceFn, studentSols[0]->getFileName(), referenceSol->getFileName());
        }
    }
}
