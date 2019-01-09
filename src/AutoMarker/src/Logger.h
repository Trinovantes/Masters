#pragma once

#include <string>
#include <vector>

namespace Logger {

void printBar();
void printDivider();

void printDeduction(std::string message, int marks = 0);
void abortError(std::string message);

} // namespace Logger
