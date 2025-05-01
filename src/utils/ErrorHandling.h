#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H

#include <string>

class ErrorHandling {
public:
    static void reportError(const std::string& message);
};

#endif // ERROR_HANDLING_H
