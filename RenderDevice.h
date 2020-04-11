#pragma once

#include <algorithm>
#include <set>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")

class IndexedPrimitive {
public:
    VkBuffer vertex_buffer_;
    VkDeviceMemory vertex_buffer_memory_;
    VkBuffer index_buffer_;
    VkDeviceMemory index_buffer_memory_;
    uint32_t index_count_;
};

class RenderDevice {
public:
    static void (*Log)(const char* format, ...);

    VkSurfaceKHR surface_ = nullptr;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
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
    }

    void Destroy() {
        vkDestroyCommandPool(device_, command_pool_, nullptr);
        Log("command pool created");
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

private:
    bool debug_layers_ = false;
    VkInstance instance_;
    VkDebugUtilsMessengerEXT debug_messenger_;
    const std::vector<const char*> device_extensions_{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const std::vector<const char*> validation_layers_{"VK_LAYER_KHRONOS_validation"};

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
        RenderDevice::Log("%s", callback_data->pMessage);
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
};
