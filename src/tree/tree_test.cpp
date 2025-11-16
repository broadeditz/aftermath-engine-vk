#include "tree.hpp"
#include <iostream>
#include <cmath>

// Simple test macros
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "... "; \
    test_##name(); \
    std::cout << "PASSED" << std::endl; \
} while(0)

#define ASSERT_EQ(actual, expected) do { \
    if ((actual) != (expected)) { \
        std::cerr << "FAILED: " << #actual << " != " << #expected \
                  << " (got " << (actual) << ", expected " << (expected) << ")" << std::endl; \
        std::exit(1); \
    } \
} while(0)

#define ASSERT_NEAR(actual, expected, epsilon) do { \
    if (fabs((actual) - (expected)) > (epsilon)) { \
        std::cerr << "FAILED: " << #actual << " not near " << #expected \
                  << " (got " << (actual) << ", expected " << (expected) \
                  << ", diff " << fabs((actual) - (expected)) << ")" << std::endl; \
        std::exit(1); \
    } \
} while(0)

TEST(getChunkPosition_index0) {
    vec3 parent = { 0, 0, 0 };
    vec3 result = getChunkPosition(0, 1.0f, parent);

    ASSERT_NEAR(result.x, -1.5f, 0.001f);
    ASSERT_NEAR(result.y, -1.5f, 0.001f);
    ASSERT_NEAR(result.z, -1.5f, 0.001f);
}

TEST(getChunkPosition_index63) {
    vec3 parent = { 0, 0, 0 };
    vec3 result = getChunkPosition(63, 1.0f, parent);

    ASSERT_NEAR(result.x, 1.5f, 0.001f);
    ASSERT_NEAR(result.y, 1.5f, 0.001f);
    ASSERT_NEAR(result.z, 1.5f, 0.001f);
}

TEST(calculateLOD_variousDistances) {
    int treeDepth = 6;
    float threshold = 64.0f;

    // At threshold: should return full depth
    ASSERT_EQ(calculateLOD(treeDepth, 64.0f, threshold), 6);

    // Below threshold: should return full depth
    ASSERT_EQ(calculateLOD(treeDepth, 32.0f, threshold), 6);
    ASSERT_EQ(calculateLOD(treeDepth, 16.0f, threshold), 6);
    ASSERT_EQ(calculateLOD(treeDepth, 1.0f, threshold), 6);

    // Double distance (1 doubling): reduce by 1
    ASSERT_EQ(calculateLOD(treeDepth, 128.0f, threshold), 5);

    // Quadruple distance (2 doublings): reduce by 2
    ASSERT_EQ(calculateLOD(treeDepth, 256.0f, threshold), 4);

    // 8x distance (3 doublings): reduce by 3
    ASSERT_EQ(calculateLOD(treeDepth, 512.0f, threshold), 3);

    // 16x distance (4 doublings): reduce by 4
    ASSERT_EQ(calculateLOD(treeDepth, 1024.0f, threshold), 3); // Clamped to minimum 3

    // 32x distance (5 doublings): reduce by 5
    ASSERT_EQ(calculateLOD(treeDepth, 2048.0f, threshold), 3); // Clamped to minimum 3

    // Very far: should clamp to minimum
    ASSERT_EQ(calculateLOD(treeDepth, 10000.0f, threshold), 3);
}

TEST(sampleDistanceAt_aboveFloor) {
    vec3 pos = { 0, 0, 0 };
    float dist = sampleDistanceAt(pos);
    ASSERT_NEAR(dist, 5.0f, 0.001f);
}

int main() {
    std::cout << "=== Running Tree Tests ===" << std::endl;

    RUN_TEST(getChunkPosition_index0);
    RUN_TEST(getChunkPosition_index63);
    RUN_TEST(calculateLOD_variousDistances);
    RUN_TEST(sampleDistanceAt_aboveFloor);

    std::cout << std::endl << "=== All Tests Passed ===" << std::endl;
    return 0;
}