#pragma once

#include <glm/glm.hpp>

#include <random>
#include <string>
#include <vector>

struct AABB
{
    glm::vec3 min;
    glm::vec3 max;
};

struct Maze
{
    std::vector<AABB> walls;
    std::vector<glm::vec3> emptyCells;
    float cellSize = 1.0f;
    float wallHeight = 1.75f;
};

Maze buildMazeFromGrid(const std::vector<std::string> &grid, float cellSize, float wallHeight);
glm::vec3 randomEmptyCell(const Maze &maze, std::mt19937 &rng);
