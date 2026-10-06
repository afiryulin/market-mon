
#include <gtest/gtest.h>

extern "C" {
#include "ItchParser.h"
}

TEST(ItchParserTest, ParseAddOrder)
{
    uint8_t payload[] = {'A'};
    itch_event_t event{};

    int rc = itch_parse(payload, sizeof(payload), &event);

    EXPECT_EQ(rc, 0);
    EXPECT_EQ(event.type, ITCH_ADD_ORDER);
}

TEST(ItchParseTest, RejectEmpty)
{
    itch_event_t event{};
    int rc = itch_parse(nullptr, 0, &event);

    EXPECT_NE(rc, 0);
}
