#include "platform/mac/caret/secure_field_classifier.h"

bool isSecureFieldRole(const std::string& role, const std::string& subrole) {
    if (role == "AXTextField" && subrole == "AXSecureTextField") {
        return true;
    }
    return false;
}
