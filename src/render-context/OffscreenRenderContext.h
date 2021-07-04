#pragma once

#include <iostream>
#include <vector>
#include "../utils/vulkan.h"
#include "../memory/VulkanImage.h"

class OffscreenRenderContext {
    public:
        VkRenderPass renderPass;
        // We need only one framebuffer for off screen rendering, since only one drawing operation is performend at once.
        VkFramebuffer framebuffer;

        VulkanImage::VulkanImage colorImage;
        VulkanImage::VulkanImage depthImage;
        
        void init(VulkanSwapchain *swapchainContext) {
            this->swapchainContext = swapchainContext;
            createColorResources();
            createDepthResources();
            createRenderPass();
            createFramebuffers();
        }

        void destroy() {
            colorImage.destroy();
            depthImage.destroy();
            vkDestroyFramebuffer(VulkanGlobal::context.device, framebuffer, nullptr);
            vkDestroyRenderPass(VulkanGlobal::context.device, renderPass, nullptr);
        }

    private:
        VulkanSwapchain *swapchainContext;
        
        void createRenderPass() {
            // Color attachment for a framebuffer.
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = swapchainContext->swapChainImageFormat;
            colorAttachment.samples = VulkanGlobal::context.msaaSamples;
            // Clear the frame before render.
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            // Rendered contents will be stored in memory and can be read later.
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Attachment for a sub-pass.
            VkAttachmentReference colorAttachmentRef{};
            // Index of the attachement in the attachments array.
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentDescription depthAttachment{};
            depthAttachment.format = findDepthFormat();
            depthAttachment.samples = VulkanGlobal::context.msaaSamples;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkAttachmentReference depthAttachmentRef{};
            depthAttachmentRef.attachment = 1;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;
            subpass.pDepthStencilAttachment = &depthAttachmentRef;

            std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            renderPassInfo.pAttachments = attachments.data();
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;

            // Use subpass dependencies for layout transitions
            std::array<VkSubpassDependency, 2> dependencies;

            dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[0].dstSubpass = 0;
            dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            dependencies[1].srcSubpass = 0;
            dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            renderPassInfo.dependencyCount = 2;
            renderPassInfo.pDependencies = dependencies.data();
            
            if (vkCreateRenderPass(VulkanGlobal::context.device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
                throw std::runtime_error("failed to create render pass!");
            }
        }

        void createFramebuffers() {            
            std::array<VkImageView, 2> attachments = {
                colorImage.imageView,
                depthImage.imageView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapchainContext->swapChainExtent.width;
            framebufferInfo.height = swapchainContext->swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(VulkanGlobal::context.device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }

        VkFormat findDepthFormat() {
            return VulkanGlobal::context.findSupportedFormat(
                {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            );
        }

        bool hasStencilComponent(VkFormat format) {
            return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
        }

        void createDepthResources() {
            VkFormat depthFormat = findDepthFormat();
            VulkanImage::createImage(swapchainContext->swapChainExtent.width,
                                     swapchainContext->swapChainExtent.height,
                                     1,
                                     VK_SAMPLE_COUNT_1_BIT,
                                     depthFormat,
                                     VK_IMAGE_TILING_OPTIMAL,
                                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                     VK_IMAGE_ASPECT_DEPTH_BIT,
                                     VMA_MEMORY_USAGE_GPU_ONLY,
                                     depthImage);
        }

        void createColorResources() {
            VkFormat colorFormat = swapchainContext->swapChainImageFormat;

            VulkanImage::createImage(swapchainContext->swapChainExtent.width,
                                     swapchainContext->swapChainExtent.height,
                                     1,
                                     VK_SAMPLE_COUNT_1_BIT,
                                     colorFormat,
                                     VK_IMAGE_TILING_OPTIMAL,
                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                     VK_IMAGE_ASPECT_COLOR_BIT,
                                     VMA_MEMORY_USAGE_GPU_ONLY,
                                     colorImage);
        }
};
