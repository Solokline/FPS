#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "maze.h"
#include "shader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace
{
    static constexpr float kPlayerRadius = 0.22f;
    static constexpr float kPlayerEyeHeight = 1.0f;

    static constexpr float kEnemyRadius = 0.45f;
    static constexpr int kEnemyCount = 6;
    static constexpr float kEnemyY = 0.5f;

    static constexpr float kRespawnInterval = 2.0f;

    static const std::vector<std::string> kMazeGrid = {
        "#################",
        "#.######........#",
        "#.######.###.####",
        "#.##.....#......#",
        "#.######.#......#",
        "#.###....#......#",
        "#.######.########",
        "#...............#",
        "#.######.######.#",
        "#.######.######.#",
        "#.######.######.#",
        "#.######.######.#",
        "#.######.######.#",
        "#...............#",
        "#################",
    };

    struct Target
    {
        glm::vec3 pos{0.0f};
        bool alive = true;
    };

    struct GlMesh
    {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
    };

    struct Crosshair
    {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint texture = 0;
    };

    struct AppState
    {
        unsigned int width = 800;
        unsigned int height = 600;
        GLFWwindow *window = nullptr;

        Camera camera{glm::vec3(0.0f, kPlayerEyeHeight, 3.0f)};
        bool firstMouse = true;
        float lastX = 0.0f;
        float lastY = 0.0f;

        float deltaTime = 0.0f;
        float lastFrame = 0.0f;

        Maze maze;
        std::mt19937 rng{std::random_device{}()};

        std::vector<Target> targets;
        float spawnTimer = 0.0f;

        GlMesh cube;
        GlMesh texturedCube;
        GlMesh cubeEdges;
        GLuint wallTexture = 0;
        Crosshair cross;
    };

    static float clampf(float v, float lo, float hi)
    {
        if (v < lo)
            return lo;
        if (v > hi)
            return hi;
        return v;
    }

    static bool circleIntersectsAABB_XZ(const glm::vec3 &pos, float radius, const AABB &box)
    {
        float closestX = clampf(pos.x, box.min.x, box.max.x);
        float closestZ = clampf(pos.z, box.min.z, box.max.z);
        float dx = pos.x - closestX;
        float dz = pos.z - closestZ;
        return dx * dx + dz * dz < radius * radius;
    }

    static bool isBlocked(const Maze &maze, const glm::vec3 &pos, float radius)
    {
        for (const auto &w : maze.walls)
            if (circleIntersectsAABB_XZ(pos, radius, w))
                return true;
        return false;
    }

    static bool rayAABB(const glm::vec3 &origin, const glm::vec3 &dir, const AABB &box, float &tHit)
    {
        float tmin = 0.0f;
        float tmax = std::numeric_limits<float>::infinity();

        for (int axis = 0; axis < 3; axis++)
        {
            float o = origin[axis];
            float d = dir[axis];
            float mn = box.min[axis];
            float mx = box.max[axis];

            if (std::fabs(d) < 1e-6f)
            {
                if (o < mn || o > mx)
                    return false;
                continue;
            }

            float inv = 1.0f / d;
            float t1 = (mn - o) * inv;
            float t2 = (mx - o) * inv;
            if (t1 > t2)
                std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax)
                return false;
        }

        tHit = tmin;
        return tmax >= 0.0f;
    }

    static bool raySphere(const glm::vec3 &origin, const glm::vec3 &dir, const glm::vec3 &center, float radius, float &tHit)
    {
        glm::vec3 oc = origin - center;
        float b = glm::dot(oc, dir);
        float c = glm::dot(oc, oc) - radius * radius;
        float h = b * b - c;
        if (h < 0.0f)
            return false;
        h = std::sqrt(h);

        float t0 = -b - h;
        float t1 = -b + h;
        if (t1 < 0.0f)
            return false;
        tHit = (t0 >= 0.0f) ? t0 : t1;
        return true;
    }

    static float nearestWallT(const Maze &maze, const glm::vec3 &origin, const glm::vec3 &dir)
    {
        float best = std::numeric_limits<float>::infinity();
        for (const auto &w : maze.walls)
        {
            float t = 0.0f;
            if (rayAABB(origin, dir, w, t) && t >= 0.0f)
                best = std::min(best, t);
        }
        return best;
    }

    static GLuint loadTextureRGBA(const char *path)
    {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int w = 0, h = 0, c = 0;
        stbi_set_flip_vertically_on_load(true);
        unsigned char *data = stbi_load(path, &w, &h, &c, 4);
        if (!data)
        {
            std::cout << "Не удалось загрузить текстуру: " << path << std::endl;
            return 0;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
        return tex;
    }

    static GLuint loadTextureRGBARepeat(const char *path)
    {
        GLuint tex = loadTextureRGBA(path);
        if (!tex)
            return 0;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        return tex;
    }

    static void setupCubeMesh(GlMesh &m)
    {
        float cube[] = {
            -0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            0.5f,
            -0.5f,
            0.5f,
            0.5f,
            0.5f,
            0.5f,
            -0.5f,
            0.5f,
            0.5f,
        };

        unsigned int idx[] = {
            0,
            1,
            2,
            2,
            3,
            0,
            4,
            5,
            6,
            6,
            7,
            4,
            0,
            1,
            5,
            5,
            4,
            0,
            2,
            3,
            7,
            7,
            6,
            2,
            0,
            3,
            7,
            7,
            4,
            0,
            1,
            2,
            6,
            6,
            5,
            1,
        };

        glGenVertexArrays(1, &m.vao);
        glGenBuffers(1, &m.vbo);
        glGenBuffers(1, &m.ebo);

        glBindVertexArray(m.vao);
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
    }

    static void setupTexturedCubeMesh(GlMesh &m)
    {
        // 36 vertices (no indices): pos(3) + uv(2)
        const float v[] = {
            // +X
            0.5f,
            -0.5f,
            -0.5f,
            0,
            0,
            0.5f,
            -0.5f,
            0.5f,
            1,
            0,
            0.5f,
            0.5f,
            0.5f,
            1,
            1,
            0.5f,
            -0.5f,
            -0.5f,
            0,
            0,
            0.5f,
            0.5f,
            0.5f,
            1,
            1,
            0.5f,
            0.5f,
            -0.5f,
            0,
            1,
            // -X
            -0.5f,
            -0.5f,
            0.5f,
            0,
            0,
            -0.5f,
            -0.5f,
            -0.5f,
            1,
            0,
            -0.5f,
            0.5f,
            -0.5f,
            1,
            1,
            -0.5f,
            -0.5f,
            0.5f,
            0,
            0,
            -0.5f,
            0.5f,
            -0.5f,
            1,
            1,
            -0.5f,
            0.5f,
            0.5f,
            0,
            1,
            // +Y
            -0.5f,
            0.5f,
            -0.5f,
            0,
            0,
            0.5f,
            0.5f,
            -0.5f,
            1,
            0,
            0.5f,
            0.5f,
            0.5f,
            1,
            1,
            -0.5f,
            0.5f,
            -0.5f,
            0,
            0,
            0.5f,
            0.5f,
            0.5f,
            1,
            1,
            -0.5f,
            0.5f,
            0.5f,
            0,
            1,
            // -Y
            -0.5f,
            -0.5f,
            0.5f,
            0,
            0,
            0.5f,
            -0.5f,
            0.5f,
            1,
            0,
            0.5f,
            -0.5f,
            -0.5f,
            1,
            1,
            -0.5f,
            -0.5f,
            0.5f,
            0,
            0,
            0.5f,
            -0.5f,
            -0.5f,
            1,
            1,
            -0.5f,
            -0.5f,
            -0.5f,
            0,
            1,
            // +Z
            -0.5f,
            -0.5f,
            0.5f,
            0,
            0,
            -0.5f,
            0.5f,
            0.5f,
            0,
            1,
            0.5f,
            0.5f,
            0.5f,
            1,
            1,
            -0.5f,
            -0.5f,
            0.5f,
            0,
            0,
            0.5f,
            0.5f,
            0.5f,
            1,
            1,
            0.5f,
            -0.5f,
            0.5f,
            1,
            0,
            // -Z
            0.5f,
            -0.5f,
            -0.5f,
            0,
            0,
            0.5f,
            0.5f,
            -0.5f,
            0,
            1,
            -0.5f,
            0.5f,
            -0.5f,
            1,
            1,
            0.5f,
            -0.5f,
            -0.5f,
            0,
            0,
            -0.5f,
            0.5f,
            -0.5f,
            1,
            1,
            -0.5f,
            -0.5f,
            -0.5f,
            1,
            0,
        };

        glGenVertexArrays(1, &m.vao);
        glGenBuffers(1, &m.vbo);

        glBindVertexArray(m.vao);
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    static void setupCubeEdgesMesh(GlMesh &m)
    {
        // 12 edges => 24 vertices (GL_LINES), pos(3)
        const float e[] = {
            // bottom square
            -0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            0.5f,
            0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            -0.5f,
            -0.5f,
            // top square
            -0.5f,
            0.5f,
            -0.5f,
            0.5f,
            0.5f,
            -0.5f,
            0.5f,
            0.5f,
            -0.5f,
            0.5f,
            0.5f,
            0.5f,
            0.5f,
            0.5f,
            0.5f,
            -0.5f,
            0.5f,
            0.5f,
            -0.5f,
            0.5f,
            0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            // verticals
            -0.5f,
            -0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            0.5f,
            0.5f,
            0.5f,
            0.5f,
            -0.5f,
            -0.5f,
            0.5f,
            -0.5f,
            0.5f,
            0.5f,
        };

        glGenVertexArrays(1, &m.vao);
        glGenBuffers(1, &m.vbo);

        glBindVertexArray(m.vao);
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(e), e, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
    }

    static void setupCrosshair(Crosshair &c, unsigned int w, unsigned int h)
    {
        float size = 16.0f;
        float cx = w * 0.5f;
        float cy = h * 0.5f;

        float quad[] = {
            cx - size,
            cy + size,
            0,
            1,
            cx - size,
            cy - size,
            0,
            0,
            cx + size,
            cy - size,
            1,
            0,

            cx - size,
            cy + size,
            0,
            1,
            cx + size,
            cy - size,
            1,
            0,
            cx + size,
            cy + size,
            1,
            1,
        };

        glGenVertexArrays(1, &c.vao);
        glGenBuffers(1, &c.vbo);

        glBindVertexArray(c.vao);
        glBindBuffer(GL_ARRAY_BUFFER, c.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    static void respawnDeadTargets(AppState &s)
    {
        s.spawnTimer += s.deltaTime;
        if (s.spawnTimer < kRespawnInterval)
            return;
        s.spawnTimer = 0.0f;

        for (auto &t : s.targets)
        {
            if (t.alive)
                continue;
            glm::vec3 p = randomEmptyCell(s.maze, s.rng);
            t.pos = {p.x, kEnemyY, p.z};
            t.alive = true;
            break;
        }
    }

    static void shoot(AppState &s)
    {
        glm::vec3 rayDir = glm::normalize(s.camera.Front);
        float wallT = nearestWallT(s.maze, s.camera.Position, rayDir);

        float bestT = std::numeric_limits<float>::infinity();
        Target *best = nullptr;
        for (auto &t : s.targets)
        {
            if (!t.alive)
                continue;
            float tHit = 0.0f;
            if (!raySphere(s.camera.Position, rayDir, t.pos, kEnemyRadius, tHit))
                continue;
            if (tHit >= wallT)
                continue;
            if (tHit < bestT)
            {
                bestT = tHit;
                best = &t;
            }
        }

        if (best)
            best->alive = false;
    }

    static void updateDelta(AppState &s)
    {
        float current = (float)glfwGetTime();
        s.deltaTime = current - s.lastFrame;
        s.lastFrame = current;
    }

    static void movePlayer(AppState &s)
    {
        float speed = 3.0f;
        if (glfwGetKey(s.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            speed = 5.0f;

        glm::vec3 forward = glm::vec3(s.camera.Front.x, 0.0f, s.camera.Front.z);
        if (glm::length(forward) < 1e-4f)
            forward = {0.0f, 0.0f, -1.0f};
        forward = glm::normalize(forward);
        glm::vec3 right = glm::normalize(glm::cross(forward, s.camera.Up));

        glm::vec3 move(0.0f);
        if (glfwGetKey(s.window, GLFW_KEY_W) == GLFW_PRESS)
            move += forward;
        if (glfwGetKey(s.window, GLFW_KEY_S) == GLFW_PRESS)
            move -= forward;
        if (glfwGetKey(s.window, GLFW_KEY_A) == GLFW_PRESS)
            move -= right;
        if (glfwGetKey(s.window, GLFW_KEY_D) == GLFW_PRESS)
            move += right;

        if (glm::length(move) > 0.0f)
        {
            move = glm::normalize(move) * speed * s.deltaTime;

            glm::vec3 next = s.camera.Position;
            next.x += move.x;
            if (!isBlocked(s.maze, next, kPlayerRadius))
                s.camera.Position.x = next.x;

            next = s.camera.Position;
            next.z += move.z;
            if (!isBlocked(s.maze, next, kPlayerRadius))
                s.camera.Position.z = next.z;
        }

        s.camera.Position.y = kPlayerEyeHeight;
    }

    static void processInput(AppState &s)
    {
        if (glfwGetKey(s.window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(s.window, true);

        movePlayer(s);
    }

    static void render(AppState &s, Shader &shader, Shader &crossShader, Shader &texShader)
    {
        glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)s.width / s.height, 0.1f, 100.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"), 1, GL_FALSE, &proj[0][0]);

        glm::mat4 view = s.camera.getView();
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, &view[0][0]);

        glBindVertexArray(s.cube.vao);

        // floor
        float mazeW = (float)kMazeGrid[0].size() * s.maze.cellSize;
        float mazeH = (float)kMazeGrid.size() * s.maze.cellSize;
        glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), {0, -0.05f, 0}), {mazeW, 0.1f, mazeH});
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"), 1, GL_FALSE, &model[0][0]);
        glUniform3f(glGetUniformLocation(shader.ID, "color"), 0.35f, 0.35f, 0.35f);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // walls (textured)
        texShader.use();
        glUniformMatrix4fv(glGetUniformLocation(texShader.ID, "projection"), 1, GL_FALSE, &proj[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(texShader.ID, "view"), 1, GL_FALSE, &view[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s.wallTexture);
        glUniform1i(glGetUniformLocation(texShader.ID, "tex"), 0);
        glBindVertexArray(s.texturedCube.vao);

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        for (const auto &box : s.maze.walls)
        {
            glm::vec3 center = (box.min + box.max) * 0.5f;
            glm::vec3 size = (box.max - box.min);
            model = glm::scale(glm::translate(glm::mat4(1.0f), center), size);
            glUniformMatrix4fv(glGetUniformLocation(texShader.ID, "model"), 1, GL_FALSE, &model[0][0]);
            float uScale = std::max(size.x, size.z);
            float vScale = size.y;
            glUniform2f(glGetUniformLocation(texShader.ID, "uvScale"), uScale, vScale);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glDisable(GL_POLYGON_OFFSET_FILL);

        // wall outline without diagonals (edges only)
        shader.use();
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"), 1, GL_FALSE, &proj[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, &view[0][0]);
        glBindVertexArray(s.cubeEdges.vao);
        glLineWidth(2.0f);
        glUniform3f(glGetUniformLocation(shader.ID, "color"), 0.05f, 0.06f, 0.08f);
        for (const auto &box : s.maze.walls)
        {
            glm::vec3 center = (box.min + box.max) * 0.5f;
            glm::vec3 size = (box.max - box.min);
            model = glm::scale(glm::translate(glm::mat4(1.0f), center), size);
            glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"), 1, GL_FALSE, &model[0][0]);
            glDrawArrays(GL_LINES, 0, 24);
        }

        // targets
        glBindVertexArray(s.cube.vao);
        for (const auto &t : s.targets)
        {
            if (!t.alive)
                continue;
            model = glm::translate(glm::mat4(1.0f), t.pos);
            glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"), 1, GL_FALSE, &model[0][0]);
            glUniform3f(glGetUniformLocation(shader.ID, "color"), 1.0f, 0.2f, 0.2f);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // crosshair
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        crossShader.use();
        glm::mat4 ortho = glm::ortho(0.0f, (float)s.width, 0.0f, (float)s.height);
        glUniformMatrix4fv(glGetUniformLocation(crossShader.ID, "ortho"), 1, GL_FALSE, &ortho[0][0]);
        glBindVertexArray(s.cross.vao);
        glBindTexture(GL_TEXTURE_2D, s.cross.texture);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    static void framebufferSizeCallback(GLFWwindow *window, int w, int h)
    {
        glViewport(0, 0, w, h);
        auto *s = (AppState *)glfwGetWindowUserPointer(window);
        if (!s)
            return;
        s->width = (unsigned int)w;
        s->height = (unsigned int)h;
    }

    static void mouseCallback(GLFWwindow *window, double xpos, double ypos)
    {
        auto *s = (AppState *)glfwGetWindowUserPointer(window);
        if (!s)
            return;
        if (s->firstMouse)
        {
            s->lastX = (float)xpos;
            s->lastY = (float)ypos;
            s->firstMouse = false;
        }

        float xoffset = (float)xpos - s->lastX;
        float yoffset = s->lastY - (float)ypos;
        s->lastX = (float)xpos;
        s->lastY = (float)ypos;

        s->camera.processMouse(xoffset, yoffset);
    }
} // namespace

int main()
{
    AppState s;

    if (!glfwInit())
    {
        std::cout << "GLFW init failed" << std::endl;
        return 1;
    }

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    s.width = (unsigned int)mode->width;
    s.height = (unsigned int)mode->height;

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    s.window = glfwCreateWindow((int)s.width, (int)s.height, "SimpleFPS", nullptr, nullptr);
    if (!s.window)
    {
        std::cout << "GLFW window create failed" << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(s.window);
    glfwSetWindowPos(s.window, 0, 0);
    glfwSetWindowUserPointer(s.window, &s);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "GLAD init failed" << std::endl;
        glfwDestroyWindow(s.window);
        glfwTerminate();
        return 1;
    }

    glViewport(0, 0, (int)s.width, (int)s.height);
    glfwSetFramebufferSizeCallback(s.window, framebufferSizeCallback);
    glfwSetCursorPosCallback(s.window, mouseCallback);
    glfwSetInputMode(s.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);

    Shader shader("shaders/vertex.glsl", "shaders/fragment.glsl");
    Shader crossShader("shaders/cross_vert.glsl", "shaders/cross_frag.glsl");
    Shader texShader("shaders/tex_vertex.glsl", "shaders/tex_fragment.glsl");

    setupCubeMesh(s.cube);
    setupTexturedCubeMesh(s.texturedCube);
    setupCubeEdgesMesh(s.cubeEdges);
    setupCrosshair(s.cross, s.width, s.height);
    s.cross.texture = loadTextureRGBA("textures/crosshair.png");
    s.wallTexture = loadTextureRGBARepeat("textures/blue_wall.jpg");

    s.maze = buildMazeFromGrid(kMazeGrid, 1.0f, 1.75f);
    if (!s.maze.emptyCells.empty())
        s.camera.Position = s.maze.emptyCells.front() + glm::vec3(0.0f, kPlayerEyeHeight, 0.0f);

    s.targets.clear();
    for (int i = 0; i < kEnemyCount; i++)
    {
        glm::vec3 p = randomEmptyCell(s.maze, s.rng);
        s.targets.push_back({{p.x, kEnemyY, p.z}, true});
    }

    bool wasPressed = false;

    while (!glfwWindowShouldClose(s.window))
    {
        updateDelta(s);
        processInput(s);

        bool pressed = glfwGetMouseButton(s.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (pressed && !wasPressed)
            shoot(s);
        wasPressed = pressed;

        respawnDeadTargets(s);
        render(s, shader, crossShader, texShader);

        glfwSwapBuffers(s.window);
        glfwPollEvents();
    }

    glfwDestroyWindow(s.window);
    glfwTerminate();
    return 0;
}
