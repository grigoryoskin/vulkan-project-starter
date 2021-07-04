#pragma once

#include <vector>
#include <string>
#include "../utils/vulkan.h"
#include "DrawableModel.h"
#include "../memory/VulkanBuffer.h"

class UntexturedVulkanModel: public DrawableModel {
    public:
        void init(VkDescriptorSetLayout *descriptorSetLayout,
                  int swapChainSize,
                  std::string modelPath,
                  std::vector<VulkanMemory::VulkanBuffer<SharedUniformBufferObject> >* sharedUniformBuffers) {
            this->swapChainSize = swapChainSize;
            this->descriptorSetLayout = descriptorSetLayout;
            mesh = Mesh(modelPath);
            initVertexBuffer();
            initIndexBuffer();
            this->sharedUniformBuffers = sharedUniformBuffers;
            initDescriptorPool();
            initDescriptorSets();
        }

        void destroy() {
            indexBuffer.destroy();
            vertexBuffer.destroy();
            vkDestroyDescriptorPool(VulkanGlobal::context.device, descriptorPool, nullptr);
        }

    private: 
        std::vector<VulkanMemory::VulkanBuffer<SharedUniformBufferObject> >* sharedUniformBuffers;
        int swapChainSize;

        void initDescriptorPool() {
            std::array<VkDescriptorPoolSize, 1> poolSizes{};
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainSize);

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = static_cast<uint32_t>(swapChainSize);

            if (vkCreateDescriptorPool(VulkanGlobal::context.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
                throw std::runtime_error("failed to create descriptor pool!");
            }
        }

        void initDescriptorSets() {
            std::vector<VkDescriptorSetLayout> layouts(swapChainSize, *descriptorSetLayout);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainSize);
            allocInfo.pSetLayouts = layouts.data();

            descriptorSets.resize(swapChainSize);
            if (vkAllocateDescriptorSets(VulkanGlobal::context.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate descriptor sets!");
            }

            for (size_t i = 0; i < swapChainSize; i++) {
                VkDescriptorBufferInfo sharedBufferInfo{};
                sharedBufferInfo.buffer = (*sharedUniformBuffers)[i].buffer;
                sharedBufferInfo.offset = 0;
                sharedBufferInfo.range = sizeof(SharedUniformBufferObject);

                std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].dstSet = descriptorSets[i];
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pBufferInfo = &sharedBufferInfo;
                
                vkUpdateDescriptorSets(VulkanGlobal::context.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
            }
        }
};
