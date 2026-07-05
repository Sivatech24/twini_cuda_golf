#include "level_gen.cuh"
#include <curand_kernel.h>
#include <iostream>
#include <ctime>
#define _USE_MATH_DEFINES
#include <cmath>

#define NUM_SIMULATIONS 10000 
#define MAX_SIM_STEPS 1000

__global__ void VerifyShotsKernel(Vec2 start, Vec2 hole, Obstacle* d_obstacles, int numObstacles, int* d_successFlag, unsigned long seed) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= NUM_SIMULATIONS) return;

    curandState state;
    curand_init(seed, idx, 0, &state);

    float angle = curand_uniform(&state) * 2.0f * M_PI;
    float power = 10.0f + curand_uniform(&state) * 90.0f;

    float vx = cos(angle) * power;
    float vy = sin(angle) * power;
    
    Vec2 pos = start;
    float dt = 0.016f;

    for(int step = 0; step < MAX_SIM_STEPS; ++step) {
        pos.x += vx * dt;
        pos.y += vy * dt;
        
        vx *= 0.99f;
        vy *= 0.99f;

        // Check if ball is in hole (approximate radius)
        float distSq = (pos.x - hole.x)*(pos.x - hole.x) + (pos.y - hole.y)*(pos.y - hole.y);
        if (distSq < 200.0f) { 
            atomicExch(d_successFlag, 1);
            return;
        }

        // Basic AABB Collision with Obstacles
        for (int i = 0; i < numObstacles; i++) {
            Obstacle obs = d_obstacles[i];
            if (pos.x + 8 > obs.x && pos.x - 8 < obs.x + obs.width &&
                pos.y + 8 > obs.y && pos.y - 8 < obs.y + obs.height) {
                // Crude bounce (reverse velocity)
                vx *= -1.0f;
                vy *= -1.0f;
                break; 
            }
        }
        
        if (abs(vx) < 0.1f && abs(vy) < 0.1f) break; 
    }
}

bool VerifyLevelSolvableCUDA(const LevelData& level, int difficultyStrokes) {
    Obstacle* d_obstacles;
    int* d_successFlag;
    int h_successFlag = 0;

    cudaMalloc(&d_obstacles, level.obstacles.size() * sizeof(Obstacle));
    cudaMemcpy(d_obstacles, level.obstacles.data(), level.obstacles.size() * sizeof(Obstacle), cudaMemcpyHostToDevice);
    
    cudaMalloc(&d_successFlag, sizeof(int));
    cudaMemcpy(d_successFlag, &h_successFlag, sizeof(int), cudaMemcpyHostToDevice);

    int threadsPerBlock = 256;
    int blocksPerGrid = (NUM_SIMULATIONS + threadsPerBlock - 1) / threadsPerBlock;

    VerifyShotsKernel<<<blocksPerGrid, threadsPerBlock>>>(level.startPos, level.holePos, d_obstacles, level.obstacles.size(), d_successFlag, time(NULL));

    cudaMemcpy(&h_successFlag, d_successFlag, sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(d_obstacles);
    cudaFree(d_successFlag);

    return h_successFlag == 1;
}

LevelData GenerateAndVerifyLevel(int difficultyStrokes) {
    LevelData level;
    bool isSolvable = false;
    int attempts = 0;
    
    // Seed standard rand for CPU side generation
    srand(time(NULL));

    while (!isSolvable && attempts < 50) {
        level.obstacles.clear();
        
        // Fit within left split screen (320x480)
        level.startPos = {64.0f, 400.0f}; // Bottom center of left screen
        level.holePos = {160.0f, 64.0f};  // Top center of left screen
        level.maxStrokes = difficultyStrokes;

        // Generate 6 to 10 random 32x32 tiles mapped to a grid
        int numTiles = 6 + (rand() % 5);
        for(int i = 0; i < numTiles; i++) {
            Obstacle obs;
            // Snap to 32-pixel grid to look like a real tiled game
            obs.x = (float)((rand() % 8 + 1) * 32);  // x: 32 to 256
            obs.y = (float)((rand() % 8 + 4) * 32);  // y: 128 to 352
            obs.width = 32.0f;
            obs.height = 32.0f;
            level.obstacles.push_back(obs);
        }

        isSolvable = VerifyLevelSolvableCUDA(level, difficultyStrokes);
        attempts++;
    }
    
    if (!isSolvable) {
        level.obstacles.clear(); // Safe fallback
    }
    
    return level;
}