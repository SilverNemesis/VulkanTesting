#include <chrono>
#include <stdexcept>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#ifdef _DEBUG
#pragma comment(lib, "SDL2maind.lib")
#else
#pragma comment(lib, "SDL2main.lib")
#endif

#include "RenderEngine.h"
#include "CubeApplication.h"
#include "ModelApplication.h"
#include "SpriteApplication.h"
#include "FontApplication.h"

class Application : RenderApplication {
public:
    Application(int window_width, int window_height) : window_width_(window_width), window_height_(window_height) {}

    void Startup() {
        SDL_Log("application startup");
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            throw std::runtime_error(SDL_GetError());
        }

        window_ = SDL_CreateWindow("Vulkan Testing", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width_, window_height_, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_VULKAN);

        if (window_ == nullptr) {
            throw std::runtime_error(SDL_GetError());
        }

        application_.Startup(this, window_width_, window_height_);
    }

    void Run() {
        long long frame_time = 0;

        auto previous_time = std::chrono::high_resolution_clock::now();

        while (!window_closed_) {
            ProcessInput();

            auto current_time = std::chrono::high_resolution_clock::now();
            frame_time += std::chrono::duration_cast<std::chrono::microseconds>(current_time - previous_time).count();
            previous_time = current_time;

            while (frame_time > 4000) {
                frame_time -= 4000;
                Update();
            }

            if (!window_minimized_) {
                Render();
            }
        }
    }

    void Shutdown() {
        SDL_Log("application shutdown");
        application_.Shutdown();
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    CubeApplication application_;
    SDL_Window* window_ = NULL;
    int window_width_;
    int window_height_;

    bool window_minimized_ = false;
    bool window_closed_ = false;

    std::array<bool, SDL_NUM_SCANCODES> key_state_{};
    bool mouse_capture_ = false;

    glm::vec3 camera_position_{0.0f, 0.5f, -3.0f};
    glm::vec3 camera_forward_{0.0f, 0.0f, 1.0f};
    glm::vec3 camera_right_{1.0f, 0.0f, 0.0f};
    glm::vec3 camera_up_{0.0f, -1.0f, 0.0f};
    float camera_yaw_ = 0.0;
    float camera_pitch_ = 0.0;

    void ProcessInput() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                window_closed_ = true;
                break;
            case SDL_KEYDOWN:
                if (event.key.repeat == 0) {
                    key_state_[event.key.keysym.scancode] = true;
                }
                switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_ESCAPE:
                    if (mouse_capture_) {
                        mouse_capture_ = false;
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                    } else {
                        window_closed_ = true;
                    }
                    break;
                case SDL_SCANCODE_SPACE:
                    if (mouse_capture_) {
                        mouse_capture_ = false;
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                    } else {
                        mouse_capture_ = true;
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        SDL_GetRelativeMouseState(NULL, NULL);
                    }
                    break;
                }
                break;
            case SDL_KEYUP:
                if (event.key.repeat == 0) {
                    key_state_[event.key.keysym.scancode] = false;
                }
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                case SDL_WINDOWEVENT_RESIZED:
                    SDL_Vulkan_GetDrawableSize(window_, &window_width_, &window_height_);
                    application_.Resize(window_width_, window_height_);
                    break;
                case SDL_WINDOWEVENT_MINIMIZED:
                    window_minimized_ = true;
                    break;
                case SDL_WINDOWEVENT_MAXIMIZED:
                case SDL_WINDOWEVENT_RESTORED:
                    window_minimized_ = false;
                    break;
                }
                break;
            }
        }
    }

    void Update() {
        int mouse_x;
        int mouse_y;
        SDL_GetRelativeMouseState(&mouse_x, &mouse_y);

        if (mouse_capture_) {
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

        if (key_state_[SDL_SCANCODE_W]) {
            camera_position_ += speed * camera_forward_;
        }

        if (key_state_[SDL_SCANCODE_S]) {
            camera_position_ -= speed * camera_forward_;
        }

        if (key_state_[SDL_SCANCODE_A]) {
            camera_position_ += speed * camera_right_;
        }

        if (key_state_[SDL_SCANCODE_D]) {
            camera_position_ -= speed * camera_right_;
        }

        glm::mat4 view_matrix = glm::lookAt(camera_position_, camera_position_ + camera_forward_, camera_up_);

        application_.Update(view_matrix);
    }

    void Render() {
        application_.Render();
    }

    void GetRequiredExtensions(std::vector<const char*>& required_extensions) {
        uint32_t required_extension_count = 0;
        SDL_Vulkan_GetInstanceExtensions(window_, &required_extension_count, nullptr);
        required_extensions.resize(required_extension_count);
        SDL_Vulkan_GetInstanceExtensions(window_, &required_extension_count, required_extensions.data());
    }

    void CreateSurface(VkInstance& instance, VkSurfaceKHR& surface) {
        if (!SDL_Vulkan_CreateSurface(window_, instance, &surface)) {
            throw std::runtime_error("failed to create window surface");
        }
    }

    void GetDrawableSize(int& window_width, int& window_height) {
        SDL_Vulkan_GetDrawableSize(window_, &window_width, &window_height);
    }

    void PipelineReset() {
        application_.PipelineReset();
    }

    void PipelineRebuild() {
        application_.PipelineRebuild();
    }
};

void (*(RenderEngine::Log))(const char* fmt, ...) = SDL_Log;

int main(int argc, char* argv[]) {
    Application app(800, 600);

    try {
        app.Startup();
        app.Run();
        app.Shutdown();
    }
    catch (const std::exception& exception) {
#ifdef _DEBUG
        SDL_Log("%s", exception.what());
#else
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Run-Time Error", exception.what(), nullptr);
#endif
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
