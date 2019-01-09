#pragma once

#include <vector>
#include "ast/Solution.h"

namespace clang {

class Marker {
    const std::string markerName;

    void printHeader();
    void printFooter();

protected:
    const std::map<std::string, std::vector<float>> keyFns;

    std::vector<std::string> studentFiles;
    std::vector<std::string> referenceFiles;
    std::vector<Solution*> studentSols;
    std::vector<Solution*> referenceSols;

    virtual void markAssignment() = 0;
    std::set<std::string> getInterestingFunctions() const;
    void calculateASTDiff(CaptedASTNode* studentFnAST, CaptedASTNode* referenceFnAST, std::string studentFileName, std::string referenceFileName);

public:
    enum KeyFnCost {
        DELETION,
        INSERTION,
    };

    void setStudentFiles(std::vector<std::string> studentFiles, std::vector<Solution*> studentSols);
    void setReferenceFiles(std::vector<std::string> referenceFiles, std::vector<Solution*> referenceSols);

    Marker(std::string markerName, const std::map<std::string, std::vector<float>> keyFns);
    virtual ~Marker();
    void run();
};

} // namespace clang
