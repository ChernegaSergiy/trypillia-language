#include "ErrorHandling.h"
#include <iostream>

void ErrorHandling::reportError(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
}
