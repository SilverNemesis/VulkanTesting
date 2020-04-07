#pragma once

#include "RenderDevice.h"

#include <array>

class RenderSwapchain {
public:
    VkSwapchainKHR swapchain_;
    VkExtent2D swapchain_extent_;

    RenderSwapchain(RenderDevice& render_device) : render_device_(render_device) {}

    void Initialize(uint32_t windowWidth, uint32_t windowHeight) {
        CreateSwapChain(windowWidth, windowHeight);
        RenderDevice::Log("swapchain created");
        CreateImageViews();
        RenderDevice::Log("image views created");
        CreateColorResources();
        RenderDevice::Log("color resources created");
        CreateDepthResources();
        RenderDevice::Log("depth resources created");
    }

    void Destroy() {
        vkDestroyImageView(render_device_.device_, depth_image_view_, nullptr);
        vkDestroyImage(render_device_.device_, depth_image_, nullptr);
        vkFreeMemory(render_device_.device_, depth_image_memory_, nullptr);
        RenderDevice::Log("depth resources destroyed");

        vkDestroyImageView(render_device_.device_, color_image_view_, nullptr);
        vkDestroyImage(render_device_.device_, color_image_, nullptr);
        vkFreeMemory(render_device_.device_, color_image_memory_, nullptr);
        RenderDevice::Log("color resources destroyed");

        for (auto image_view : image_views_) {
            vkDestroyImageView(render_device_.device_, image_view, nullptr);
        }
        RenderDevice::Log("image views destroyed");

        vkDestroySwapchainKHR(render_device_.device_, swapchain_, nullptr);
        RenderDevice::Log("swapchain destroyed");
    }

private:
    RenderDevice& render_device_;

    std::vector<VkImage> images_;
    std::vector<VkImageView> image_views_;

    VkImage color_image_;
    VkDeviceMemory color_image_memory_;
    VkImageView color_image_view_;

    VkImage depth_image_;
    VkDeviceMemory depth_image_memory_;
    VkImageView depth_image_view_;

    void CreateSwapChain(uint32_t window_width, uint32_t window_height) {
        VkExtent2D extent = render_device_.ChooseSwapExtent(window_width, window_height);

        VkSwapchainCreateInfoKHR create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = render_device_.surface_;

        create_info.minImageCount = render_device_.image_count_;
        create_info.imageFormat = render_device_.surface_format_.format;
        create_info.imageColorSpace = render_device_.surface_format_.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queue_family_indices[] = {render_device_.graphics_family_index_, render_device_.present_family_index_};

        if (render_device_.graphics_family_index_ != render_device_.present_family_index_) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = queue_family_indices;
        } else {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        create_info.preTransform = render_device_.capabilities_.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = render_device_.present_mode_;
        create_info.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(render_device_.device_, &create_info, nullptr, &swapchain_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(render_device_.device_, swapchain_, &render_device_.image_count_, nullptr);
        images_.resize(render_device_.image_count_);
        vkGetSwapchainImagesKHR(render_device_.device_, swapchain_, &render_device_.image_count_, images_.data());

        swapchain_extent_ = extent;
    }

    void CreateImageViews() {
        image_views_.resize(images_.size());

        for (size_t i = 0; i < images_.size(); i++) {
            image_views_[i] = CreateImageView(render_device_.device_, images_[i], render_device_.surface_format_.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        }
    }

    void CreateColorResources() {
        VkFormat color_format = render_device_.surface_format_.format;
        CreateImage(render_device_.device_, swapchain_extent_.width, swapchain_extent_.height, 1, render_device_.msaa_samples_, color_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, color_image_, color_image_memory_);
        color_image_view_ = CreateImageView(render_device_.device_, color_image_, color_format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }

    void CreateDepthResources() {
        VkFormat depth_format = render_device_.depth_format_;
        CreateImage(render_device_.device_, swapchain_extent_.width, swapchain_extent_.height, 1, render_device_.msaa_samples_, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_image_, depth_image_memory_);
        depth_image_view_ = CreateImageView(render_device_.device_, depth_image_, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
    }

    void CreateImage(VkDevice device, uint32_t width, uint32_t height, uint32_t mip_levels, VkSampleCountFlagBits num_samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory) {
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

        if (vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(device, image, &memory_requirements);

        VkMemoryAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = render_device_.FindMemoryType(memory_requirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocate_info, nullptr, &image_memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, image_memory, 0);
    }

    VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t mip_levels) {
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
        if (vkCreateImageView(device, &view_info, nullptr, &image_view) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }

        return image_view;
    }
};
