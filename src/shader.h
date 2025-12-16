#pragma once
#include <string>
#include <glad/glad.h>

class Shader {
public:
    GLuint ID;
    Shader(const char* vert, const char* frag);
    void use();
};
