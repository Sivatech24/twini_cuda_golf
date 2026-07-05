#include "level_gen.cuh"
#include <curand_kernel.h>
#include <iostream>
#define _USE_MATH_DEFINES
#include <cmath>

#define NUM_SIMULATIONS 10000 
#define MAX_SIM_STEPS 1000

// Simple CUDA physics step to check if a trajectory hits the hole
__global__ void VerifyShotsKernel(Vec2 start, Vec2 hole, Obstacle* d_obstacles, int numObstacles, int* d_successFlag, unsigned long seed) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= NUM_SIMULATIONS) return;

    curandState state;
    curand_init(seed, idx, 0, &state);

    // Randomize angle (0 to 2PI) and power (10 to 100) for this simulation thread
    float angle = curand_uniform(&state) * 2.0f * M_PI;
    float power = 10.0f + curand_uniform(&state) * 90.0f;

    float vx = cos(angle) * power;
    float vy = sin(angle) * power;
    
    Vec2 pos = start;
    float dt = 0.016f; // 60fps step

    // Simulate physics
    for(int step = 0; step < MAX_SIM_STEPS; ++step) {
        pos.x += vx * dt;
        pos.y += vy * dt;
        
        // Simple friction
        vx *= 0.99f;
        vy *= 0.99f;

        // Check distance to hole
        float distSq = (pos.x - hole.x)*(pos.x - hole.x) + (pos.y - hole.y)*(pos.y - hole.y);
        if (distSq < 100.0f) { // Hole radius squared
            atomicExch(d_successFlag, 1); // Mark as solvable!
            return;
        }

        // --- Add AABB Collision with d_obstacles here (Bounce logic) ---
        // (Omitted for brevity: reverse vx/vy based on overlap)
        
        if (abs(vx) < 0.1f && abs(vy) < 0.1f) break; // Ball stopped
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

    // Simulate one stroke (Expand logic to chain strokes for medium/hard)
    VerifyShotsKernel<<<blocksPerGrid, threadsPerBlock>>>(level.startPos, level.holePos, d_obstacles, level.obstacles.size(), d_successFlag, time(NULL));

    cudaMemcpy(&h_successFlag, d_successFlag, sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(d_obstacles);
    cudaFree(d_successFlag);

    return h_successFlag == 1;
}

LevelData GenerateAndVerifyLevel(int difficultyStrokes) {
    LevelData level;
    bool isSolvable = false;
    int attempts = 0; // Prevent infinite loop freezing
    
    while (!isSolvable && attempts < 50) {
        level.obstacles.clear();
        level.startPos = {100.0f, 300.0f};
        level.holePos = {700.0f, 300.0f};
        level.maxStrokes = difficultyStrokes;

        // Procedurally generate obstacles (random math)
        for(int i=0; i < 5; i++) {
            Obstacle obs;
            obs.x = 200.0f + (rand() % 400);
            obs.y = 100.0f + (rand() % 400);
            obs.width = 32.0f + (rand() % 64);
            obs.height = 32.0f + (rand() % 64);
            level.obstacles.push_back(obs);
        }

        // Offload to GPU to verify if this random layout can actually be beaten
        isSolvable = VerifyLevelSolvableCUDA(level, difficultyStrokes);
        attempts++;
    }
    
    // If it failed 50 times, give the player an empty level to prevent locking up
    if (!isSolvable) {
        level.obstacles.clear(); 
    }
    
    return level;
}