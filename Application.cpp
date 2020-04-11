#include <chrono>
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

static const int MAX_FRAMES_IN_FLIGHT = 2;

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
        CreateSyncObjects();
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
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(render_device_.device_, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(render_device_.device_, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(render_device_.device_, inFlightFences[i], nullptr);
        }
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
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;

    IndexedPrimitive primitive_{};

    static void CreateSurface(void* window, VkInstance& instance, VkSurfaceKHR& surface) {
        if (!SDL_Vulkan_CreateSurface((SDL_Window*)window, instance, &surface)) {
            throw std::runtime_error("failed to create window surface");
        }
    }

    void RecreateSwapchain() {
        vkDeviceWaitIdle(render_device_.device_);
        render_pipeline_.Reset();
        render_swapchain_.Rebuild(window_width_, window_height_);
        render_pipeline_.Rebuild(render_swapchain_);
    }

    void CreateSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(render_device_.image_count_, VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(render_device_.device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(render_device_.device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(render_device_.device_, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame");
            }
        }
    }

    void UpdateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo = {};
        ubo.model = glm::mat4(1.0f);
        ubo.model = glm::rotate(ubo.model, time * glm::radians(60.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.model = glm::rotate(ubo.model, time * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.model = glm::rotate(ubo.model, time * glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        ubo.model = glm::scale(ubo.model, glm::vec3(2.4f, 2.4f, 2.4f));
        ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 4.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), render_swapchain_.swapchain_extent_.width / (float)render_swapchain_.swapchain_extent_.height, 0.1f, 100.0f);
        ubo.proj[1][1] *= -1;

        void* data;
        vkMapMemory(render_device_.device_, render_pipeline_.uniform_buffers_memory_[currentImage], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(render_device_.device_, render_pipeline_.uniform_buffers_memory_[currentImage]);
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
        vkWaitForFences(render_device_.device_, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(render_device_.device_, render_swapchain_.swapchain_, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            RecreateSwapchain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image");
        }

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(render_pipeline_.command_buffers_[imageIndex], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer");
        }

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = render_pipeline_.render_pass_;
        renderPassInfo.framebuffer = render_pipeline_.framebuffers_[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = render_swapchain_.swapchain_extent_;

        std::array<VkClearValue, 2> clearValues = {};
        clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(render_pipeline_.command_buffers_[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(render_pipeline_.command_buffers_[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline_.graphics_pipeline_);

        VkBuffer vertexBuffers[] = {primitive_.vertex_buffer_};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(render_pipeline_.command_buffers_[imageIndex], 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(render_pipeline_.command_buffers_[imageIndex], primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(render_pipeline_.command_buffers_[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline_.pipeline_layout_, 0, 1, &render_pipeline_.descriptor_sets_[imageIndex], 0, nullptr);

        vkCmdDrawIndexed(render_pipeline_.command_buffers_[imageIndex], primitive_.index_count_, 1, 0, 0, 0);

        vkCmdEndRenderPass(render_pipeline_.command_buffers_[imageIndex]);

        if (vkEndCommandBuffer(render_pipeline_.command_buffers_[imageIndex]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        UpdateUniformBuffer(imageIndex);

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(render_device_.device_, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &render_pipeline_.command_buffers_[imageIndex];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(render_device_.device_, 1, &inFlightFences[currentFrame]);

        if (vkQueueSubmit(render_device_.graphics_queue_, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer");
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = {render_swapchain_.swapchain_};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(render_device_.present_queue_, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            RecreateSwapchain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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
