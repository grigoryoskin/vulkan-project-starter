#pragma once
#include <vector>
#include <memory>
#include "../memory/Buffer.h"
#include "../utils/vulkan.h"
#include "../memory/Image.h"
#include "../app-context/VulkanSwapchain.h"


namespace mcvkp
{

    class Material
    {
    public:
        void* bufferBundle;
        Material(
            const std::string &vertexShaderPath,
            const std::string &fragmentShaderPath);

        ~Material();

        void addTexture(const std::shared_ptr<Texture> &texture);

        void addBufferBundle(const std::shared_ptr<BufferBundle> &bufferBundle);

        // Initialize material when adding to a scene.
        void init(const VkRenderPass &renderPass);

        void bind(VkCommandBuffer &commandBuffer, size_t currentFrame);

    private:
        void __initDescriptorSetLayout();
        void __initDescriptorPool();
        void __initDescriptorSets();
        void __initPipeline(
            const VkExtent2D &swapChainExtent,
            const VkRenderPass &renderPass,
            std::string vertexShaderPath,
            std::string fragmentShaderPath);

    private:
        std::vector<std::shared_ptr<BufferBundle> > m_bufferBundles;
        std::vector<std::shared_ptr<Texture> > m_textures;

        std::string m_vertexShaderPath;
        std::string m_fragmentShaderPath;

        bool m_initialized;

        uint32_t m_descriptorSetsSize;

        VkPipelineLayout m_pipelineLayout;
        VkPipeline m_pipeline;

        VkDescriptorPool m_descriptorPool;
        std::vector<VkDescriptorSet> m_descriptorSets;
        VkDescriptorSetLayout m_descriptorSetLayout;
    };
}
