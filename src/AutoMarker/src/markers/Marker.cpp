#include "Marker.h"
#include "Logger.h"
#include "costmodels/KeyFnCostModel.h"
#include <iostream>

using namespace clang;
using std::cout;
using std::endl;

//------------------------------------------------------------------------------
// Marker
//------------------------------------------------------------------------------

Marker::Marker(std::string markerName, const std::map<std::string, std::vector<float>> keyFns)
    : markerName(markerName)
    , keyFns(keyFns) {
    // nop
}

Marker::~Marker() {
    // nop
}

void Marker::run() {
    printHeader();
    markAssignment();
    printFooter();
}

void Marker::calculateASTDiff(CaptedASTNode* studentFnAST, CaptedASTNode* referenceFnAST, std::string studentFileName, std::string referenceFileName) {
    KeyFnCostModel costModel(keyFns);
    capted::Apted<SimpleStatement> algorithm(&costModel);

    float diff = algorithm.computeEditDistance(studentFnAST, referenceFnAST); // src to dest
    FunctionStatement* studentFnNode = cast<FunctionStatement>(studentFnAST->getData());
    FunctionStatement* referenceFnNode = cast<FunctionStatement>(referenceFnAST->getData());

    cout
        << "stuFile:" << studentFileName << " "
        << "refFile:" << referenceFileName << " "
        << "stuFn:" << studentFnNode->getName() << " "
        << "refFn:" << referenceFnNode->getName() << " "
        << "diff:" << diff << " "
        << endl;
}

std::set<std::string> Marker::getInterestingFunctions() const {
    std::set<std::string> interestingFunctions;

    for (auto &pair : keyFns) {
        interestingFunctions.insert(pair.first);
    }

    return interestingFunctions;
}

//------------------------------------------------------------------------------
// Registering Files
//------------------------------------------------------------------------------

void Marker::setStudentFiles(std::vector<std::string> studentFiles, std::vector<Solution*> studentSols) {
    assert(this->studentFiles.size() == 0);
    assert(this->studentSols.size() == 0);
    assert(studentFiles.size() == studentSols.size());

    this->studentFiles = studentFiles;
    this->studentSols = studentSols;
}

void Marker::setReferenceFiles(std::vector<std::string> referenceFiles, std::vector<Solution*> referenceSols) {
    assert(this->referenceFiles.size() == 0);
    assert(this->referenceSols.size() == 0);
    assert(referenceFiles.size() == referenceSols.size());

    this->referenceFiles = referenceFiles;
    this->referenceSols = referenceSols;
}

//------------------------------------------------------------------------------
// Printing
//------------------------------------------------------------------------------

void Marker::printHeader() {
    Logger::printBar();

    cout << "Clang AutoMarker Tool" << endl;
    cout << "Created by Stephen Li" << endl;

    cout << endl;
    cout << "Current Marker: " << markerName << endl;

    cout << endl;
    cout << "Student Solutions:" << endl;
    for (auto &f : studentFiles) {
        cout << f << endl;
    }

    cout << endl;
    cout << "Reference Solutions:" << endl;
    for (auto &f : referenceFiles) {
        cout << f << endl;
    }

    Logger::printDivider();
}

void Marker::printFooter() {
    Logger::printBar();
}
