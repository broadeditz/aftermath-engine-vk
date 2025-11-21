#include "camera.hpp"
#include <cmath>
#include <iostream>

FPSCamera::FPSCamera(GLFWwindow* window)
    : window(window)
    , position(0.0f, 0.0f, 0.0f)
    , pitch(0.0f)
    , yaw(90.0f)  // 90 degrees to start facing forward (positive Z)
    , forward(0.0f, 0.0f, 1.0f)
    , right(1.0f, 0.0f, 0.0f)
    , up(0.0f, 1.0f, 0.0f)
    , moveSpeed(10.0f)
    , mouseSensitivity(0.1f)
    , lastMouseX(0.0)
    , lastMouseY(0.0)
    , firstMouse(true)
    , mouseCaptured(false)
{
    // Don't capture mouse immediately - wait for user click
}

void FPSCamera::update(float deltaTime) {
    // Toggle mouse capture with ESC
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (mouseCaptured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            mouseCaptured = false;
            firstMouse = true;
        }
    }

    // Re-capture mouse on click
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        if (!mouseCaptured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            mouseCaptured = true;
            firstMouse = true;
        }
    }

    if (mouseCaptured) {
        processMouse();
    }

    processKeyboard(deltaTime);
    updateCameraVectors();
}

void FPSCamera::processKeyboard(float deltaTime) {
    float velocity = moveSpeed * deltaTime;

    // WASD movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        position += forward * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        position -= forward * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        position += right * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        position -= right * velocity;
    }

    // Vertical movement (noclip style)
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        position += up * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        position -= up * velocity;
    }

    // Speed modifier with Left Control
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        moveSpeed = 7.0f; // Sprint, roughly 25km/h, high end in average humans
    }
    else {
        moveSpeed = 1.750f; // Walk, roughly 6km/h, fast walking speed irl
    }
}

void FPSCamera::processMouse() {
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    if (firstMouse) {
        lastMouseX = mouseX;
        lastMouseY = mouseY;
        firstMouse = false;
        return;
    }

    double xOffset = mouseX - lastMouseX;
    double yOffset = lastMouseY - mouseY;  // Reversed since y-coordinates go from bottom to top

    lastMouseX = mouseX;
    lastMouseY = mouseY;

    xOffset *= mouseSensitivity;
    yOffset *= mouseSensitivity;

    yaw += static_cast<float>(xOffset);
    pitch += static_cast<float>(yOffset);

    // Constrain pitch to avoid gimbal lock
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
}

void FPSCamera::updateCameraVectors() {
    // Calculate the new forward vector
    glm::vec3 newForward;
    newForward.x = sin(glm::radians(yaw)) * (cos(glm::radians(pitch)) * 0.5 + 0.5);
    newForward.y = sin(glm::radians(pitch));
    newForward.z = cos(glm::radians(yaw)) * (cos(glm::radians(pitch)) * 0.5 + 0.5);

    forward = glm::normalize(newForward);

    // Recalculate right and up vectors
    right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, forward));
}