#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class FPSCamera {
public:
    FPSCamera(GLFWwindow* window);

    void update(float deltaTime);

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getDirection() const { return forward; }

    void setPosition(const glm::vec3& pos) { position = pos; }
    void setSpeed(float speed) { moveSpeed = speed; }
    void setSensitivity(float sens) { mouseSensitivity = sens; }

private:
    void processKeyboard(float deltaTime);
    void processMouse();

    GLFWwindow* window;

    // Camera position and orientation
    glm::vec3 position;
    float pitch;
    float yaw;

    // Camera vectors
    glm::vec3 forward;
    glm::vec3 right;
    glm::vec3 up;

    // Movement settings
    float moveSpeed;
    float mouseSensitivity;

    // Mouse state
    double lastMouseX;
    double lastMouseY;
    bool firstMouse;
    bool mouseCaptured;

    void updateCameraVectors();
};
