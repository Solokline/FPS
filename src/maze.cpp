#include "maze.h"

#include <limits>

static glm::vec3 cellCenter(const std::vector<std::string> &grid, int col, int row, float cellSize)
{
    const float w = (float)grid[0].size();
    const float h = (float)grid.size();
    float x = ((float)col - w * 0.5f + 0.5f) * cellSize;
    float z = ((float)row - h * 0.5f + 0.5f) * cellSize;
    return {x, 0.0f, z};
}

Maze buildMazeFromGrid(const std::vector<std::string> &grid, float cellSize, float wallHeight)
{
    Maze maze;
    maze.cellSize = cellSize;
    maze.wallHeight = wallHeight;

    for (int r = 0; r < (int)grid.size(); r++)
    {
        for (int c = 0; c < (int)grid[r].size(); c++)
        {
            glm::vec3 center = cellCenter(grid, c, r, cellSize);
            if (grid[r][c] == '#')
            {
                AABB box;
                box.min = center + glm::vec3(-0.5f * cellSize, 0.0f, -0.5f * cellSize);
                box.max = center + glm::vec3(0.5f * cellSize, wallHeight, 0.5f * cellSize);
                maze.walls.push_back(box);
            }
            else
            {
                maze.emptyCells.push_back(center);
            }
        }
    }

    return maze;
}

glm::vec3 randomEmptyCell(const Maze &maze, std::mt19937 &rng)
{
    if (maze.emptyCells.empty())
        return {0.0f, 0.0f, 0.0f};
    std::uniform_int_distribution<size_t> dist(0, maze.emptyCells.size() - 1);
    return maze.emptyCells[dist(rng)];
}
