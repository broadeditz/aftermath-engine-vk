#include "camera.hpp"
#include <cmath>
#include <iostream>

FPSCamera::FPSCamera(GLFWwindow* window)
    : window(window)
    , position(0.0f, 0.0f, 0.0f)
    , yaw(0.0f)
    , pitch(0.0f)
    , forward(0.0f, 0.0f, 1.0f)
    , right(1.0f, 0.0f, 0.0f)
    , up(0.0f, 1.0f, 0.0f)
    , moveSpeed(5.0f)
    , mouseSensitivity(0.002f)
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
    // Speed modifier with Left Control
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        moveSpeed = 20.0f; // Fast mode
    }
    else {
        moveSpeed = 5.0f; // Normal speed
    }

    float velocity = moveSpeed * deltaTime;

    // WASD movement

    // TODO:
    // I'm not sure why, but these seem to adapt other keyboard layouts automatically.
    // Investigate why/how, to know if it's reliable.
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        position += forward * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        position -= forward * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        position -= right * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        position += right * velocity;
    }

    // Vertical movement (noclip style)
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        position += up * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        position -= up * velocity;
    }
}

void FPSCamera::processMouse() {
    double currentMouseX, currentMouseY;
    glfwGetCursorPos(window, &currentMouseX, &currentMouseY);

    if (firstMouse) {
        lastMouseX = currentMouseX;
        lastMouseY = currentMouseY;
        firstMouse = false;
        return;
    }

    double offsetX = lastMouseX - currentMouseX;
    double offsetY = lastMouseY - currentMouseY; // Reversed since y-coordinates go from bottom to top

    lastMouseX = currentMouseX;
    lastMouseY = currentMouseY;

    yaw += static_cast<float>(offsetX) * mouseSensitivity;
    pitch += static_cast<float>(offsetY) * mouseSensitivity;

    //std::cout << yaw << std::endl;

    // Constrain pitch to prevent screen flip
    const float maxPitch = glm::radians(89.0f);
    if (pitch > maxPitch) pitch = maxPitch;
    if (pitch < -maxPitch) pitch = -maxPitch;
}

void FPSCamera::updateCameraVectors() {
    // Calculate forward vector from yaw and pitch
    // Yaw rotates around Y axis (horizontal), pitch rotates around X axis (vertical)
    forward.x = cos(yaw) * sin(pitch);
    forward.y = sin(yaw);
    forward.z = cos(yaw) * cos(pitch);
    forward = glm::normalize(forward);

    // Calculate right and up vectors
    right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, forward));
}