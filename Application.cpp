#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

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

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "RenderEngine.h"
#include "RenderPipeline.h"
#include "Geometry.h"
#include "Geometry_Color.h"
#include "Geometry_Texture.h"

#define MODE                        0       // 0 = cubes, 1 = model, 2 = sprites

#if MODE == 1
static const char* MODEL_PATH = "models/chalet.obj";
static const char* TEXTURE_PATH = "textures/chalet.jpg";
#endif

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
        render_engine_.Initialize(window_width_, window_height_, required_extensions, CreateSurface, window_);

#if MODE == 1
        {
            std::vector<unsigned char> byte_code{};
            byte_code = ReadFile("shaders/texture/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = ReadFile("shaders/texture/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            render_pipeline_.Initialize(vertex_shader_module, fragment_shader_module, sizeof(UniformBufferObject), 1);
        }

        int texture_width, texture_height, texture_channels;
        stbi_uc* pixels = stbi_load(TEXTURE_PATH, &texture_width, &texture_height, &texture_channels, STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error("failed to load texture image");
        }

        render_engine_.CreateTexture(pixels, texture_width, texture_height, texture_);

        stbi_image_free(pixels);

        for (uint32_t image_index = 0; image_index < render_engine_.image_count_; image_index++) {
            render_pipeline_.UpdateDescriptorSet(image_index, texture_.texture_image_view_, texture_.texture_sampler_);
        }

        std::vector<Vertex_Texture> vertices;
        std::vector<uint32_t> indices;
        LoadModel(MODEL_PATH, vertices, indices);
        render_engine_.CreateIndexedPrimitive<Vertex_Texture, uint32_t>(vertices, indices, primitive_);
#else
        {
            std::vector<unsigned char> byte_code{};
            byte_code = ReadFile("shaders/color/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = ReadFile("shaders/color/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            render_pipeline_color_.Initialize(vertex_shader_module, fragment_shader_module, sizeof(UniformBufferObject), 0);
        }

        {
            std::vector<unsigned char> byte_code{};
            byte_code = ReadFile("shaders/notexture/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = ReadFile("shaders/notexture/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            render_pipeline_texture_.Initialize(vertex_shader_module, fragment_shader_module, sizeof(UniformBufferObject), 0);
        }

        {
            std::vector<glm::vec3> vertices{};
            std::vector<std::vector<uint32_t>> faces{};
            Geometry::CreateCube(vertices, faces);

            std::vector<glm::vec3> colors = {
                {1.0, 1.0, 1.0},
                {1.0, 0.0, 0.0},
                {0.0, 1.0, 0.0},
                {0.0, 0.0, 1.0},
                {1.0, 1.0, 0.0},
                {1.0, 0.0, 1.0}
            };

            Geometry_Color geometry_color{};
            geometry_color.AddFaces(vertices, faces, colors);
            render_engine_.CreateIndexedPrimitive<Vertex_Color, uint32_t>(geometry_color.vertices, geometry_color.indices, color_primitive_);

            std::vector<glm::vec2> texture_coordinates = {
                {0, 0},
                {1, 0},
                {1, 1},
                {0, 1}
            };

            Geometry_Texture geometry_texture{};
            geometry_texture.AddFaces(vertices, faces, texture_coordinates);
            render_engine_.CreateIndexedPrimitive<Vertex_Texture, uint32_t>(geometry_texture.vertices, geometry_texture.indices, texture_primitive_);
        }
#endif

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
        vkDeviceWaitIdle(render_engine_.device_);
#if MODE == 1
        render_pipeline_.Destroy();
        render_engine_.DestroyIndexedPrimitive(primitive_);
        render_engine_.DestroyTexture(texture_);
#else
        render_pipeline_texture_.Destroy();
        render_pipeline_color_.Destroy();
        render_engine_.DestroyIndexedPrimitive(texture_primitive_);
        render_engine_.DestroyIndexedPrimitive(color_primitive_);
#endif
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(render_engine_.device_, render_finished_semaphores_[i], nullptr);
            vkDestroySemaphore(render_engine_.device_, image_available_semaphores_[i], nullptr);
            vkDestroyFence(render_engine_.device_, in_flight_fences_[i], nullptr);
        }
        render_engine_.Destroy();
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    SDL_Window* window_ = NULL;
    int window_width_;
    int window_height_;
    bool window_minimized_ = false;
    bool window_closed_ = false;

#if MODE == 1
    RenderEngine render_engine_{1};
    RenderPipeline render_pipeline_{render_engine_, Vertex_Texture::getBindingDescription(), Vertex_Texture::getAttributeDescriptions(), 0};
#else
    RenderEngine render_engine_{2};
    RenderPipeline render_pipeline_color_{render_engine_, Vertex_Color::getBindingDescription(), Vertex_Color::getAttributeDescriptions(), 0};
    RenderPipeline render_pipeline_texture_{render_engine_, Vertex_Texture::getBindingDescription(), Vertex_Texture::getAttributeDescriptions(), 1};
#endif

    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    UniformBufferObject uniform_buffer_{};
    UniformBufferObject uniform_buffer_1_{};
    UniformBufferObject uniform_buffer_2_{};

    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    std::vector<VkFence> images_in_flight_;
    size_t current_frame_ = 0;

#if MODE == 1
    IndexedPrimitive primitive_{};
    TextureSampler texture_;
#else
    IndexedPrimitive color_primitive_{};
    IndexedPrimitive texture_primitive_{};
#endif

    static void CreateSurface(void* window, VkInstance& instance, VkSurfaceKHR& surface) {
        if (!SDL_Vulkan_CreateSurface((SDL_Window*)window, instance, &surface)) {
            throw std::runtime_error("failed to create window surface");
        }
    }

    void RecreateSwapchain() {
        vkDeviceWaitIdle(render_engine_.device_);
#if MODE == 1
        render_pipeline_.Reset();
#else
        render_pipeline_texture_.Reset();
        render_pipeline_color_.Reset();
#endif
        render_engine_.RebuildSwapchain(window_width_, window_height_);
#if MODE == 1
        render_pipeline_.Rebuild();
        for (uint32_t image_index = 0; image_index < render_engine_.image_count_; image_index++) {
            render_pipeline_.UpdateDescriptorSet(image_index, texture_.texture_image_view_, texture_.texture_sampler_);
        }
#else
        render_pipeline_color_.Rebuild();
        render_pipeline_texture_.Rebuild();
#endif
    }

    void CreateSyncObjects() {
        image_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
        render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
        in_flight_fences_.resize(MAX_FRAMES_IN_FLIGHT);
        images_in_flight_.resize(render_engine_.image_count_, VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(render_engine_.device_, &semaphore_info, nullptr, &image_available_semaphores_[i]) != VK_SUCCESS ||
                vkCreateSemaphore(render_engine_.device_, &semaphore_info, nullptr, &render_finished_semaphores_[i]) != VK_SUCCESS ||
                vkCreateFence(render_engine_.device_, &fence_info, nullptr, &in_flight_fences_[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame");
            }
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
        static auto start_time = std::chrono::high_resolution_clock::now();
        auto current_time = std::chrono::high_resolution_clock::now();
        float total_time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

#if MODE == 1
        uniform_buffer_.model = glm::scale(glm::rotate(glm::mat4(1.0f), total_time * glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec3(1.2f, 1.2f, 1.2f));
        uniform_buffer_.view = glm::lookAt(glm::vec3(2.0f, 2.4f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        uniform_buffer_.proj = glm::perspective(glm::radians(45.0f), render_engine_.swapchain_extent_.width / (float)render_engine_.swapchain_extent_.height, 0.1f, 10.0f);
        uniform_buffer_.proj[1][1] *= -1;
#else
        float offset_1 = std::sin(total_time);
        float offset_2 = std::cos(total_time);

        uniform_buffer_1_.model = glm::mat4(1.0f);
        uniform_buffer_1_.model = glm::translate(uniform_buffer_1_.model, glm::vec3(-1.0f, 0.0f, offset_1));
        uniform_buffer_1_.model = glm::rotate(uniform_buffer_1_.model, total_time * glm::radians(60.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        uniform_buffer_1_.model = glm::rotate(uniform_buffer_1_.model, total_time * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uniform_buffer_1_.model = glm::rotate(uniform_buffer_1_.model, total_time * glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        uniform_buffer_1_.model = glm::scale(uniform_buffer_1_.model, glm::vec3(1.5f, 1.5f, 1.5f));
        uniform_buffer_1_.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 4.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uniform_buffer_1_.proj = glm::perspective(glm::radians(45.0f), render_engine_.swapchain_extent_.width / (float)render_engine_.swapchain_extent_.height, 0.1f, 100.0f);
        uniform_buffer_1_.proj[1][1] *= -1;

        uniform_buffer_2_.model = glm::mat4(1.0f);
        uniform_buffer_2_.model = glm::translate(uniform_buffer_2_.model, glm::vec3(1.0f, 0.0f, offset_2));
        uniform_buffer_2_.model = glm::rotate(uniform_buffer_2_.model, total_time * glm::radians(60.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        uniform_buffer_2_.model = glm::rotate(uniform_buffer_2_.model, total_time * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uniform_buffer_2_.model = glm::rotate(uniform_buffer_2_.model, total_time * glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        uniform_buffer_2_.model = glm::scale(uniform_buffer_2_.model, glm::vec3(1.5f, 1.5f, 1.5f));
        uniform_buffer_2_.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 4.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uniform_buffer_2_.proj = glm::perspective(glm::radians(45.0f), render_engine_.swapchain_extent_.width / (float)render_engine_.swapchain_extent_.height, 0.1f, 100.0f);
        uniform_buffer_2_.proj[1][1] *= -1;
#endif
    }

    void Render() {
        vkWaitForFences(render_engine_.device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);

        uint32_t image_index;
        VkResult result = vkAcquireNextImageKHR(render_engine_.device_, render_engine_.swapchain_, UINT64_MAX, image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            RecreateSwapchain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image");
        }

        //auto beg_time = std::chrono::high_resolution_clock::now();

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(render_engine_.command_buffers_[image_index], &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer");
        }

        VkRenderPassBeginInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_engine_.render_pass_;
        render_pass_info.framebuffer = render_engine_.framebuffers_[image_index];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = render_engine_.swapchain_extent_;

        std::array<VkClearValue, 2> clear_values = {};
        clear_values[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
        clear_values[1].depthStencil = {1.0f, 0};

        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(render_engine_.command_buffers_[image_index], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

#if MODE == 1
        vkCmdBindPipeline(render_engine_.command_buffers_[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline_.graphics_pipeline_);
        VkBuffer vertex_buffers_1[] = {primitive_.vertex_buffer_};
        VkDeviceSize offsets_1[] = {0};
        vkCmdBindVertexBuffers(render_engine_.command_buffers_[image_index], 0, 1, vertex_buffers_1, offsets_1);
        vkCmdBindIndexBuffer(render_engine_.command_buffers_[image_index], primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(render_engine_.command_buffers_[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline_.pipeline_layout_, 0, 1, &render_pipeline_.descriptor_sets_[image_index], 0, nullptr);
        vkCmdDrawIndexed(render_engine_.command_buffers_[image_index], primitive_.index_count_, 1, 0, 0, 0);
#else
        vkCmdBindPipeline(render_engine_.command_buffers_[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline_color_.graphics_pipeline_);
        VkBuffer vertex_buffers_1[] = {color_primitive_.vertex_buffer_};
        VkDeviceSize offsets_1[] = {0};
        vkCmdBindVertexBuffers(render_engine_.command_buffers_[image_index], 0, 1, vertex_buffers_1, offsets_1);
        vkCmdBindIndexBuffer(render_engine_.command_buffers_[image_index], color_primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(render_engine_.command_buffers_[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline_color_.pipeline_layout_, 0, 1, &render_pipeline_color_.descriptor_sets_[image_index], 0, nullptr);
        vkCmdDrawIndexed(render_engine_.command_buffers_[image_index], color_primitive_.index_count_, 1, 0, 0, 0);

        vkCmdNextSubpass(render_engine_.command_buffers_[image_index], VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(render_engine_.command_buffers_[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline_texture_.graphics_pipeline_);
        VkBuffer vertex_buffers_2[] = {texture_primitive_.vertex_buffer_};
        VkDeviceSize offsets_2[] = {0};
        vkCmdBindVertexBuffers(render_engine_.command_buffers_[image_index], 0, 1, vertex_buffers_2, offsets_2);
        vkCmdBindIndexBuffer(render_engine_.command_buffers_[image_index], texture_primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(render_engine_.command_buffers_[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline_texture_.pipeline_layout_, 0, 1, &render_pipeline_texture_.descriptor_sets_[image_index], 0, nullptr);
        vkCmdDrawIndexed(render_engine_.command_buffers_[image_index], texture_primitive_.index_count_, 1, 0, 0, 0);
#endif

        vkCmdEndRenderPass(render_engine_.command_buffers_[image_index]);

        if (vkEndCommandBuffer(render_engine_.command_buffers_[image_index]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

#if MODE == 1
        void* data;

        vkMapMemory(render_engine_.device_, render_pipeline_.uniform_buffers_memory_[image_index], 0, sizeof(uniform_buffer_), 0, &data);
        memcpy(data, &uniform_buffer_, sizeof(uniform_buffer_));
        vkUnmapMemory(render_engine_.device_, render_pipeline_.uniform_buffers_memory_[image_index]);
#else
        void* data;

        vkMapMemory(render_engine_.device_, render_pipeline_color_.uniform_buffers_memory_[image_index], 0, sizeof(uniform_buffer_1_), 0, &data);
        memcpy(data, &uniform_buffer_1_, sizeof(uniform_buffer_1_));
        vkUnmapMemory(render_engine_.device_, render_pipeline_color_.uniform_buffers_memory_[image_index]);

        vkMapMemory(render_engine_.device_, render_pipeline_texture_.uniform_buffers_memory_[image_index], 0, sizeof(uniform_buffer_2_), 0, &data);
        memcpy(data, &uniform_buffer_2_, sizeof(uniform_buffer_2_));
        vkUnmapMemory(render_engine_.device_, render_pipeline_texture_.uniform_buffers_memory_[image_index]);
#endif

        //auto end_time = std::chrono::high_resolution_clock::now();
        //auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - beg_time).count();
        //RenderDevice::Log("%lld", duration);

        if (images_in_flight_[image_index] != VK_NULL_HANDLE) {
            vkWaitForFences(render_engine_.device_, 1, &images_in_flight_[image_index], VK_TRUE, UINT64_MAX);
        }
        images_in_flight_[image_index] = in_flight_fences_[current_frame_];

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = {image_available_semaphores_[current_frame_]};
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &render_engine_.command_buffers_[image_index];

        VkSemaphore signal_semaphores[] = {render_finished_semaphores_[current_frame_]};
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        vkResetFences(render_engine_.device_, 1, &in_flight_fences_[current_frame_]);

        if (vkQueueSubmit(render_engine_.graphics_queue_, 1, &submit_info, in_flight_fences_[current_frame_]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer");
        }

        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;
        VkSwapchainKHR swap_chains[] = {render_engine_.swapchain_};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swap_chains;
        present_info.pImageIndices = &image_index;

        result = vkQueuePresentKHR(render_engine_.present_queue_, &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            RecreateSwapchain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image");
        }

        current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void LoadModel(const char* fileName, std::vector<Vertex_Texture>& vertices, std::vector<uint32_t>& indices) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fileName)) {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<Vertex_Texture, uint32_t, Vertex_Texture_Hash> unique_vertices = {};

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex_Texture vertex = {};

                vertex.pos = {
                    attrib.vertices[static_cast<size_t>(index.vertex_index) * 3 + 0],
                    attrib.vertices[static_cast<size_t>(index.vertex_index) * 3 + 1],
                    attrib.vertices[static_cast<size_t>(index.vertex_index) * 3 + 2]
                };

                vertex.texCoord = {
                    attrib.texcoords[static_cast<size_t>(index.texcoord_index) * 2 + 0],
                    1.0f - attrib.texcoords[static_cast<size_t>(index.texcoord_index) * 2 + 1]
                };

                if (unique_vertices.count(vertex) == 0) {
                    unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(unique_vertices[vertex]);
            }
        }
    }

    std::vector<unsigned char> ReadFile(const std::string& file_name) {
        std::ifstream file(file_name, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error(std::string{"failed to open file "}+file_name);
        }

        size_t file_size = (size_t)file.tellg();
        std::vector<unsigned char> buffer(file_size);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), file_size);

        file.close();

        return buffer;
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
