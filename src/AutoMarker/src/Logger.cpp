#include "Logger.h"

#include <iostream>
#include <ostream>
#include <sstream>

using std::cout;
using std::endl;

//-----------------------------------------------------------------------------

enum ColourCode {
    FG_RED      = 31,
    FG_GREEN    = 32,
    FG_YELLOW   = 33,
    FG_BLUE     = 34,
    FG_DEFAULT  = 39,
    BG_RED      = 41,
    BG_GREEN    = 42,
    BG_BLUE     = 44,
    BG_DEFAULT  = 49
};

class Modifier {
    ColourCode colourCode;

public:
    Modifier(ColourCode colourCode) : colourCode(colourCode) {}

    friend
    std::ostream& operator<<(std::ostream& os, const Modifier& mod) {
        return os << "\033[" << mod.colourCode << "m";
    }
};

static Modifier Red(FG_RED);
static Modifier Yellow(FG_YELLOW);
static Modifier Green(FG_GREEN);
static Modifier Default(FG_DEFAULT);

//-----------------------------------------------------------------------------

void Logger::printBar() {
    cout << endl;
    cout << std::string(80, '=') << endl;
    cout << endl;
}

void Logger::printDivider() {
    cout << endl;
    cout << std::string(40, '-') << endl;
    cout << endl;
}

void Logger::printDeduction(std::string message, int marks) {
    cout << Yellow;
    cout << "Deduction: {" << marks << "} " << message << endl;
    cout << Default;
}

void Logger::abortError(std::string message) {
    cout << Red;
    cout << "Abort: " << message << endl;
    cout << Default;

    printBar();
    exit(1);
}
