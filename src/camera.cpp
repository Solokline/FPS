#include "camera.h"

Camera::Camera(glm::vec3 startPos)
    : Position(startPos), Front(0.0f, 0.0f, -1.0f), Up(0,1,0), Yaw(-90), Pitch(0) {}

glm::mat4 Camera::getView() {
    return glm::lookAt(Position, Position + Front, Up);
}

void Camera::processKeyboard(Movement dir, float delta) {
    float speed = 2.5f * delta;
    if (dir == FORWARD) Position += Front * speed;
    if (dir == BACKWARD) Position -= Front * speed;
    if (dir == LEFT) Position -= glm::normalize(glm::cross(Front, Up)) * speed;
    if (dir == RIGHT) Position += glm::normalize(glm::cross(Front, Up)) * speed;
}

void Camera::processMouse(float xoff, float yoff) {
    float sens = 0.1f;
    xoff *= sens;
    yoff *= sens;
    Yaw += xoff;
    Pitch += yoff;
    if (Pitch > 89.0f) Pitch = 89.0f;
    if (Pitch < -89.0f) Pitch = -89.0f;

    glm::vec3 dir;
    dir.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    dir.y = sin(glm::radians(Pitch));
    dir.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(dir);
}
