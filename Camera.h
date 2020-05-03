#pragma once

#include <array>

#include <SDL2/SDL.h>

#include "Math.h"

class Camera {
public:
    struct CameraMatrix {
        glm::mat4 view_matrix;
        glm::mat4 projection_matrix;
    };

    CameraMatrix camera_{};

    Camera(RenderEngine& render_engine) : render_engine_(render_engine) {}

    void Update(std::array<bool, SDL_NUM_SCANCODES>& key_state, bool mouse_capture, int mouse_x, int mouse_y) {
        if (mouse_capture) {
            camera_yaw_ = std::fmod(camera_yaw_ - 0.05f * mouse_x, 360.0f);
            camera_pitch_ = std::fmod(camera_pitch_ + 0.05f * mouse_y, 360.0f);
            camera_pitch_ = camera_pitch_ < -89.0f ? -89.0f : camera_pitch_ > 89.0f ? 89.0f : camera_pitch_;

            glm::mat4 rotation = glm::mat4{1.0f};
            rotation = glm::rotate(rotation, glm::radians(camera_pitch_), glm::vec3{-1.0f, 0.0f, 0.0f});
            rotation = glm::rotate(rotation, glm::radians(camera_yaw_), glm::vec3{0.0f, -1.0f, 0.0f});
            camera_forward_ = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) * rotation;
            camera_right_ = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) * rotation;
            camera_up_ = glm::vec4(0.0f, -1.0f, 0.0f, 1.0f) * rotation;
        }

        const float speed = 0.03f;

        if (key_state[SDL_SCANCODE_W]) {
            camera_position_ += speed * camera_forward_;
        }

        if (key_state[SDL_SCANCODE_S]) {
            camera_position_ -= speed * camera_forward_;
        }

        if (key_state[SDL_SCANCODE_A]) {
            camera_position_ += speed * camera_right_;
        }

        if (key_state[SDL_SCANCODE_D]) {
            camera_position_ -= speed * camera_right_;
        }

        camera_.view_matrix = glm::lookAt(camera_position_, camera_position_ + camera_forward_, camera_up_);
        camera_.projection_matrix = glm::perspective(glm::radians(45.0f), render_engine_.swapchain_extent_.width / (float)render_engine_.swapchain_extent_.height, 0.1f, 100.0f);
    }

private:
    RenderEngine& render_engine_;
    glm::vec3 camera_position_{0.0f, 0.5f, -3.0f};
    glm::vec3 camera_forward_{0.0f, 0.0f, 1.0f};
    glm::vec3 camera_right_{1.0f, 0.0f, 0.0f};
    glm::vec3 camera_up_{0.0f, -1.0f, 0.0f};
    float camera_yaw_ = 0.0;
    float camera_pitch_ = 0.0;
};