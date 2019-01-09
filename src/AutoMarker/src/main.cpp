#include "Logger.h"
#include "ast/Solution.h"
#include "markers/ECE459_A1_parallel.h"
#include "markers/ECE459_A1_nbio.h"

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/Signals.h"

using namespace clang;
using namespace clang::tooling;

//------------------------------------------------------------------------------
// Configuring help message
//------------------------------------------------------------------------------

// Apply a custom category to all command-line options so that they are the only ones displayed.
static llvm::cl::OptionCategory CAMCategory("ClangAutoMarker options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static llvm::cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static llvm::cl::extrahelp MoreHelp("See README for more info\n");

//------------------------------------------------------------------------------

static llvm::cl::opt<std::string>
CAM_Marker("cam-marker"
    , llvm::cl::desc("Marker to use (a1nbio, a1parallel)")
    , llvm::cl::Required
    , llvm::cl::cat(CAMCategory)
);

static llvm::cl::list<std::string>
CAM_ReferenceSols("cam-reference-solution"
    , llvm::cl::desc("The reference solution files to compare against")
    , llvm::cl::CommaSeparated
    , llvm::cl::cat(CAMCategory)
);

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------

void gcSolutions(std::vector<Solution*> &solutions) {
    for (Solution* sol : solutions) {
        delete sol;
    }
}

int main(int argc, const char* argv[]) {
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    CommonOptionsParser optionsParser(argc, argv, CAMCategory);

    // Need to do AST processing via ClangTool in main() so that the clang ASTs
    // don't get garbage collected before we reach Marker::run()

    const std::vector<std::string> &studentFiles = optionsParser.getSourcePathList();
    const std::vector<std::string> &referenceFiles = CAM_ReferenceSols;
    std::vector<Solution*> studentSols;
    std::vector<Solution*> referenceSols;

    //-------------------------------------------------------------------------
    // Student Files
    //-------------------------------------------------------------------------

    if (studentFiles.size() == 0) {
        Logger::abortError("No student solutions provided");
    }

    if (studentFiles.size() != 1) {
        // We could technically process more than 1 student solution per LLVM instance (process)
        // but it's more reliable if we create a new process via Python script per student in case
        // the compiler crashes -- we don't want to break the compiler if there's a malformed
        // student submission, especially if it's a long batch job with hundreds of students
        Logger::abortError("Must provide 1 input file for student solution");
    }

    ClangTool studentParser(optionsParser.getCompilations(), studentFiles);
    std::vector<std::unique_ptr<ASTUnit>> studentASTs;
    int studentSuccess = studentParser.buildASTs(studentASTs);
    if (studentSuccess != 0) {
        Logger::abortError("Failed to build student ASTs");
    }

    for (std::unique_ptr<ASTUnit> &astPtr : studentASTs) {
        Solution* sol = new Solution(astPtr->getASTContext().getTranslationUnitDecl());
        studentSols.push_back(sol);
    }

    assert(studentSols.size() == 1);

    //-------------------------------------------------------------------------
    // Reference Files
    // Note: Not all markers need reference solutions to work so this is set as
    //       an optional compiler flag
    //-------------------------------------------------------------------------

    ClangTool referenceParser(optionsParser.getCompilations(), referenceFiles);
    std::vector<std::unique_ptr<ASTUnit>> referenceASTs;
    int referenceSuccess = referenceParser.buildASTs(referenceASTs);
    if (referenceSuccess != 0) {
        Logger::abortError("Failed to build reference ASTs");
    }

    for (std::unique_ptr<ASTUnit> &astPtr : referenceASTs) {
        Solution* sol = new Solution(astPtr->getASTContext().getTranslationUnitDecl());
        referenceSols.push_back(sol);
    }

    //-------------------------------------------------------------------------
    // Marker
    //-------------------------------------------------------------------------

    std::unique_ptr<Marker> marker;
    if (CAM_Marker == "a1nbio") {
        marker.reset(new ece459::A1nbio());
    } else if (CAM_Marker == "a1parallel") {
        marker.reset(new ece459::A1parallel());
    } else {
        Logger::abortError("Unknown marker specified: " + CAM_Marker);
    }

    marker->setStudentFiles(studentFiles, studentSols);
    marker->setReferenceFiles(referenceFiles, referenceSols);
    marker->run();

    gcSolutions(studentSols);
    gcSolutions(referenceSols);
    return 0;
}
