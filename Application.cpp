#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
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

#include <ft2build.h>
#include FT_FREETYPE_H

#include "RenderEngine.h"
#include "RenderPipeline.h"
#include "Geometry.h"
#include "Geometry_Color.h"
#include "Geometry_Texture.h"
#include "Geometry_2D.h"
#include "Geometry_Text.h"

#define MODE                        0       // 0 = cubes, 1 = model, 2 = sprites, 3 = text
#define DIAG                        0       // 0 = none, 1 = timing

#if MODE == 1
static const char* MODEL_PATH = "models/chalet.obj";
static const char* TEXTURE_PATH = "textures/chalet.jpg";
#elif MODE == 2
static const char* SPRITE_PATH = "textures/texture.jpg";
#endif

static const int MAX_FRAMES_IN_FLIGHT = 2;

struct Font {
    float size;

    TextureSampler font_texture_;

    struct Character {
        uint16_t x;
        uint16_t y;
        uint8_t a;
        uint8_t w;
        uint8_t h;
        uint8_t dx;
        uint8_t dy;
    };

    std::map<unsigned char, Character> characters_;
};

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

        render_engine_.Initialize(this);

#if DIAG == 1
        VkQueryPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        createInfo.queryCount = 2;

        if (vkCreateQueryPool(render_engine_.device_, &createInfo, nullptr, &query_pool_) != VK_SUCCESS) {
            throw std::runtime_error("unable to create query pool");
        }
#endif

#if MODE == 1
        {
            std::vector<unsigned char> byte_code{};
            byte_code = ReadFile("shaders/texture/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = ReadFile("shaders/texture/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            render_pipeline_.Initialize(vertex_shader_module, fragment_shader_module, sizeof(UniformBufferObject), 0, 1, 1, false);
        }

        LoadTexture(TEXTURE_PATH, texture_);

        render_pipeline_.UpdateDescriptorSets(0, {texture_});

        std::vector<Vertex_Texture> vertices;
        std::vector<uint32_t> indices;
        LoadModel(MODEL_PATH, vertices, indices);
        render_engine_.CreateIndexedPrimitive<Vertex_Texture, uint32_t>(vertices, indices, primitive_);
#elif MODE == 2
        {
            std::vector<unsigned char> byte_code{};
            byte_code = ReadFile("shaders/ortho2d/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = ReadFile("shaders/ortho2d/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            render_pipeline_sprite_.Initialize(vertex_shader_module, fragment_shader_module, 0, 0, 1, 1, false);
        }

        LoadTexture(SPRITE_PATH, sprite_texture_);

        render_pipeline_sprite_.UpdateDescriptorSets(0, {sprite_texture_});

        {
            std::vector<glm::vec2> vertices{};
            std::vector<std::vector<uint32_t>> faces{};
            Geometry2D::CreateSquare(0.35f, vertices, faces);

            std::vector<glm::vec2> texture_coordinates = {
                {0, 0},
                {1, 0},
                {1, 1},
                {0, 1}
            };

            Geometry_2D geometry_sprite{};
            geometry_sprite.AddFaces(vertices, faces, texture_coordinates);
            render_engine_.CreateIndexedPrimitive<Vertex_2D, uint32_t>(geometry_sprite.vertices, geometry_sprite.indices, sprite_primitive_);
        }
#elif MODE == 3
        {
            std::vector<unsigned char> byte_code{};
            byte_code = ReadFile("shaders/text/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = ReadFile("shaders/text/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            render_pipeline_text_.Initialize(vertex_shader_module, fragment_shader_module, 0, 0, 1, 2, true);
        }

        LoadFont("fonts/Inconsolata/Inconsolata-Regular.ttf", 36, font_1_);
        LoadFont("fonts/katakana/katakana.ttf", 48, font_2_);

        render_pipeline_text_.UpdateDescriptorSets(0, {font_1_.font_texture_});
        render_pipeline_text_.UpdateDescriptorSets(1, {font_2_.font_texture_});

        std::vector<glm::vec2> texture_coordinates = {
            {0, 1},
            {1, 1},
            {1, 0},
            {0, 0}
        };

        std::vector<glm::vec2> vertices{};
        std::vector<std::vector<uint32_t>> faces{};

        const char* text = "Hello world!";

        Geometry_Text geometry_text_1{};
        RenderText(font_1_, text, -GetTextLength(font_1_, text) - 10, 0, {1.0, 1.0, 1.0}, geometry_text_1);
        render_engine_.CreateIndexedPrimitive<Vertex_Text, uint32_t>(geometry_text_1.vertices, geometry_text_1.indices, font_primitive_1_);

        Geometry_Text geometry_text_2{};
        RenderText(font_2_, text, 10, 0, {1.0, 1.0, 1.0}, geometry_text_2);
        render_engine_.CreateIndexedPrimitive<Vertex_Text, uint32_t>(geometry_text_2.vertices, geometry_text_2.indices, font_primitive_2_);
#else
        {
            std::vector<unsigned char> byte_code{};
            byte_code = ReadFile("shaders/color/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = ReadFile("shaders/color/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            render_pipeline_color_.Initialize(vertex_shader_module, fragment_shader_module, sizeof(UniformBufferObject), 0, 0, 1, false);
        }

        {
            std::vector<unsigned char> byte_code{};
            byte_code = ReadFile("shaders/notexture/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = ReadFile("shaders/notexture/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            render_pipeline_texture_.Initialize(vertex_shader_module, fragment_shader_module, sizeof(UniformBufferObject), 0, 0, 1, false);
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
        vkDeviceWaitIdle(render_engine_.device_);
#if DIAG == 1
        vkDestroyQueryPool(render_engine_.device_, query_pool_, NULL);
#endif
#if MODE == 1
        render_pipeline_.Destroy();
        render_engine_.DestroyIndexedPrimitive(primitive_);
        render_engine_.DestroyTexture(texture_);
#elif MODE == 2
        render_pipeline_sprite_.Destroy();
        render_engine_.DestroyIndexedPrimitive(sprite_primitive_);
        render_engine_.DestroyTexture(sprite_texture_);
#elif MODE == 3
        render_pipeline_text_.Destroy();
        render_engine_.DestroyIndexedPrimitive(font_primitive_1_);
        render_engine_.DestroyIndexedPrimitive(font_primitive_2_);
        DestroyFont(font_1_);
        DestroyFont(font_2_);
#else
        render_pipeline_texture_.Destroy();
        render_pipeline_color_.Destroy();
        render_engine_.DestroyIndexedPrimitive(texture_primitive_);
        render_engine_.DestroyIndexedPrimitive(color_primitive_);
#endif
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
    std::array<bool, SDL_NUM_SCANCODES> key_state_{};
    bool mouse_capture_ = false;
    glm::vec3 camera_position_{0.0f, 0.5f, -3.0f};
    glm::vec3 camera_forward_{0.0f, 0.0f, 1.0f};
    glm::vec3 camera_right_{1.0f, 0.0f, 0.0f};
    glm::vec3 camera_up_{0.0f, -1.0f, 0.0f};
    float camera_yaw_ = 0.0;
    float camera_pitch_ = 0.0;

#if DIAG == 1
    VkQueryPool query_pool_{};
#endif

#if MODE == 1
    RenderEngine render_engine_{1, MAX_FRAMES_IN_FLIGHT};
    RenderPipeline render_pipeline_{render_engine_, Vertex_Texture::getBindingDescription(), Vertex_Texture::getAttributeDescriptions(), 0};
#elif MODE == 2
    RenderEngine render_engine_{1, MAX_FRAMES_IN_FLIGHT};
    RenderPipeline render_pipeline_sprite_{render_engine_, Vertex_2D::getBindingDescription(), Vertex_2D::getAttributeDescriptions(), 0};
#elif MODE == 3
    RenderEngine render_engine_{1, MAX_FRAMES_IN_FLIGHT};
    RenderPipeline render_pipeline_text_{render_engine_, Vertex_Text::getBindingDescription(), Vertex_Text::getAttributeDescriptions(), 0};
#else
    RenderEngine render_engine_{2, MAX_FRAMES_IN_FLIGHT};
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

    Font font_1_{};
    Font font_2_{};

#if MODE == 1
    IndexedPrimitive primitive_{};
    TextureSampler texture_;
#elif MODE == 2
    IndexedPrimitive sprite_primitive_{};
    TextureSampler sprite_texture_;
#elif MODE == 3
    IndexedPrimitive font_primitive_1_{};
    IndexedPrimitive font_primitive_2_{};
#else
    IndexedPrimitive color_primitive_{};
    IndexedPrimitive texture_primitive_{};
#endif

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
        glm::mat4 projection_matrix = glm::perspective(glm::radians(45.0f), render_engine_.swapchain_extent_.width / (float)render_engine_.swapchain_extent_.height, 0.1f, 100.0f);

        if (mouse_capture_) {
            int mouse_x;
            int mouse_y;
            SDL_GetRelativeMouseState(&mouse_x, &mouse_y);
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

        static float total_time;
        total_time += 4.0f / 1000.0f;

#if MODE == 1
        uniform_buffer_.model = glm::mat4(1.0f);
        uniform_buffer_.model = glm::translate(uniform_buffer_.model, glm::vec3(0.0f, 0.0f, 0.0f));
        uniform_buffer_.model = glm::rotate(uniform_buffer_.model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        uniform_buffer_.model = glm::rotate(uniform_buffer_.model, total_time * glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        uniform_buffer_.view = view_matrix;
        uniform_buffer_.proj = projection_matrix;
#elif MODE == 2
#elif MODE == 3
#else
        float offset_1 = std::sin(total_time);
        float offset_2 = std::cos(total_time);

        uniform_buffer_1_.model = glm::mat4(1.0f);
        uniform_buffer_1_.model = glm::translate(uniform_buffer_1_.model, glm::vec3(-1.0f, 0.5f, offset_1 + 1.0f));
        uniform_buffer_1_.model = glm::rotate(uniform_buffer_1_.model, total_time * glm::radians(60.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        uniform_buffer_1_.model = glm::rotate(uniform_buffer_1_.model, total_time * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uniform_buffer_1_.model = glm::rotate(uniform_buffer_1_.model, total_time * glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        uniform_buffer_1_.model = glm::scale(uniform_buffer_1_.model, glm::vec3(1.5f, 1.5f, 1.5f));
        uniform_buffer_1_.view = view_matrix;
        uniform_buffer_1_.proj = projection_matrix;

        uniform_buffer_2_.model = glm::mat4(1.0f);
        uniform_buffer_2_.model = glm::translate(uniform_buffer_2_.model, glm::vec3(1.0f, 0.5f, offset_2 + 1.0f));
        uniform_buffer_2_.model = glm::rotate(uniform_buffer_2_.model, total_time * glm::radians(60.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        uniform_buffer_2_.model = glm::rotate(uniform_buffer_2_.model, total_time * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uniform_buffer_2_.model = glm::rotate(uniform_buffer_2_.model, total_time * glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        uniform_buffer_2_.model = glm::scale(uniform_buffer_2_.model, glm::vec3(1.5f, 1.5f, 1.5f));
        uniform_buffer_2_.view = view_matrix;
        uniform_buffer_2_.proj = projection_matrix;
#endif
    }

    void Render() {
        uint32_t image_index;

        if (!render_engine_.AcquireNextImage(image_index)) {
            return;
        }

#if DIAG == 1
        auto beg_time = std::chrono::high_resolution_clock::now();
#endif

        VkCommandBuffer& command_buffer = render_engine_.command_buffers_[image_index];

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer");
        }

#if DIAG == 1
        vkCmdResetQueryPool(command_buffer, query_pool_, 0, 2);
        vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_, 0);
#endif

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

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

#if MODE == 1
        RenderPipeline& render_pipeline = render_pipeline_;
        const VkDescriptorSet& descriptor_set = render_pipeline.GetDescriptorSet(image_index, 0);

        render_pipeline.UpdateUniformBuffer(image_index, 0, &uniform_buffer_);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.graphics_pipeline_);
        VkBuffer vertex_buffers_1[] = {primitive_.vertex_buffer_};
        VkDeviceSize offsets_1[] = {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_1, offsets_1);
        vkCmdBindIndexBuffer(command_buffer, primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.pipeline_layout_, 0, 1, &descriptor_set, 0, nullptr);
        vkCmdDrawIndexed(command_buffer, primitive_.index_count_, 1, 0, 0, 0);
#elif MODE == 2
        RenderPipeline& render_pipeline = render_pipeline_sprite_;
        const VkDescriptorSet& descriptor_set = render_pipeline.GetDescriptorSet(image_index, 0);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.graphics_pipeline_);
        if (sprite_primitive_.index_count_ > 0) {
            VkBuffer vertex_buffers_1[] = {sprite_primitive_.vertex_buffer_};
            VkDeviceSize offsets_1[] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_1, offsets_1);
            vkCmdBindIndexBuffer(command_buffer, sprite_primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.pipeline_layout_, 0, 1, &descriptor_set, 0, nullptr);
            vkCmdDrawIndexed(command_buffer, sprite_primitive_.index_count_, 1, 0, 0, 0);
        }
#elif MODE == 3
        RenderPipeline& render_pipeline = render_pipeline_text_;
        const VkDescriptorSet& descriptor_set_1 = render_pipeline.GetDescriptorSet(image_index, 0);
        const VkDescriptorSet& descriptor_set_2 = render_pipeline.GetDescriptorSet(image_index, 1);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.graphics_pipeline_);

        VkBuffer vertex_buffers_1[] = {font_primitive_1_.vertex_buffer_};
        VkDeviceSize offsets_1[] = {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_1, offsets_1);
        vkCmdBindIndexBuffer(command_buffer, font_primitive_1_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.pipeline_layout_, 0, 1, &descriptor_set_1, 0, nullptr);
        vkCmdDrawIndexed(command_buffer, font_primitive_1_.index_count_, 1, 0, 0, 0);

        VkBuffer vertex_buffers_2[] = {font_primitive_2_.vertex_buffer_};
        VkDeviceSize offsets_2[] = {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_2, offsets_2);
        vkCmdBindIndexBuffer(command_buffer, font_primitive_2_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.pipeline_layout_, 0, 1, &descriptor_set_2, 0, nullptr);
        vkCmdDrawIndexed(command_buffer, font_primitive_2_.index_count_, 1, 0, 0, 0);
#else

        {
            RenderPipeline& render_pipeline = render_pipeline_color_;
            const VkDescriptorSet& descriptor_set = render_pipeline.GetDescriptorSet(image_index, 0);

            render_pipeline.UpdateUniformBuffer(image_index, 0, &uniform_buffer_1_);

            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.graphics_pipeline_);
            VkBuffer vertex_buffers_1[] = {color_primitive_.vertex_buffer_};
            VkDeviceSize offsets_1[] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_1, offsets_1);
            vkCmdBindIndexBuffer(command_buffer, color_primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.pipeline_layout_, 0, 1, &descriptor_set, 0, nullptr);
            vkCmdDrawIndexed(command_buffer, color_primitive_.index_count_, 1, 0, 0, 0);
        }

        vkCmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

        {
            RenderPipeline& render_pipeline = render_pipeline_texture_;
            const VkDescriptorSet& descriptor_set = render_pipeline.GetDescriptorSet(image_index, 0);

            render_pipeline.UpdateUniformBuffer(image_index, 0, &uniform_buffer_2_);

            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.graphics_pipeline_);
            VkBuffer vertex_buffers_2[] = {texture_primitive_.vertex_buffer_};
            VkDeviceSize offsets_2[] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers_2, offsets_2);
            vkCmdBindIndexBuffer(command_buffer, texture_primitive_.index_buffer_, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline.pipeline_layout_, 0, 1, &descriptor_set, 0, nullptr);
            vkCmdDrawIndexed(command_buffer, texture_primitive_.index_count_, 1, 0, 0, 0);
        }
#endif

#if DIAG == 1
        vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool_, 1);
#endif
        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        render_engine_.SubmitDrawCommands(image_index);

#if DIAG == 1
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - beg_time).count();

        uint64_t time_stamp[2] = {0, 0};
        if (vkGetQueryPoolResults(render_engine_.device_, query_pool_, 0, 2, sizeof(uint64_t) * 2, time_stamp, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS) {
            throw std::runtime_error("unable to get query results");
        }

        SDL_Log("%8lld %8lld", duration, static_cast<long long>((time_stamp[1] - time_stamp[0]) * render_engine_.limits_.timestampPeriod / 1000.0f));
#endif

        render_engine_.PresentImage(image_index);
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
#if MODE == 1
        render_pipeline_.Reset();
#elif MODE == 2
        render_pipeline_sprite_.Reset();
#elif MODE == 3
        render_pipeline_text_.Reset();
#else
        render_pipeline_texture_.Reset();
        render_pipeline_color_.Reset();
#endif
    }

    void PipelineRebuild() {
#if MODE == 1
        render_pipeline_.Rebuild();
        render_pipeline_.UpdateDescriptorSets(0, {texture_});
#elif MODE == 2
        render_pipeline_sprite_.Rebuild();
        render_pipeline_sprite_.UpdateDescriptorSets(0, {sprite_texture_});
#elif MODE == 3
        render_pipeline_text_.Rebuild();
        render_pipeline_text_.UpdateDescriptorSets(0, {font_1_.font_texture_});
        render_pipeline_text_.UpdateDescriptorSets(1, {font_2_.font_texture_});
#else
        render_pipeline_color_.Rebuild();
        render_pipeline_texture_.Rebuild();
#endif
    }

    void LoadTexture(const char* fileName, TextureSampler& texture) {
        int texture_width, texture_height, texture_channels;
        stbi_uc* pixels = stbi_load(fileName, &texture_width, &texture_height, &texture_channels, STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error("failed to load texture image");
        }

        render_engine_.CreateTexture(pixels, texture_width, texture_height, texture);

        stbi_image_free(pixels);
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

    void RenderText(Font& font, const char* text, int x, int y, glm::vec3 color, Geometry_Text& geometry) {
        float xscale = 1.0f / window_width_;
        float yscale = 1.0f / window_height_;

        std::vector<uint32_t> face = {3, 2, 1, 0};

        for (const char* cur = text; *cur != 0; cur++) {
            Font::Character ch = font.characters_[*cur];

            float l = static_cast<float>(x + ch.dx) * xscale;
            float r = static_cast<float>(x + ch.dx + ch.w) * xscale;
            float t = -static_cast<float>(y - ch.h + ch.dy) * yscale;
            float b = -static_cast<float>(y + ch.dy) * yscale;

            std::vector<glm::vec2> vertices = {
                {l, b},
                {r, b},
                {r, t},
                {l, t}
            };

            l = static_cast<float>(ch.x) / font.size;
            r = static_cast<float>(ch.x + ch.w) / font.size;
            t = static_cast<float>(ch.y) / font.size;
            b = static_cast<float>(ch.y + ch.h) / font.size;

            std::vector<glm::vec2> texture_coords = {
                {l, b},
                {r, b},
                {r, t},
                {l, t}
            };

            geometry.AddFace(vertices, face, texture_coords, color);

            x += ch.a;
        }
    }

    int32_t GetTextLength(Font& font, const char* text) {
        int32_t size = 0;
        for (const char* cur = text; *cur != 0; cur++) {
            Font::Character ch = font.characters_[*cur];
            size += ch.a;
        }
        return size;
    }

    void LoadFont(const char* file_name, uint32_t font_size, Font& font) {
        FT_Library ft;
        if (FT_Init_FreeType(&ft)) {
            throw std::runtime_error("unable to initialize font library");
        }

        FT_Face face;
        if (FT_New_Face(ft, file_name, 0, &face)) {
            throw std::runtime_error("unable to load font");
        }

        FT_Set_Pixel_Sizes(face, 0, font_size);

        uint32_t size;

        for (size = 128; size < 4096; size *= 2) {
            uint32_t width = size;
            uint32_t height = size;
            unsigned char* pixels = new unsigned char[width * height];
            memset(pixels, 0, width * height);

            bool resize = false;

            uint32_t x = 0;
            uint32_t y = 0;
            uint32_t my = 0;

            for (int i = 0; i < 256; i++) {
                int index = FT_Get_Char_Index(face, i);

                if (index == 0) {
                    continue;
                }

                if (FT_Load_Glyph(face, index, FT_LOAD_RENDER)) {
                    throw std::runtime_error("failed to load glyph");
                }

                FT_Bitmap b = face->glyph->bitmap;

                if (x + b.width > width) {
                    x = 0;
                    y = my + 1;
                }

                if (y + b.rows > height) {
                    resize = true;
                    continue;
                }

                unsigned char* src = b.buffer;
                unsigned char* dst = pixels + y * width + x;

                for (uint32_t r = 0; r < b.rows; r++) {
                    memcpy(dst, src, b.width);
                    dst += width;
                    src += b.width;
                }

                font.characters_[i] = {(uint16_t)x, (uint16_t)y, (uint8_t)(face->glyph->advance.x >> 6), (uint8_t)b.width, (uint8_t)b.rows, (uint8_t)face->glyph->bitmap_left, (uint8_t)face->glyph->bitmap_top};

                x += b.width + 1;

                if (y + b.rows > my) {
                    my = y + b.rows;
                }
            }

            if (!resize) {
                render_engine_.CreateAlphaTexture(pixels, width, height, font.font_texture_);
                delete[] pixels;
                break;
            }

            font.characters_.clear();
            delete[] pixels;
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        font.size = static_cast<float>(size);
    }

    void DestroyFont(Font& font) {
        render_engine_.DestroyTexture(font.font_texture_);
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
