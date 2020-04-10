#include <fstream>
#include <stdexcept>
#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#ifdef _DEBUG
#pragma comment(lib, "SDL2maind.lib")
#else
#pragma comment(lib, "SDL2main.lib")
#endif

#include "RenderDevice.h"
#include "RenderSwapchain.h"
#include "RenderPipeline.h"
#include "Geometry.h"
#include "Geometry_Color.h"

class Application {
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

        SDL_Vulkan_GetDrawableSize(window_, &window_width_, &window_height_);
        uint32_t required_extension_count = 0;
        SDL_Vulkan_GetInstanceExtensions(window_, &required_extension_count, nullptr);
        std::vector<const char*> required_extensions(required_extension_count);
        SDL_Vulkan_GetInstanceExtensions(window_, &required_extension_count, required_extensions.data());
        render_device_.Initialize(window_width_, window_height_, required_extensions, CreateSurface, window_);
        render_swapchain_.Initialize(window_width_, window_height_);
        std::vector<unsigned char> byte_code = ReadFile("shaders/color/vert.spv");
        VkShaderModule vertex_shader_module = render_device_.CreateShaderModule(byte_code.data(), byte_code.size());
        byte_code = ReadFile("shaders/color/frag.spv");
        VkShaderModule fragment_shader_module = render_device_.CreateShaderModule(byte_code.data(), byte_code.size());
        render_pipeline_.Initialize(vertex_shader_module, fragment_shader_module, sizeof(UniformBufferObject), 0, render_swapchain_);

        std::vector<glm::vec3> vertices{};
        std::vector<std::vector<uint32_t>> faces{};
        Geometry::CreateCube(vertices, faces);

        std::vector<glm::vec3> colors = {
            {1.0, 1.0, 1.0},
            {1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0},
            {0.0, 0.0, 1.0},
            {1.0, 1.0, 0.0},
            {1.0, 0.0, 1.0},
        };

        Geometry_Color geometry{};
        geometry.AddFaces(vertices, faces, colors);
        render_device_.CreateIndexedPrimitive<Vertex_Color, uint32_t>(geometry.vertices, geometry.indices, primitive_);
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
        SDL_Log("application shutdown");
        vkDeviceWaitIdle(render_device_.device_);
        render_pipeline_.Destroy();
        render_swapchain_.Destroy();
        render_device_.DestroyIndexedPrimitive(primitive_);
        render_device_.Destroy();
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    SDL_Window* window_ = NULL;
    int window_width_;
    int window_height_;
    bool window_minimized_ = false;
    bool window_closed_ = false;
    RenderDevice render_device_{};
    RenderSwapchain render_swapchain_{render_device_};
    RenderPipeline render_pipeline_{render_device_, Vertex_Color::getBindingDescription(), Vertex_Color::getAttributeDescriptions()};
    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    IndexedPrimitive primitive_{};

    static void CreateSurface(void* window, VkInstance& instance, VkSurfaceKHR& surface) {
        if (!SDL_Vulkan_CreateSurface((SDL_Window*)window, instance, &surface)) {
            throw std::runtime_error("failed to create window surface");
        }
    }

    void RecreateSwapchain() {
        vkDeviceWaitIdle(render_device_.device_);
        render_swapchain_.Rebuild(window_width_, window_height_);
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
                    SDL_Vulkan_GetDrawableSize(window_, &window_width_, &window_height_);
                    RecreateSwapchain();
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

    std::vector<unsigned char> ReadFile(const std::string& file_name) {
        std::ifstream file(file_name, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error(std::string{"failed to open file "} + file_name);
        }

        size_t file_size = (size_t)file.tellg();
        std::vector<unsigned char> buffer(file_size);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), file_size);

        file.close();

        return buffer;
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
