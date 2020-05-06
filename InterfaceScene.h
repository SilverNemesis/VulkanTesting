#pragma once

#include <Windows.h>

#include <imgui.h>

#include "Math.h"
#include "Scene.h"
#include "RenderEngine.h"
#include "Font.h"

class InterfaceScene : public Scene {
public:
    InterfaceScene(RenderEngine& render_engine, SDL_Window* window) : render_engine_(render_engine), window_(window) {}

    void OnQuit() {
        if (startup_) {
            vkDeviceWaitIdle(render_engine_.device_);
            render_engine_.DestroyGraphicsPipeline(graphics_pipeline_);
            render_engine_.DestroyDescriptorSet(descriptor_set_);

            render_engine_.DestroyTexture(texture_);

            render_engine_.DestroyBuffer(vertex_buffer_);
            render_engine_.DestroyBuffer(index_buffer_);
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

    void Update(std::array<bool, SDL_NUM_SCANCODES>& key_state, bool mouse_capture, int mouse_x, int mouse_y) {
        static Uint64 frequency = SDL_GetPerformanceFrequency();
        Uint64 current_time = SDL_GetPerformanceCounter();
        ImGuiIO& io = ImGui::GetIO();
        io.DeltaTime = last_time_ > 0 && current_time - last_time_ > 0 ? (float)((double)(current_time - last_time_) / frequency) : (float)(1.0f / 60.0f);
        last_time_ = current_time;

        UpdateMousePosAndButtons();
        UpdateMouseCursor();
        UpdateGamepads();
    }

    bool EventHandler(const SDL_Event* event) {
        return ProcessEvent(event);
    }

    void Render() {
        uint32_t image_index;

        if (!render_engine_.AcquireNextImage(image_index)) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        IM_ASSERT(io.Fonts->IsBuilt() && "Font atlas not built! It is generally built by the renderer back-end. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame().");

        io.DisplaySize = ImVec2((float)render_engine_.swapchain_extent_.width, (float)render_engine_.swapchain_extent_.width);
        io.DisplayFramebufferScale = ImVec2(1.0, 1.0);

        ImGui::NewFrame();

        if (show_demo_window_) {
            ImGui::ShowDemoWindow(&show_demo_window_);
        }

        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window_);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window_);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color_); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        if (show_another_window_) {
            ImGui::Begin("Another Window", &show_another_window_);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window_ = false;
            ImGui::End();
        }

        ImGui::Render();

        ImDrawData* draw_data = ImGui::GetDrawData();

        int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
        int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);

        size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

        if (vertex_buffer_.size < vertex_size) {
            render_engine_.CreateOrResizeBuffer(vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertex_buffer_);
        }

        if (index_buffer_.size < index_size) {
            render_engine_.CreateOrResizeBuffer(index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, index_buffer_);
        }

        if (vertex_size != 0 && index_size != 0) {
            ImDrawVert* vtx_dst = NULL;
            ImDrawIdx* idx_dst = NULL;

            vkMapMemory(render_engine_.device_, vertex_buffer_.memory, 0, vertex_size, 0, (void**)(&vtx_dst));
            vkMapMemory(render_engine_.device_, index_buffer_.memory, 0, index_size, 0, (void**)(&idx_dst));

            for (int n = 0; n < draw_data->CmdListsCount; n++) {
                const ImDrawList* cmd_list = draw_data->CmdLists[n];
                memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
                memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
                vtx_dst += cmd_list->VtxBuffer.Size;
                idx_dst += cmd_list->IdxBuffer.Size;
            }

            VkMappedMemoryRange range[2] = {};
            range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range[0].memory = vertex_buffer_.memory;
            range[0].size = VK_WHOLE_SIZE;
            range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range[1].memory = index_buffer_.memory;
            range[1].size = VK_WHOLE_SIZE;
            vkFlushMappedMemoryRanges(render_engine_.device_, 2, range);

            vkUnmapMemory(render_engine_.device_, vertex_buffer_.memory);
            vkUnmapMemory(render_engine_.device_, index_buffer_.memory);
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
        clear_values[0].color.float32[0] = clear_color_.x;
        clear_values[0].color.float32[1] = clear_color_.y;
        clear_values[0].color.float32[2] = clear_color_.z;
        clear_values[0].color.float32[3] = clear_color_.w;
        clear_values[1].depthStencil = {1.0f, 0};

        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->graphics_pipeline);

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_->pipeline_layout, 0, 1, &descriptor_set_->descriptor_sets[image_index], 0, nullptr);

        if (vertex_size != 0 && index_size != 0) {
            VkBuffer vertex_buffers[1] = {vertex_buffer_.buffer};
            VkDeviceSize vertex_offset[1] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, vertex_offset);
            vkCmdBindIndexBuffer(command_buffer, index_buffer_.buffer, 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
        }

        {
            VkViewport viewport;
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = (float)fb_width;
            viewport.height = (float)fb_height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        }

        {
            float scale[2];
            scale[0] = 2.0f / draw_data->DisplaySize.x;
            scale[1] = 2.0f / draw_data->DisplaySize.y;
            float translate[2];
            translate[0] = -1.0f - draw_data->DisplayPos.x * scale[0];
            translate[1] = -1.0f - draw_data->DisplayPos.y * scale[1];
            vkCmdPushConstants(command_buffer, graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
            vkCmdPushConstants(command_buffer, graphics_pipeline_->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);
        }

        ImVec2 clip_off = draw_data->DisplayPos;
        ImVec2 clip_scale = draw_data->FramebufferScale;

        int global_vtx_offset = 0;
        int global_idx_offset = 0;

        for (int n = 0; n < draw_data->CmdListsCount; n++) {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];

            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

                ImVec4 clip_rect;
                clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

                if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
                    if (clip_rect.x < 0.0f)
                        clip_rect.x = 0.0f;
                    if (clip_rect.y < 0.0f)
                        clip_rect.y = 0.0f;

                    VkRect2D scissor;
                    scissor.offset.x = (int32_t)(clip_rect.x);
                    scissor.offset.y = (int32_t)(clip_rect.y);
                    scissor.extent.width = (uint32_t)(clip_rect.z - clip_rect.x);
                    scissor.extent.height = (uint32_t)(clip_rect.w - clip_rect.y);
                    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

                    vkCmdDrawIndexed(command_buffer, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
                }
            }

            global_idx_offset += cmd_list->IdxBuffer.Size;
            global_vtx_offset += cmd_list->VtxBuffer.Size;
        }

        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }

        render_engine_.SubmitDrawCommands(image_index);

        render_engine_.PresentImage(image_index);
    }

private:
    RenderEngine& render_engine_;
    SDL_Window* window_{};
    bool startup_ = false;

    std::shared_ptr<RenderEngine::RenderPass> render_pass_{};
    std::shared_ptr<RenderEngine::GraphicsPipeline> graphics_pipeline_{};
    std::shared_ptr<RenderEngine::DescriptorSet> descriptor_set_{};

    TextureSampler texture_{};

    ImDrawData draw_data_;

    struct PushConstants {
        glm::vec2 scale{};
        glm::vec2 translate{};
    } push_constants_;

    Buffer vertex_buffer_{};
    Buffer index_buffer_{};

    Uint64 last_time_{};

    bool show_demo_window_ = true;
    bool show_another_window_ = false;
    ImVec4 clear_color_ = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    bool mouse_pressed_[3] = {false, false, false};
    SDL_Cursor* mouse_cursors_[ImGuiMouseCursor_COUNT] = {};

    static VkVertexInputBindingDescription getBindingDescription() {
        static VkVertexInputBindingDescription bindingDescription = {0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX};
        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        static std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {{
            {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)},
            {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)},
            {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)}
            }};
        return attributeDescriptions;
    }

    void Startup() {
        render_pass_ = render_engine_.CreateRenderPass();

        {
            descriptor_set_ = render_engine_.CreateDescriptorSet({}, 1);

            graphics_pipeline_ = render_engine_.CreateGraphicsPipeline
            (
                render_pass_,
                "shaders/interface/vert.spv",
                "shaders/interface/frag.spv",
                {
                    PushConstant{offsetof(PushConstants, scale), sizeof(push_constants_.scale), VK_SHADER_STAGE_FRAGMENT_BIT},
                    PushConstant{offsetof(PushConstants, translate), sizeof(push_constants_.translate), VK_SHADER_STAGE_VERTEX_BIT}
                },
                getBindingDescription(),
                getAttributeDescriptions(),
                descriptor_set_,
                0,
                false,
                true,
                true,
                true
            );
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
        size_t upload_size = width * height * sizeof(char);
        render_engine_.CreateAlphaTexture(pixels, width, height, texture_);

        render_engine_.UpdateDescriptorSets(descriptor_set_, {texture_});

        ImGui::StyleColorsDark();
    }

    bool ProcessEvent(const SDL_Event* event) {
        ImGuiIO& io = ImGui::GetIO();
        switch (event->type) {
        case SDL_MOUSEWHEEL:
        {
            if (event->wheel.x > 0) io.MouseWheelH += 1;
            if (event->wheel.x < 0) io.MouseWheelH -= 1;
            if (event->wheel.y > 0) io.MouseWheel += 1;
            if (event->wheel.y < 0) io.MouseWheel -= 1;
            return true;
        }
        case SDL_MOUSEBUTTONDOWN:
        {
            if (event->button.button == SDL_BUTTON_LEFT) mouse_pressed_[0] = true;
            if (event->button.button == SDL_BUTTON_RIGHT) mouse_pressed_[1] = true;
            if (event->button.button == SDL_BUTTON_MIDDLE) mouse_pressed_[2] = true;
            return true;
        }
        case SDL_TEXTINPUT:
        {
            io.AddInputCharactersUTF8(event->text.text);
            return true;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            int key = event->key.keysym.scancode;
            IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
            io.KeysDown[key] = (event->type == SDL_KEYDOWN);
            io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
            io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
            io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
            io.KeySuper = false;
            return true;
        }
        }
        return false;
    }

    void UpdateMousePosAndButtons() {
        ImGuiIO& io = ImGui::GetIO();

        if (io.WantSetMousePos) {
            SDL_WarpMouseInWindow(window_, (int)io.MousePos.x, (int)io.MousePos.y);
        } else {
            io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
        }

        int mx, my;
        Uint32 mouse_buttons = SDL_GetMouseState(&mx, &my);
        io.MouseDown[0] = mouse_pressed_[0] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        io.MouseDown[1] = mouse_pressed_[1] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
        io.MouseDown[2] = mouse_pressed_[2] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
        mouse_pressed_[0] = mouse_pressed_[1] = mouse_pressed_[2] = false;

        if (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) {
            io.MousePos = ImVec2((float)mx, (float)my);
        }
    }

    void UpdateMouseCursor() {
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
            return;

        ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
        if (io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None) {
            SDL_ShowCursor(SDL_FALSE);
        } else {
            SDL_SetCursor(mouse_cursors_[imgui_cursor] ? mouse_cursors_[imgui_cursor] : mouse_cursors_[ImGuiMouseCursor_Arrow]);
            SDL_ShowCursor(SDL_TRUE);
        }
    }

    void UpdateGamepads() {
        ImGuiIO& io = ImGui::GetIO();
        memset(io.NavInputs, 0, sizeof(io.NavInputs));
        if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
            return;

        SDL_GameController* game_controller = SDL_GameControllerOpen(0);
        if (!game_controller) {
            io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
            return;
        }

#define MAP_BUTTON(NAV_NO, BUTTON_NO)       { io.NavInputs[NAV_NO] = (SDL_GameControllerGetButton(game_controller, BUTTON_NO) != 0) ? 1.0f : 0.0f; }
#define MAP_ANALOG(NAV_NO, AXIS_NO, V0, V1) { float vn = (float)(SDL_GameControllerGetAxis(game_controller, AXIS_NO) - V0) / (float)(V1 - V0); if (vn > 1.0f) vn = 1.0f; if (vn > 0.0f && io.NavInputs[NAV_NO] < vn) io.NavInputs[NAV_NO] = vn; }
        const int thumb_dead_zone = 8000;
        MAP_BUTTON(ImGuiNavInput_Activate, SDL_CONTROLLER_BUTTON_A);
        MAP_BUTTON(ImGuiNavInput_Cancel, SDL_CONTROLLER_BUTTON_B);
        MAP_BUTTON(ImGuiNavInput_Menu, SDL_CONTROLLER_BUTTON_X);
        MAP_BUTTON(ImGuiNavInput_Input, SDL_CONTROLLER_BUTTON_Y);
        MAP_BUTTON(ImGuiNavInput_DpadLeft, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        MAP_BUTTON(ImGuiNavInput_DpadRight, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        MAP_BUTTON(ImGuiNavInput_DpadUp, SDL_CONTROLLER_BUTTON_DPAD_UP);
        MAP_BUTTON(ImGuiNavInput_DpadDown, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        MAP_BUTTON(ImGuiNavInput_FocusPrev, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        MAP_BUTTON(ImGuiNavInput_FocusNext, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        MAP_BUTTON(ImGuiNavInput_TweakSlow, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        MAP_BUTTON(ImGuiNavInput_TweakFast, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        MAP_ANALOG(ImGuiNavInput_LStickLeft, SDL_CONTROLLER_AXIS_LEFTX, -thumb_dead_zone, -32768);
        MAP_ANALOG(ImGuiNavInput_LStickRight, SDL_CONTROLLER_AXIS_LEFTX, +thumb_dead_zone, +32767);
        MAP_ANALOG(ImGuiNavInput_LStickUp, SDL_CONTROLLER_AXIS_LEFTY, -thumb_dead_zone, -32767);
        MAP_ANALOG(ImGuiNavInput_LStickDown, SDL_CONTROLLER_AXIS_LEFTY, +thumb_dead_zone, +32767);
#undef MAP_BUTTON
#undef MAP_ANALOG

        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    }
};
