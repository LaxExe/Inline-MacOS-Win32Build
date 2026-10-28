#include "platform/win/caret/secure_field_classifier.h"

bool isSecureFieldRole(const std::string& role, const std::string& subrole) {
    if ((role == "AXTextField" || role == "Edit") && subrole == "AXSecureTextField") {
        return true;
    }
    return false;
}
