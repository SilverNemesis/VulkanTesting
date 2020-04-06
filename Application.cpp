#include "RenderDevice.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#ifdef _DEBUG
#pragma comment(lib, "SDL2maind.lib")
#else
#pragma comment(lib, "SDL2main.lib")
#endif

#include <stdexcept>

class Application {
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

        SDL_GetWindowSize(window_, &window_width_, &window_height_);

        uint32_t required_extension_count = 0;
        SDL_Vulkan_GetInstanceExtensions(window_, &required_extension_count, nullptr);
        std::vector<const char*> required_extensions(required_extension_count);
        SDL_Vulkan_GetInstanceExtensions(window_, &required_extension_count, required_extensions.data());
        render_device_.Initialize(window_width_, window_height_, required_extensions, CreateSurface, window_);
    }

    void Run() {
        while (!window_closed_) {
            ProcessInput();

            Update();

            if (!window_minimized_) {
                Render();
            }
        }
    }

    void Shutdown() {
        render_device_.Destroy();
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    SDL_Window* window_ = NULL;
    int window_width_;
    int window_height_;
    bool window_resized_ = false;
    bool window_minimized_ = false;
    bool window_closed_ = false;
    RenderDevice render_device_;

    static void CreateSurface(void* window, VkInstance& instance, VkSurfaceKHR& surface) {
        if (!SDL_Vulkan_CreateSurface((SDL_Window*)window, instance, &surface)) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void ProcessInput() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                window_closed_ = true;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_ESCAPE:
                    window_closed_ = true;
                    break;
                }
                break;
            case SDL_KEYUP:
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                case SDL_WINDOWEVENT_RESIZED:
                    window_resized_ = true;
                    SDL_GetWindowSize(window_, &window_width_, &window_height_);
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
    }

    void Render() {
    }
};

void (*(RenderDevice::Log))(const char* fmt, ...) = SDL_Log;

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
