#include <gtest/gtest.h>
#include "core/buffers/buffer_gate.h"

TEST(BufferGate, SuppressesWriteWhenSecure) {
    CurrentWordBuffer buf;
    bool wrote = appendCharIfAllowed(buf, L'p', /*secureFieldActive=*/true);
    EXPECT_FALSE(wrote);
    EXPECT_TRUE(buf.chars.empty()); // nothing leaked into the buffer at all
}
TEST(BufferGate, AllowsWriteWhenNotSecure) {
    CurrentWordBuffer buf;
    bool wrote = appendCharIfAllowed(buf, L'h', /*secureFieldActive=*/false);
    EXPECT_TRUE(wrote);
    EXPECT_EQ(buf.chars, L"h");
}
