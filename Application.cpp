#include <chrono>
#include <stdexcept>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#ifdef _DEBUG
#pragma comment(lib, "SDL2maind.lib")
#else
#pragma comment(lib, "SDL2main.lib")
#endif

#include "Math.h"
#include "Scene.h"
#include "RenderEngine.h"
#include "InterfaceScene.h"
#include "CubeScene.h"
#include "FontScene.h"
#include "ModelScene.h"
#include "SpriteScene.h"

class Application : RenderApplication {
public:
    Application(int window_width, int window_height) : window_width_(window_width), window_height_(window_height) {}

    void Startup() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            throw std::runtime_error(SDL_GetError());
        }

        window_ = SDL_CreateWindow("Vulkan Testing", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width_, window_height_, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_VULKAN);

        if (window_ == nullptr) {
            throw std::runtime_error(SDL_GetError());
        }

        scenes_.push_back(new InterfaceScene{render_engine_, window_});
        scenes_.push_back(new CubeScene{render_engine_});
        scenes_.push_back(new FontScene{render_engine_});
        scenes_.push_back(new ModelScene{render_engine_});
        scenes_.push_back(new SpriteScene{render_engine_});

        render_engine_.Initialize(this);

        scene_ = scenes_[scene_index_];
        scene_->OnEntry();
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
        for (auto scene : scenes_) {
            scene->OnExit();
            scene->OnQuit();
        }
        render_engine_.Destroy();
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    std::vector<Scene*> scenes_{};
    uint32_t scene_index_ = 0;
    Scene* scene_;
    SDL_Window* window_ = nullptr;
    int window_width_;
    int window_height_;

    bool window_minimized_ = false;
    bool window_closed_ = false;

    std::array<bool, SDL_NUM_SCANCODES> key_state_{};
    bool mouse_capture_ = false;

    RenderEngine render_engine_{};

    void ProcessInput() {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            scene_->EventHandler(&event);
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
                        SDL_GetRelativeMouseState(nullptr, nullptr);
                    }
                    break;
                case SDL_SCANCODE_TAB:
                    scene_->OnExit();
                    scene_index_ = (scene_index_ + 1) % scenes_.size();
                    scene_ = scenes_[scene_index_];
                    scene_->OnEntry();
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
                    render_engine_.RebuildSwapchain();
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
        scene_->Update(key_state_, mouse_capture_, mouse_x, mouse_y);
    }

    void Render() {
        scene_->Render();
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
};

int main(int argc, char* argv[]) {
    Application app(800, 600);

    try {
        app.Startup();
        app.Run();
        app.Shutdown();
    }
    catch (const std::exception& exception) {
#ifdef _DEBUG
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", exception.what());
#else
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Run-Time Error", exception.what(), nullptr);
#endif
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
