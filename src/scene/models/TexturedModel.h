#ifndef VULKAN_SIMPLE_MODEL_H
#define VULKAN_SIMPLE_MODEL_H

class TexturedVulkanModel {
    public:
        VkDescriptorPool descriptorPool;
        std::vector<VkDescriptorSet> descriptorSets;

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        std::vector<VkBuffer> uniformBuffers;
        std::vector<VkDeviceMemory> uniformBuffersMemory;

        uint32_t mipLevels;
        VkImage textureImage;
        VkDeviceMemory textureImageMemory;
        VkImageView textureImageView;
        VkSampler textureSampler;

        Mesh mesh;

        void init(VulkanApplicationContext *context,
                  VkDescriptorSetLayout *descriptorSetLayout,
                  int swapChainSize,
                  std::string modelPath,
                  std::string texturePath,
                  std::vector<VkBuffer> *sharedUniformBuffers,
                  std::vector<VkDeviceMemory> *sharedUniformBuffersMemory) {
            this->context = context; 
            this->swapChainSize = swapChainSize;
            this->sharedUniformBuffers = sharedUniformBuffers;
            this->sharedUniformBuffersMemory = sharedUniformBuffersMemory;
            this->descriptorSetLayout = descriptorSetLayout;
            initTextureImage(texturePath);
            initTextureImageView();
            initTextureSampler();
            mesh = Mesh(modelPath);
            initVertexBuffer();
            initIndexBuffer();    
            initUniformBuffers();
            initDescriptorPool();
            initDescriptorSets();
        }

        void destroy() {
            vkDestroySampler(context->device, textureSampler, nullptr);
            vkDestroyImageView(context->device, textureImageView, nullptr);
            vkDestroyImage(context->device, textureImage, nullptr);
            vkFreeMemory(context->device, textureImageMemory, nullptr);

            vkDestroyBuffer(context->device, vertexBuffer, nullptr);
            vkFreeMemory(context->device, vertexBufferMemory, nullptr);
            vkDestroyBuffer(context->device, indexBuffer, nullptr);
            vkFreeMemory(context->device, indexBufferMemory, nullptr);

            for (size_t i = 0; i < swapChainSize; i++) {
                vkDestroyBuffer(context->device, uniformBuffers[i], nullptr);
                vkFreeMemory(context->device, uniformBuffersMemory[i], nullptr);
            }
            vkDestroyDescriptorPool(context->device, descriptorPool, nullptr);
        }

        void updateUniformBuffer(UniformBufferObject &ubo, uint32_t currentImage) {
            VkDeviceSize bufferSize = sizeof(ubo);

            void* data;
            vkMapMemory(context->device, uniformBuffersMemory[currentImage], 0, bufferSize, 0, &data);
            memcpy(data, &ubo, (size_t) bufferSize);
            vkUnmapMemory(context->device, uniformBuffersMemory[currentImage]);
        }
    private: 
        VulkanApplicationContext *context;
        VkDescriptorSetLayout *descriptorSetLayout;
        std::vector<VkBuffer> *sharedUniformBuffers;
        std::vector<VkDeviceMemory> *sharedUniformBuffersMemory;
        int swapChainSize;
        

        void initTextureImage(std::string texturePath) {
            VulkanImage::createTextureImage( *context, texturePath, textureImage, textureImageMemory, mipLevels);
        }

        void initTextureImageView() {
            VulkanImage::createTextureImageView( *context, textureImage, textureImageView, mipLevels);
        }

        void initTextureSampler() {
            VulkanImage::createTextureSampler( *context, textureSampler, mipLevels);
        }

        void initVertexBuffer() {
            VulkanBuffer::createVertexBuffer( *context, mesh.vertices, vertexBuffer, vertexBufferMemory);
        }

        void initIndexBuffer() {
            VulkanBuffer::createIndexBuffer( *context, mesh.indices, indexBuffer, indexBufferMemory);
        }

        void initDescriptorPool() {
            std::array<VkDescriptorPoolSize, 3> poolSizes{};
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainSize);
            poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSizes[1].descriptorCount = static_cast<uint32_t>(swapChainSize);
            poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSizes[2].descriptorCount = static_cast<uint32_t>(swapChainSize);

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = static_cast<uint32_t>(swapChainSize);

            if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
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
            if (vkAllocateDescriptorSets(context->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate descriptor sets!");
            }

            for (size_t i = 0; i < swapChainSize; i++) {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = uniformBuffers[i];
                bufferInfo.offset = 0;
                bufferInfo.range = sizeof(UniformBufferObject);

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = textureImageView;
                imageInfo.sampler = textureSampler;

                VkDescriptorBufferInfo sharedBufferInfo{};
                sharedBufferInfo.buffer = (*sharedUniformBuffers)[i];
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

                vkUpdateDescriptorSets(context->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
            }
        }

        void initUniformBuffers() {
            VkDeviceSize bufferSize = sizeof(UniformBufferObject);
            uniformBuffers.resize(swapChainSize);
            uniformBuffersMemory.resize(swapChainSize);

            for (size_t i = 0; i < swapChainSize; i++) {
                VulkanBuffer::createBuffer(*context, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
            }
        }
};

#endif