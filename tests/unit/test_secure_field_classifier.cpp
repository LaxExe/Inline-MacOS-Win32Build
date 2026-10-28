#include <gtest/gtest.h>
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/caret/secure_field_classifier.h"
#else
#include "platform/win/caret/secure_field_classifier.h"
#endif

TEST(SecureFieldClassifier, RecognizesSecureTextField) {
    EXPECT_TRUE(isSecureFieldRole("AXTextField", "AXSecureTextField"));
}
TEST(SecureFieldClassifier, NormalTextFieldIsNotSecure) {
    EXPECT_FALSE(isSecureFieldRole("AXTextField", ""));
}
TEST(SecureFieldClassifier, NonTextFieldRoleIsNotSecure) {
    EXPECT_FALSE(isSecureFieldRole("AXButton", "AXSecureTextField")); // malformed input, must not misclassify
}
TEST(SecureFieldClassifier, EmptyRoleIsNotSecure) {
    EXPECT_FALSE(isSecureFieldRole("", ""));
}
