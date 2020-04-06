#pragma once

#include <algorithm>
#include <set>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")

class RenderDevice {
public:
    static void (*Log)(const char* format, ...);

    VkSurfaceKHR surface_;

    void Initialize(uint32_t window_width, uint32_t window_height, std::vector<const char*>& required_extensions, void (*CreateSurface)(void* window, VkInstance& instance, VkSurfaceKHR& surface), void* window) {
        CreateInstance(required_extensions);
        Log("instance created");
#ifdef _DEBUG
        SetupDebugMessenger();
        Log("debug messenger created");
#endif
        CreateSurface(window, instance_, surface_);
        Log("surface created");
    }

    void Destroy() {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        Log("surface destroyed");
#ifdef _DEBUG
        DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
        Log("debug messenger destroyed");
#endif
        vkDestroyInstance(instance_, nullptr);
        Log("instance destroyed");
    }

private:
    VkInstance instance_;
    VkDebugUtilsMessengerEXT debug_messenger_;

    const std::vector<const char*> validation_layers_ = {
        "VK_LAYER_KHRONOS_validation"
    };

    void CreateInstance(std::vector<const char*>& required_extensions) {
#ifdef _DEBUG
        if (!CheckValidationLayerSupport()) {
            throw std::runtime_error("validation layers are not available");
        }
#endif

        VkApplicationInfo application_info = {};
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pApplicationName = "Hello Triangle";
        application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.pEngineName = "No Engine";
        application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &application_info;

#ifdef _DEBUG
        required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
        create_info.ppEnabledExtensionNames = required_extensions.data();

#ifdef _DEBUG
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers_.size());
        create_info.ppEnabledLayerNames = validation_layers_.data();

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
        PopulateDebugMessengerCreateInfo(debug_create_info);
        create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
#else
        create_info.enabledLayerCount = 0;
        create_info.pNext = nullptr;
#endif

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
};
