#pragma once

#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")

struct TextureSampler {
    VkSampler texture_sampler_{};
    VkImageView texture_image_view_{};
    VkImage texture_image_{};
    VkDeviceMemory texture_image_memory_{};
};

struct IndexedPrimitive {
    VkBuffer vertex_buffer_;
    VkDeviceMemory vertex_buffer_memory_;
    VkBuffer index_buffer_;
    VkDeviceMemory index_buffer_memory_;
    uint32_t index_count_;
};

class RenderEngine {
public:
    static void (*Log)(const char* format, ...);

    VkSurfaceKHR surface_ = nullptr;
    VkSampleCountFlagBits msaa_samples_ = VK_SAMPLE_COUNT_1_BIT;
    uint32_t graphics_family_index_ = 0;
    uint32_t present_family_index_ = 0;
    VkDevice device_ = nullptr;
    VkQueue graphics_queue_ = nullptr;
    VkQueue present_queue_ = nullptr;
    VkCommandPool command_pool_ = nullptr;
    VkSurfaceCapabilitiesKHR capabilities_{};
    uint32_t image_count_ = 0;
    VkSurfaceFormatKHR surface_format_{};
    VkFormat depth_format_ = VK_FORMAT_UNDEFINED;
    VkPresentModeKHR present_mode_ = VK_PRESENT_MODE_IMMEDIATE_KHR;
    VkSwapchainKHR swapchain_{};
    VkExtent2D swapchain_extent_{};
    std::vector<VkCommandBuffer> command_buffers_{};
    uint32_t subpass_count_ = 1;
    VkRenderPass render_pass_{};
    std::vector<VkFramebuffer> framebuffers_{};

    RenderEngine(uint32_t subpass_count) : subpass_count_(subpass_count) {}

    void Initialize(uint32_t window_width, uint32_t window_height, std::vector<const char*>& required_extensions, void (*CreateSurface)(void* window, VkInstance& instance, VkSurfaceKHR& surface), void* window) {
#ifdef _DEBUG
        debug_layers_ = true;
#endif
        CreateInstance(required_extensions);
        Log("instance created");
        if (debug_layers_) {
            SetupDebugMessenger();
            Log("debug messenger created");
        }
        CreateSurface(window, instance_, surface_);
        Log("surface created");
        PickPhysicalDevice();
        Log("physical device selected");
        CreateLogicalDevice();
        Log("logical device created");
        CreateCommandPool();
        Log("command pool created");
        ChooseSwapExtent(window_width, window_height);
        image_count_ = capabilities_.minImageCount > 2 ? capabilities_.minImageCount : 2;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
        QuerySwapChainSupport(physical_device_, capabilities_, formats, present_modes);
        if (capabilities_.maxImageCount > 0 && image_count_ > capabilities_.maxImageCount) {
            image_count_ = capabilities_.maxImageCount;
        }
        surface_format_ = ChooseSwapSurfaceFormat(formats);
        present_mode_ = ChooseSwapPresentMode(present_modes);
        depth_format_ = FindDepthFormat();
        CreateSwapchain(window_width, window_height);
        CreateRenderPass();
        CreateFramebuffers(render_pass_, framebuffers_);
        Log("render pass created");
    }

    void Destroy() {
        for (auto framebuffer : framebuffers_) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
        vkDestroyRenderPass(device_, render_pass_, nullptr);
        DestroySwapchain();
        Log("render pass destroyed");
        vkDestroyCommandPool(device_, command_pool_, nullptr);
        Log("command pool destroyed");
        vkDestroyDevice(device_, nullptr);
        Log("logical device destroyed");
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        Log("surface destroyed");
        if (debug_layers_) {
            DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
            Log("debug messenger destroyed");
        }
        vkDestroyInstance(instance_, nullptr);
        Log("instance destroyed");
    }

    void RebuildSwapchain(uint32_t window_width, uint32_t window_height) {
        if (swapchain_extent_.width != window_width || swapchain_extent_.height != window_height) {
            for (auto framebuffer : framebuffers_) {
                vkDestroyFramebuffer(device_, framebuffer, nullptr);
            }
            DestroySwapchain();
            CreateSwapchain(window_width, window_height);
            CreateFramebuffers(render_pass_, framebuffers_);
        }
    }

    void CreateFramebuffers(VkRenderPass render_pass, std::vector<VkFramebuffer>& framebuffers) {
        framebuffers.resize(swapchain_image_views_.size());

        for (size_t i = 0; i < swapchain_image_views_.size(); i++) {
            std::array<VkImageView, 3> attachments = {
                color_image_view_,
                depth_image_view_,
                swapchain_image_views_[i]
            };

            VkFramebufferCreateInfo framebuffer_info = {};
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass = render_pass;
            framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = swapchain_extent_.width;
            framebuffer_info.height = swapchain_extent_.height;
            framebuffer_info.layers = 1;

            if (vkCreateFramebuffer(device_, &framebuffer_info, nullptr, &framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer");
            }
        }
    }

    VkExtent2D ChooseSwapExtent(uint32_t windowWidth, uint32_t windowHeight) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities_);

        if (capabilities_.currentExtent.width != UINT32_MAX) {
            return capabilities_.currentExtent;
        } else {
            VkExtent2D actual_extent = {windowWidth, windowHeight};

            actual_extent.width = std::max(capabilities_.minImageExtent.width, std::min(capabilities_.maxImageExtent.width, actual_extent.width));
            actual_extent.height = std::max(capabilities_.minImageExtent.height, std::min(capabilities_.maxImageExtent.height, actual_extent.height));

            return actual_extent;
        }
    }

    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory) {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device_, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer");
        }

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device_, buffer, &memory_requirements);

        VkMemoryAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = FindMemoryType(memory_requirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device_, &allocate_info, nullptr, &buffer_memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory");
        }

        vkBindBufferMemory(device_, buffer, buffer_memory, 0);
    }

    VkShaderModule CreateShaderModule(const unsigned char* byte_code, size_t byte_code_length) {
        VkShaderModuleCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = byte_code_length;
        create_info.pCode = reinterpret_cast<const uint32_t*>(byte_code);

        VkShaderModule shader_module;
        if (vkCreateShaderModule(device_, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module");
        }

        return shader_module;
    }

    void CreateTexture(unsigned char* pixels, int texWidth, int texHeight, TextureSampler& texture_sampler) {
        VkDeviceSize image_size = static_cast<VkDeviceSize>(texWidth) * texHeight * 4;
        uint32_t mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        CreateBuffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

        void* data;
        vkMapMemory(device_, staging_buffer_memory, 0, image_size, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(image_size));
        vkUnmapMemory(device_, staging_buffer_memory);

        CreateImage(texWidth, texHeight, mip_levels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_sampler.texture_image_, texture_sampler.texture_image_memory_);

        TransformImageLayout(texture_sampler.texture_image_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels);
        CopyBufferToImage(staging_buffer, texture_sampler.texture_image_, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_buffer_memory, nullptr);

        GenerateMipmaps(texture_sampler.texture_image_, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mip_levels);

        texture_sampler.texture_image_view_ = CreateImageView(texture_sampler.texture_image_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mip_levels);

        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = 16;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.minLod = 0;
        sampler_info.maxLod = static_cast<float>(mip_levels);
        sampler_info.mipLodBias = 0;

        if (vkCreateSampler(device_, &sampler_info, nullptr, &texture_sampler.texture_sampler_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler");
        }
    }

    void DestroyTexture(TextureSampler& texture_sampler) {
        vkDestroySampler(device_, texture_sampler.texture_sampler_, nullptr);
        vkDestroyImageView(device_, texture_sampler.texture_image_view_, nullptr);
        vkDestroyImage(device_, texture_sampler.texture_image_, nullptr);
        vkFreeMemory(device_, texture_sampler.texture_image_memory_, nullptr);
    }

    template <class Vertex, class Index>
    void CreateIndexedPrimitive(std::vector<Vertex>& vertices, std::vector<Index>& indices, IndexedPrimitive& primitive) {
        VkDeviceSize bufferSize = vertices.size() * sizeof(vertices[0]);

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t)bufferSize);
        vkUnmapMemory(device_, stagingBufferMemory);

        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, primitive.vertex_buffer_, primitive.vertex_buffer_memory_);

        CopyBuffer(stagingBuffer, primitive.vertex_buffer_, bufferSize);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingBufferMemory, nullptr);

        primitive.index_count_ = static_cast<uint32_t>(indices.size());

        bufferSize = indices.size() * sizeof(indices[0]);

        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t)bufferSize);
        vkUnmapMemory(device_, stagingBufferMemory);

        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, primitive.index_buffer_, primitive.index_buffer_memory_);

        CopyBuffer(stagingBuffer, primitive.index_buffer_, bufferSize);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingBufferMemory, nullptr);
    }

    void DestroyIndexedPrimitive(IndexedPrimitive& primitive) {
        vkDestroyBuffer(device_, primitive.index_buffer_, nullptr);
        vkFreeMemory(device_, primitive.index_buffer_memory_, nullptr);
        vkDestroyBuffer(device_, primitive.vertex_buffer_, nullptr);
        vkFreeMemory(device_, primitive.vertex_buffer_memory_, nullptr);
    }

    void CreateImage(uint32_t width, uint32_t height, uint32_t mip_levels, VkSampleCountFlagBits num_samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory) {
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = mip_levels;
        image_info.arrayLayers = 1;
        image_info.format = format;
        image_info.tiling = tiling;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = usage;
        image_info.samples = num_samples;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device_, &image_info, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image");
        }

        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(device_, image, &memory_requirements);

        VkMemoryAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = FindMemoryType(memory_requirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device_, &allocate_info, nullptr, &image_memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory");
        }

        vkBindImageMemory(device_, image, image_memory, 0);
    }

    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t mip_levels) {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange.aspectMask = aspect_flags;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = mip_levels;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VkImageView image_view;
        if (vkCreateImageView(device_, &view_info, nullptr, &image_view) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view");
        }

        return image_view;
    }

private:
    bool debug_layers_ = false;
    VkInstance instance_;

    VkDebugUtilsMessengerEXT debug_messenger_;

    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    const std::vector<const char*> device_extensions_{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const std::vector<const char*> validation_layers_{"VK_LAYER_KHRONOS_validation"};

    std::vector<VkImage> swapchain_images_{};
    std::vector<VkImageView> swapchain_image_views_{};

    VkImage color_image_{};
    VkDeviceMemory color_image_memory_{};
    VkImageView color_image_view_{};

    VkImage depth_image_{};
    VkDeviceMemory depth_image_memory_{};
    VkImageView depth_image_view_{};

    void CreateInstance(std::vector<const char*>& required_extensions) {
        if (debug_layers_) {
            if (!CheckValidationLayerSupport()) {
                throw std::runtime_error("validation layers are not available");
            }
        }

        VkApplicationInfo application_info = {};
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pApplicationName = "Vulkan Testing";
        application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.pEngineName = "No Engine";
        application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &application_info;

        if (debug_layers_) {
            required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
        create_info.ppEnabledExtensionNames = required_extensions.data();

        if (debug_layers_) {
            create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers_.size());
            create_info.ppEnabledLayerNames = validation_layers_.data();

            VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
            PopulateDebugMessengerCreateInfo(debug_create_info);
            create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
        } else {
            create_info.enabledLayerCount = 0;
            create_info.pNext = nullptr;
        }

        if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance");
        }
    }

    bool CheckValidationLayerSupport() {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for (const char* layer_name : validation_layers_) {
            bool layer_found = false;

            for (const auto& layer_properties : available_layers) {
                if (strcmp(layer_name, layer_properties.layerName) == 0) {
                    layer_found = true;
                    break;
                }
            }

            if (!layer_found) {
                return false;
            }
        }

        return true;
    }

    void SetupDebugMessenger() {
        VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
        PopulateDebugMessengerCreateInfo(debug_create_info);

        if (CreateDebugUtilsMessengerEXT(instance_, &debug_create_info, nullptr, &debug_messenger_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create debug messenger");
        }
    }

    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info) {
        create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = DebugCallback;
    }

    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* debug_create_info, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* debug_messenger) {
        auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (vkCreateDebugUtilsMessengerEXT != nullptr) {
            return vkCreateDebugUtilsMessengerEXT(instance, debug_create_info, allocator, debug_messenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* allocator) {
        auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (vkDestroyDebugUtilsMessengerEXT != nullptr) {
            vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, allocator);
        }
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
        RenderEngine::Log("%s", callback_data->pMessage);
        return VK_FALSE;
    }

    void PickPhysicalDevice() {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

        if (device_count == 0) {
            throw std::runtime_error("unable to find a GPU with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

        for (const auto& device : devices) {
            if (IsDeviceSuitable(device)) {
                physical_device_ = device;
                msaa_samples_ = GetMaxUsableSampleCount();
                break;
            }
        }

        if (physical_device_ == VK_NULL_HANDLE) {
            throw std::runtime_error("unable to find a GPU with the required features");
        }
    }

    bool IsDeviceSuitable(VkPhysicalDevice physical_device) {
        if (!FindQueueFamilies(physical_device)) {
            return false;
        }

        bool extensions_supported = CheckDeviceExtensionSupport(physical_device);

        bool swap_chain_adequate = false;
        if (extensions_supported) {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
            QuerySwapChainSupport(physical_device, capabilities, formats, presentModes);
            swap_chain_adequate = !formats.empty() && !presentModes.empty();
        }

        VkPhysicalDeviceFeatures supported_features;
        vkGetPhysicalDeviceFeatures(physical_device, &supported_features);

        return extensions_supported && swap_chain_adequate && supported_features.samplerAnisotropy;
    }

    VkSampleCountFlagBits GetMaxUsableSampleCount() {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(physical_device_, &physical_device_properties);

        VkSampleCountFlags counts = physical_device_properties.limits.framebufferColorSampleCounts & physical_device_properties.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    bool FindQueueFamilies(VkPhysicalDevice physical_device) {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

        graphics_family_index_ = -1;
        present_family_index_ = -1;
        int index = 0;
        for (const auto& queue_family : queue_families) {
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_family_index_ = index;
            }

            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, index, surface_, &present_support);

            if (present_support) {
                present_family_index_ = index;
            }

            if (graphics_family_index_ != -1 && present_family_index_ != -1) {
                return true;
            }

            index++;
        }

        return false;
    }

    bool CheckDeviceExtensionSupport(VkPhysicalDevice physical_device) {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());

        std::set<std::string> required_extensions;
        for (auto device_extension : device_extensions_) {
            required_extensions.insert(device_extension);
        }

        for (const auto& extension : available_extensions) {
            required_extensions.erase(extension.extensionName);
        }

        return required_extensions.empty();
    }

    void QuerySwapChainSupport(VkPhysicalDevice physical_device, VkSurfaceCapabilitiesKHR& capabilities, std::vector<VkSurfaceFormatKHR>& formats, std::vector<VkPresentModeKHR>& present_modes) {
        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface_, &format_count, nullptr);

        if (format_count != 0) {
            formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface_, &format_count, formats.data());
        }

        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface_, &present_mode_count, nullptr);

        if (present_mode_count != 0) {
            present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface_, &present_mode_count, present_modes.data());
        }
    }

    void CreateLogicalDevice() {
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<uint32_t> unique_queue_families = {graphics_family_index_, present_family_index_};

        float queuePriority = 1.0f;
        for (uint32_t queue_family : unique_queue_families) {
            VkDeviceQueueCreateInfo queue_create_info = {};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queuePriority;
            queue_create_infos.push_back(queue_create_info);
        }

        VkPhysicalDeviceFeatures device_features = {};
        device_features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        create_info.pQueueCreateInfos = queue_create_infos.data();

        create_info.pEnabledFeatures = &device_features;

        create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions_.size());
        create_info.ppEnabledExtensionNames = device_extensions_.data();

        if (debug_layers_) {
            create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers_.size());
            create_info.ppEnabledLayerNames = validation_layers_.data();
        } else {
            create_info.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physical_device_, &create_info, nullptr, &device_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device");
        }

        vkGetDeviceQueue(device_, graphics_family_index_, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, present_family_index_, 0, &present_queue_);
    }

    void CreateCommandPool() {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = graphics_family_index_;

        if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool");
        }
    }

    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);

        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
            if ((type_filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("unable to find required memory type");
    }

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) {
        for (const auto& available_format : available_formats) {
            if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return available_format;
            }
        }

        return available_formats[0];
    }

    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) {
        for (const auto& available_present_mode : available_present_modes) {
            if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return available_present_mode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkFormat FindDepthFormat() {
        return FindSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physical_device_, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format");
    }

    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = BeginCommands();

        VkBufferCopy copyRegion = {};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        EndCommands(commandBuffer);
    }

    void CreateCommandBuffers() {
        command_buffers_.resize(image_count_);

        VkCommandBufferAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = command_pool_;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = (uint32_t)command_buffers_.size();

        if (vkAllocateCommandBuffers(device_, &allocate_info, command_buffers_.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers");
        }
    }

    void CreateSwapchain(uint32_t window_width, uint32_t window_height) {
        VkExtent2D extent = ChooseSwapExtent(window_width, window_height);

        VkSwapchainCreateInfoKHR create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = surface_;

        create_info.minImageCount = image_count_;
        create_info.imageFormat = surface_format_.format;
        create_info.imageColorSpace = surface_format_.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queue_family_indices[] = {graphics_family_index_, present_family_index_};

        if (graphics_family_index_ != present_family_index_) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = queue_family_indices;
        } else {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        create_info.preTransform = capabilities_.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = present_mode_;
        create_info.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(device_, &create_info, nullptr, &swapchain_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain");
        }

        vkGetSwapchainImagesKHR(device_, swapchain_, &image_count_, nullptr);
        swapchain_images_.resize(image_count_);
        vkGetSwapchainImagesKHR(device_, swapchain_, &image_count_, swapchain_images_.data());

        swapchain_extent_ = extent;

        swapchain_image_views_.resize(image_count_);

        for (size_t i = 0; i < image_count_; i++) {
            swapchain_image_views_[i] = CreateImageView(swapchain_images_[i], surface_format_.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        }

        VkFormat color_format = surface_format_.format;
        CreateImage(swapchain_extent_.width, swapchain_extent_.height, 1, msaa_samples_, color_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, color_image_, color_image_memory_);
        color_image_view_ = CreateImageView(color_image_, color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        VkFormat depth_format = depth_format_;
        CreateImage(swapchain_extent_.width, swapchain_extent_.height, 1, msaa_samples_, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_image_, depth_image_memory_);
        depth_image_view_ = CreateImageView(depth_image_, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

        CreateCommandBuffers();

        RenderEngine::Log("swapchain created x=%d, y=%d", window_width, window_height);
    }

    void DestroySwapchain() {
        vkFreeCommandBuffers(device_, command_pool_, static_cast<uint32_t>(command_buffers_.size()), command_buffers_.data());

        vkDestroyImageView(device_, depth_image_view_, nullptr);
        vkDestroyImage(device_, depth_image_, nullptr);
        vkFreeMemory(device_, depth_image_memory_, nullptr);

        vkDestroyImageView(device_, color_image_view_, nullptr);
        vkDestroyImage(device_, color_image_, nullptr);
        vkFreeMemory(device_, color_image_memory_, nullptr);

        for (auto image_view : swapchain_image_views_) {
            vkDestroyImageView(device_, image_view, nullptr);
        }

        vkDestroySwapchainKHR(device_, swapchain_, nullptr);

        RenderEngine::Log("swapchain destroyed");
    }

    void CreateRenderPass() {
        VkAttachmentDescription color_attachment = {};
        color_attachment.format = surface_format_.format;
        color_attachment.samples = msaa_samples_;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depth_attachment = {};
        depth_attachment.format = depth_format_;
        depth_attachment.samples = msaa_samples_;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription color_attachment_resolve = {};
        color_attachment_resolve.format = surface_format_.format;
        color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_reference = {};
        color_attachment_reference.attachment = 0;
        color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_reference = {};
        depth_attachment_reference.attachment = 1;
        depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_resolve_reference = {};
        color_attachment_resolve_reference.attachment = 2;
        color_attachment_resolve_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        std::vector<VkSubpassDescription> subpasses = {};
        std::vector<VkSubpassDependency> dependencies = {};

        for (uint32_t i = 0; i < subpass_count_; i++) {
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_attachment_reference;
            subpass.pDepthStencilAttachment = &depth_attachment_reference;
            subpass.pResolveAttachments = &color_attachment_resolve_reference;
            subpasses.push_back(subpass);
        }

        VkSubpassDependency dependency = {};
        if (subpass_count_ == 1) {
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies.push_back(dependency);
        } else if (subpass_count_ == 2) {
            {
                dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
                dependency.dstSubpass = 0;
                dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.srcAccessMask = 0;
                dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependencies.push_back(dependency);
            }
            {
                dependency.srcSubpass = 0;
                dependency.dstSubpass = 1;
                dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependencies.push_back(dependency);
            }
        }

        std::array<VkAttachmentDescription, 3> attachments = {color_attachment, depth_attachment, color_attachment_resolve};
        VkRenderPassCreateInfo render_pass_Info = {};
        render_pass_Info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_Info.attachmentCount = static_cast<uint32_t>(attachments.size());
        render_pass_Info.pAttachments = attachments.data();
        render_pass_Info.subpassCount = static_cast<uint32_t>(subpasses.size());
        render_pass_Info.pSubpasses = subpasses.data();
        render_pass_Info.dependencyCount = static_cast<uint32_t>(dependencies.size());
        render_pass_Info.pDependencies = dependencies.data();

        if (vkCreateRenderPass(device_, &render_pass_Info, nullptr, &render_pass_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass");
        }
    }

    void GenerateMipmaps(VkImage image, VkFormat image_format, int32_t texture_width, int32_t texture_height, uint32_t mip_levels) {
        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties(physical_device_, image_format, &format_properties);

        if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("texture image format does not support linear blitting");
        }

        VkCommandBuffer command_buffer = BeginCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mip_width = texture_width;
        int32_t mip_height = texture_height;

        for (uint32_t i = 1; i < mip_levels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkImageBlit blit = {};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mip_width, mip_height, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            if (mip_width > 1) {
                mip_width /= 2;
            }

            if (mip_height > 1) {
                mip_height /= 2;
            }
        }

        barrier.subresourceRange.baseMipLevel = mip_levels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        EndCommands(command_buffer);
    }

    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer command_buffer = BeginCommands();

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        EndCommands(command_buffer);
    }

    void TransformImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t mipLevels) {
        VkCommandBuffer command_buffer = BeginCommands();

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags destination_stage;

        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition");
        }

        vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        EndCommands(command_buffer);
    }

    VkCommandBuffer BeginCommands() {
        VkCommandBufferAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandPool = command_pool_;
        allocate_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(command_buffer, &begin_info);

        return command_buffer;
    }

    void EndCommands(VkCommandBuffer command_buffer) {
        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_queue_);

        vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
    }
};
