#include <unity.h>
#include "sanding_config.h"

void setUp() {}
void tearDown() {}

void test_config_bisa_dipakai_tanpa_arduino()
{
    TEST_ASSERT_TRUE(config::HEARTBEAT_INTERVAL < 15000UL);
}

void test_topic_memakai_base_yang_benar()
{
    TEST_ASSERT_EQUAL_STRING("pabrik/sanding/jumlah", config::topicJumlah);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_config_bisa_dipakai_tanpa_arduino);
    RUN_TEST(test_topic_memakai_base_yang_benar);
    return UNITY_END();
}