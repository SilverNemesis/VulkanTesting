#pragma once

#include "Math.h"
#include "Utility.h"
#include "Scene.h"
#include "RenderEngine.h"
#include "Geometry.h"
#include "Geometry_Color.h"
#include "Geometry_Texture.h"

class CubeScene : public Scene {
public:
    CubeScene(RenderEngine& render_engine) : render_engine_(render_engine) {}

    void OnQuit() {
        if (startup_) {
            vkDeviceWaitIdle(render_engine_.device_);

            render_engine_.DestroyGraphicsPipeline(color_graphics_pipeline_);
            render_engine_.DestroyDescriptorSet(color_descriptor_set_);
            render_engine_.DestroyUniformBuffer(color_uniform_buffer_);

            render_engine_.DestroyGraphicsPipeline(texture_graphics_pipeline_);
            render_engine_.DestroyDescriptorSet(texture_descriptor_set_);
            render_engine_.DestroyUniformBuffer(texture_uniform_buffer_);

            render_engine_.DestroyIndexedPrimitive(texture_primitive_);
            render_engine_.DestroyIndexedPrimitive(color_primitive_);
        }
    }

    void OnEntry() {
        if (!startup_) {
            startup_ = true;
            Startup();
        }
    }

    void OnExit() {
    }

    void Update(glm::mat4 view_matrix) {
        camera_.view_matrix = view_matrix;
        camera_.projection_matrix = glm::perspective(glm::radians(45.0f), render_engine_.swapchain_extent_.width / (float)render_engine_.swapchain_extent_.height, 0.1f, 100.0f);

        static float total_time;
        total_time += 4.0f / 1000.0f;

        float offset_1 = std::sin(total_time);
        float offset_2 = std::cos(total_time);

        color_model_.model_matrix = glm::mat4(1.0f);
        color_model_.model_matrix = glm::translate(color_model_.model_matrix, glm::vec3(-1.0f, 0.5f, offset_1 + 1.0f));
        color_model_.model_matrix = glm::rotate(color_model_.model_matrix, total_time * glm::radians(60.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        color_model_.model_matrix = glm::rotate(color_model_.model_matrix, total_time * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        color_model_.model_matrix = glm::rotate(color_model_.model_matrix, total_time * glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        color_model_.model_matrix = glm::scale(color_model_.model_matrix, glm::vec3(1.5f, 1.5f, 1.5f));

        texture_model_.model_matrix = glm::mat4(1.0f);
        texture_model_.model_matrix = glm::translate(texture_model_.model_matrix, glm::vec3(1.0f, 0.5f, offset_2 + 1.0f));
        texture_model_.model_matrix = glm::rotate(texture_model_.model_matrix, total_time * glm::radians(60.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        texture_model_.model_matrix = glm::rotate(texture_model_.model_matrix, total_time * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        texture_model_.model_matrix = glm::rotate(texture_model_.model_matrix, total_time * glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        texture_model_.model_matrix = glm::scale(texture_model_.model_matrix, glm::vec3(1.5f, 1.5f, 1.5f));
    }

    void Render() {
        uint32_t image_index;

        if (!render_engine_.AcquireNextImage(image_index)) {
            return;
        }

        VkCommandBuffer& command_buffer = render_engine_.command_buffers_[image_index];

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer");
        }

        VkRenderPassBeginInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass_->render_pass_;
        render_pass_info.framebuffer = render_pass_->framebuffers_[image_index];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = render_engine_.swapchain_extent_;

        std::array<VkClearValue, 2> clear_values = {};
        clear_values[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
        clear_values[1].depthStencil = {1.0f, 0};

        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, color_graphics_pipeline_->graphics_pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, color_graphics_pipeline_->pipeline_layout, 0, 1, &color_descriptor_set_->descriptor_sets[image_index], 0, nullptr);
        render_engine_.DrawPrimitive(command_buffer, color_primitive_);

        vkCmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->graphics_pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texture_graphics_pipeline_->pipeline_layout, 0, 1, &texture_descriptor_set_->descriptor_sets[image_index], 0, nullptr);
        render_engine_.DrawPrimitive(command_buffer, texture_primitive_);

        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        render_engine_.UpdateUniformBuffer(camera_uniform_buffer_, image_index, &camera_);
        render_engine_.UpdateUniformBuffer(color_uniform_buffer_, image_index, &color_model_);
        render_engine_.UpdateUniformBuffer(texture_uniform_buffer_, image_index, &texture_model_);

        render_engine_.SubmitDrawCommands(image_index);

        render_engine_.PresentImage(image_index);
    }

private:
    RenderEngine& render_engine_;
    bool startup_ = false;

    std::shared_ptr<RenderEngine::UniformBuffer> camera_uniform_buffer_{};

    std::shared_ptr<RenderEngine::UniformBuffer> color_uniform_buffer_{};
    std::shared_ptr<RenderEngine::DescriptorSet> color_descriptor_set_{};
    std::shared_ptr<RenderEngine::GraphicsPipeline> color_graphics_pipeline_{};

    std::shared_ptr<RenderEngine::UniformBuffer> texture_uniform_buffer_{};
    std::shared_ptr<RenderEngine::DescriptorSet> texture_descriptor_set_{};
    std::shared_ptr<RenderEngine::GraphicsPipeline> texture_graphics_pipeline_{};

    std::shared_ptr<RenderEngine::RenderPass> render_pass_{};

    struct CameraMatrix {
        glm::mat4 view_matrix;
        glm::mat4 projection_matrix;
    };

    struct ModelMatrix {
        glm::mat4 model_matrix;
    };

    CameraMatrix camera_{};

    ModelMatrix color_model_{};
    ModelMatrix texture_model_{};

    IndexedPrimitive color_primitive_{};
    IndexedPrimitive texture_primitive_{};

    void Startup() {
        render_pass_ = render_engine_.CreateRenderPass();

        camera_uniform_buffer_ = render_engine_.CreateUniformBuffer(sizeof(CameraMatrix));

        {
            std::vector<unsigned char> byte_code{};
            byte_code = Utility::ReadFile("shaders/color/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = Utility::ReadFile("shaders/color/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());

            color_uniform_buffer_ = render_engine_.CreateUniformBuffer(sizeof(ModelMatrix));

            color_descriptor_set_ = render_engine_.CreateDescriptorSet({camera_uniform_buffer_, color_uniform_buffer_}, 0);

            color_graphics_pipeline_ = render_engine_.CreateGraphicsPipeline
            (
                render_pass_,
                vertex_shader_module,
                fragment_shader_module,
                {},
                Vertex_Color::getBindingDescription(),
                Vertex_Color::getAttributeDescriptions(),
                color_descriptor_set_,
                0,
                true,
                false
            );
        }

        {
            std::vector<unsigned char> byte_code{};
            byte_code = Utility::ReadFile("shaders/notexture/vert.spv");
            VkShaderModule vertex_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());
            byte_code = Utility::ReadFile("shaders/notexture/frag.spv");
            VkShaderModule fragment_shader_module = render_engine_.CreateShaderModule(byte_code.data(), byte_code.size());

            texture_uniform_buffer_ = render_engine_.CreateUniformBuffer(sizeof(ModelMatrix));

            texture_descriptor_set_ = render_engine_.CreateDescriptorSet({camera_uniform_buffer_, texture_uniform_buffer_}, 0);

            texture_graphics_pipeline_ = render_engine_.CreateGraphicsPipeline
            (
                render_pass_,
                vertex_shader_module,
                fragment_shader_module,
                {},
                Vertex_Texture::getBindingDescription(),
                Vertex_Texture::getAttributeDescriptions(),
                texture_descriptor_set_,
                0,
                true,
                false
            );
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
    }
};
