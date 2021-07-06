#pragma once

#include <string>
#include <vector>
#include "../utils/vulkan.h"
#include "DrawableModel.h"
#include "../memory/VulkanBuffer.h"
#include "../memory/VulkanImage.h"

class TexturedVulkanModel: public DrawableModel{
    public:
        std::vector<VulkanMemory::VulkanBuffer<UniformBufferObject> > uniformBuffers;
        
        uint32_t mipLevels;
        VulkanImage::VulkanImage textureImage;
        VkSampler textureSampler;

        void init(VkDescriptorSetLayout *descriptorSetLayout,
                  std::string modelPath,
                  std::string texturePath,
                  std::vector<VulkanMemory::VulkanBuffer<SharedUniformBufferObject> >* sharedUniformBuffers) {
            this->descriptorSetsSize = VulkanGlobal::context.swapChainImageCount;
            this->sharedUniformBuffers = sharedUniformBuffers;
            this->descriptorSetLayout = descriptorSetLayout;
            initTextureImage(texturePath);
            initTextureSampler();
            mesh = Mesh(modelPath);
            initVertexBuffer();
            initIndexBuffer();    
            initUniformBuffers();
            initDescriptorPool();
            initDescriptorSets();
        }

        void destroy() {
            vkDestroySampler(VulkanGlobal::context.device, textureSampler, nullptr);
            textureImage.destroy();

            vertexBuffer.destroy();
            indexBuffer.destroy();
            for (size_t i = 0; i < descriptorSetsSize; i++) {
                uniformBuffers[i].destroy();
            }

            vkDestroyDescriptorPool(VulkanGlobal::context.device, descriptorPool, nullptr);
        }

        void updateUniformBuffer(UniformBufferObject &ubo, uint32_t currentImage) {
            VkDeviceSize bufferSize = sizeof(ubo);

            void* data;
	        vmaMapMemory(VulkanGlobal::context.allocator, uniformBuffers[currentImage].allocation, &data);
	        memcpy(data, &ubo, bufferSize);
	        vmaUnmapMemory(VulkanGlobal::context.allocator, uniformBuffers[currentImage].allocation);
        }

    private:
        std::vector<VulkanMemory::VulkanBuffer<SharedUniformBufferObject> >* sharedUniformBuffers;
        int descriptorSetsSize;
        
        void initTextureImage(std::string texturePath) {
            VulkanImage::createTextureImage(texturePath, textureImage, mipLevels);
        }

        void initTextureSampler() {
            VulkanImage::createTextureSampler(textureSampler, mipLevels);
        }

        void initDescriptorPool() {
            std::array<VkDescriptorPoolSize, 3> poolSizes{};
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSizes[0].descriptorCount = static_cast<uint32_t>(descriptorSetsSize);
            poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSizes[1].descriptorCount = static_cast<uint32_t>(descriptorSetsSize);
            poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSizes[2].descriptorCount = static_cast<uint32_t>(descriptorSetsSize);

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = static_cast<uint32_t>(descriptorSetsSize);

            if (vkCreateDescriptorPool(VulkanGlobal::context.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
                throw std::runtime_error("failed to create descriptor pool!");
            }
        }

        void initDescriptorSets() {
            std::vector<VkDescriptorSetLayout> layouts(descriptorSetsSize, *descriptorSetLayout);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetsSize);
            allocInfo.pSetLayouts = layouts.data();

            descriptorSets.resize(descriptorSetsSize);
            if (vkAllocateDescriptorSets(VulkanGlobal::context.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate descriptor sets!");
            }

            for (size_t i = 0; i < descriptorSetsSize; i++) {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = uniformBuffers[i].buffer;
                bufferInfo.offset = 0;
                bufferInfo.range = sizeof(UniformBufferObject);

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = textureImage.imageView;
                imageInfo.sampler = textureSampler;

                VkDescriptorBufferInfo sharedBufferInfo{};
                sharedBufferInfo.buffer = (*sharedUniformBuffers)[i].buffer;
                sharedBufferInfo.offset = 0;
                sharedBufferInfo.range = sizeof(SharedUniformBufferObject);

                std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].dstSet = descriptorSets[i];
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pBufferInfo = &bufferInfo;

                descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[1].dstSet = descriptorSets[i];
                descriptorWrites[1].dstBinding = 1;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pImageInfo = &imageInfo;
                
                descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[2].dstSet = descriptorSets[i];
                descriptorWrites[2].dstBinding = 2;
                descriptorWrites[2].dstArrayElement = 0;
                descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[2].descriptorCount = 1;
                descriptorWrites[2].pBufferInfo = &sharedBufferInfo;

                vkUpdateDescriptorSets(VulkanGlobal::context.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
            }
        }

        void initUniformBuffers() {
            VkDeviceSize bufferSize = sizeof(UniformBufferObject);
            uniformBuffers.resize(descriptorSetsSize);

            for (size_t i = 0; i < descriptorSetsSize; i++) {
                uniformBuffers[i].allocate(bufferSize,
                                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                           VMA_MEMORY_USAGE_CPU_TO_GPU);
            }
        }
};
