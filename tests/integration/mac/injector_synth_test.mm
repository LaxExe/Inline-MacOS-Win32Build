#import <Cocoa/Cocoa.h>
#import <gtest/gtest.h>
#include "platform/mac/inject/mac_injector.h"
#include "core/injection/token_validation.h"

TEST(InjectorIntegration, PacingAndOpExecution) {
    MacInjector injector;
    injector.setValidToken(InjectionOp::TIER1, 10);
    EXPECT_TRUE(shouldApplyOp(InjectionOp{InjectionOp::TYPE_TEXT, 0, L"test", 10, InjectionOp::TIER1}, 10));
    EXPECT_FALSE(shouldApplyOp(InjectionOp{InjectionOp::TYPE_TEXT, 0, L"test", 10, InjectionOp::TIER1}, 11));
}
