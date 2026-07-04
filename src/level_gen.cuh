#pragma once
#include <vector>

struct Vec2 { float x, y; };

struct Obstacle {
    float x, y, width, height;
};

struct LevelData {
    Vec2 startPos;
    Vec2 holePos;
    std::vector<Obstacle> obstacles;
    int maxStrokes;
};

// Returns true if the procedurally generated level is solvable
bool VerifyLevelSolvableCUDA(const LevelData& level, int difficultyStrokes);
LevelData GenerateAndVerifyLevel(int difficultyStrokes);