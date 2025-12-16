#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum Movement { FORWARD, BACKWARD, LEFT, RIGHT };

class Camera {
public:
    glm::vec3 Position, Front, Up;
    float Yaw, Pitch;
    Camera(glm::vec3 startPos);
    glm::mat4 getView();
    void processKeyboard(Movement dir, float delta);
    void processMouse(float xoff, float yoff);
};
