#include <unity.h>

void setUp() {}
void tearDown() {}

void test_alat_uji_berjalan()
{
    TEST_ASSERT_EQUAL(4, 2 + 2);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_alat_uji_berjalan);
    return UNITY_END();
}