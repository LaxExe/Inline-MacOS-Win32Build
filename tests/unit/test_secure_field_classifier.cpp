#include <gtest/gtest.h>
#include "platform/mac/caret/secure_field_classifier.h"

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
