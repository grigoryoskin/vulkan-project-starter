#ifndef VULKAN_SCREEN_QUAD_MODEL_H
#define VULKAN_SCREEN_QUAD_MODEL_H

class ScreenQuadVulkanModel {
    public:
        VkDescriptorPool descriptorPool;
        std::vector<VkDescriptorSet> descriptorSets;

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;

        VkSampler textureSampler;

        Mesh mesh;
        
        void init(VulkanApplicationContext *context,
                  VkDescriptorSetLayout *descriptorSetLayout,
                  int swapChainSize,
                  VkImage *textureImage,
                  VkDeviceMemory *textureImageMemory,
                  VkImageView *textureImageView
            ) {
           this->context = context;      
           this->swapChainSize = swapChainSize;
           this->textureImage = textureImage;
           this->textureImageMemory = textureImageMemory;
           this->textureImageView = textureImageView;
           this->descriptorSetLayout = descriptorSetLayout;
           initMesh();
           initVertexBuffer(); 
           initIndexBuffer();               
           initTextureSampler();
           initDescriptorPool();
           initDescriptorSets();
        }

        void destroy() {
            vkDestroySampler(context->device, textureSampler, nullptr);

            vkDestroyBuffer(context->device, vertexBuffer, nullptr);
            vkFreeMemory(context->device, vertexBufferMemory, nullptr);
        
            vkDestroyBuffer(context->device, indexBuffer, nullptr);
            vkFreeMemory(context->device, indexBufferMemory, nullptr);

            vkDestroyDescriptorPool(context->device, descriptorPool, nullptr);
        }

    private: 
        VulkanApplicationContext *context;
        VkDescriptorSetLayout *descriptorSetLayout;
        
        VkImage *textureImage;
        VkDeviceMemory *textureImageMemory;
        VkImageView *textureImageView;
        uint32_t mipLevels = 1;

        int swapChainSize;
        void initMesh() {
            float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        };

        for (int i = 0; i<4; i++) {
            Vertex v{};
            v.pos = glm::vec3(quadVertices[5*i], quadVertices[5*i+1], quadVertices[5*i+2]);
            v.normal = glm::vec3(0.0f, 0.0f, 0.0f);
            v.texCoord = glm::vec2(quadVertices[5*i+3], quadVertices[5*i+4]);
            this->mesh.vertices.push_back(v);
        }
        
        this->mesh.indices  = { 0, 3, 2, 2, 1, 0 };
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
            std::array<VkDescriptorPoolSize, 1> poolSizes{};
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainSize);

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
                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = *textureImageView;
                imageInfo.sampler = textureSampler;

                std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].dstSet = descriptorSets[i];
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pImageInfo = &imageInfo;

                vkUpdateDescriptorSets(context->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
            }
        }
};

#endif