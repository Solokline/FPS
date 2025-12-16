#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shader.h"
#include "camera.h"

#include <vector>
#include <iostream>
#include <cstdlib>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// размеры окна
unsigned int SCR_W = 800;
unsigned int SCR_H = 600;

// камера
Camera camera({0.0f, 1.0f, 3.0f});
float lastX, lastY;
bool firstMouse = true;

// время
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// мишени
struct Target {
    glm::vec3 pos;
    bool alive = true;
};
std::vector<Target> targets;

// таймер спавна
float spawnTimer = 0.0f;
float spawnInterval = 2.0f;

// прицел
GLuint crossVAO, crossVBO;
GLuint crossTexture;

// изменение размера окна
void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
    SCR_W = w;
    SCR_H = h;
}

// движение мыши
void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.processMouse(xoffset, yoffset);
}

// ввод с клавиатуры
void processInput(GLFWwindow* window) {
    float current = glfwGetTime();
    deltaTime = current - lastFrame;
    lastFrame = current;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// стрельба
void shoot() {
    glm::vec3 rayDir = glm::normalize(camera.Front);
    float bestDist = 1000.0f;
    Target* best = nullptr;

    for (auto& t : targets) {
        if (!t.alive) continue;

        glm::vec3 to = t.pos - camera.Position;
        float dist = glm::length(to);
        float dot = glm::dot(glm::normalize(to), rayDir);

        if (dot > 0.995f && dist < 20.0f && dist < bestDist) {
            bestDist = dist;
            best = &t;
        }
    }

    if (best) best->alive = false;
}

// загрузка текстуры
GLuint loadTexture(const char* path) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int w, h, c;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &c, 4);

    if (!data) {
        std::cout << "Не удалось загрузить текстуру: " << path << std::endl;
        return 0;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    return tex;
}

// создание прицела
void setupCrosshair() {
    float size = 16.0f;
    float cx = SCR_W * 0.5f;
    float cy = SCR_H * 0.5f;

    float quad[] = {
        cx - size, cy + size, 0, 1,
        cx - size, cy - size, 0, 0,
        cx + size, cy - size, 1, 0,

        cx - size, cy + size, 0, 1,
        cx + size, cy - size, 1, 0,
        cx + size, cy + size, 1, 1
    };

    glGenVertexArrays(1, &crossVAO);
    glGenBuffers(1, &crossVBO);

    glBindVertexArray(crossVAO);
    glBindBuffer(GL_ARRAY_BUFFER, crossVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

int main() {
    glfwInit();

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    SCR_W = mode->width;
    SCR_H = mode->height;

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
        glfwCreateWindow(SCR_W, SCR_H, "SimpleFPS", nullptr, nullptr);

    glfwMakeContextCurrent(window);
    glfwSetWindowPos(window, 0, 0);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Ошибка инициализации GLAD" << std::endl;
        return -1;
    }

    glViewport(0, 0, SCR_W, SCR_H);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader shader("shaders/vertex.glsl", "shaders/fragment.glsl");
    Shader crossShader("shaders/cross_vert.glsl", "shaders/cross_frag.glsl");

    float cube[] = {
        -0.5,-0.5,-0.5,  0.5,-0.5,-0.5,
         0.5, 0.5,-0.5, -0.5, 0.5,-0.5,
        -0.5,-0.5, 0.5,  0.5,-0.5, 0.5,
         0.5, 0.5, 0.5, -0.5, 0.5, 0.5
    };

    unsigned int idx[] = {
        0,1,2,2,3,0, 4,5,6,6,7,4,
        0,1,5,5,4,0, 2,3,7,7,6,2,
        0,3,7,7,4,0, 1,2,6,6,5,1
    };

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
    glEnableVertexAttribArray(0);

    targets.push_back({{0, 0.5f, -5}});
    targets.push_back({{2, 0.5f, -7}});
    targets.push_back({{-2, 0.5f, -7}});

    setupCrosshair();
    crossTexture = loadTexture("textures/crosshair.png");

    bool wasPressed = false;

    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        bool pressed =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (pressed && !wasPressed)
            shoot();
        wasPressed = pressed;

        spawnTimer += deltaTime;
        if (spawnTimer >= spawnInterval) {
            spawnTimer = 0.0f;
            for (auto& t : targets)
                if (!t.alive) {
                    t.pos = {(rand()%800-400)/100.0f, 0.5f, -(rand()%5+2)};
                    t.alive = true;
                    break;
                }
        }

        glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)SCR_W / SCR_H, 0.1f, 100.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"), 1, GL_FALSE, &proj[0][0]);

        glm::mat4 view = camera.getView();
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, &view[0][0]);

        glBindVertexArray(VAO);

        glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), {0, -0.5f, 0}), {10, 0.1f, 10});
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"), 1, GL_FALSE, &model[0][0]);
        glUniform3f(glGetUniformLocation(shader.ID, "color"), 0.5f, 0.5f, 0.5f);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        for (auto& t : targets) {
            if (!t.alive) continue;
            model = glm::translate(glm::mat4(1.0f), t.pos);
            glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"), 1, GL_FALSE, &model[0][0]);
            glUniform3f(glGetUniformLocation(shader.ID, "color"), 1.0f, 0.2f, 0.2f);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        glDisable(GL_DEPTH_TEST);
        crossShader.use();
        glm::mat4 ortho = glm::ortho(0.0f, (float)SCR_W, 0.0f, (float)SCR_H);
        glUniformMatrix4fv(glGetUniformLocation(crossShader.ID, "ortho"), 1, GL_FALSE, &ortho[0][0]);
        glBindVertexArray(crossVAO);
        glBindTexture(GL_TEXTURE_2D, crossTexture);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
