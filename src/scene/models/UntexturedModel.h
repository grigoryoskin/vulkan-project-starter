#ifndef VULKAN_UNTEXTURED_MODEL_H
#define VULKAN_UNTEXTURED_MODEL_H

class UntexturedVulkanModel {
    public:
        VkDescriptorPool descriptorPool;
        std::vector<VkDescriptorSet> descriptorSets;

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;

        Mesh mesh;

        void init(VulkanApplicationContext *context,
                  VkDescriptorSetLayout *descriptorSetLayout,
                  int swapChainSize,
                  std::string modelPath,
                  std::vector<VkBuffer> *sharedUniformBuffers,
                  std::vector<VkDeviceMemory> *sharedUniformBuffersMemory) {
            this->context = context; 
            this->swapChainSize = swapChainSize;
            this->descriptorSetLayout = descriptorSetLayout;
            mesh = Mesh(modelPath);
            initVertexBuffer();
            initIndexBuffer();
            this->sharedUniformBuffers = sharedUniformBuffers;
            this->sharedUniformBuffersMemory = sharedUniformBuffersMemory;
            initDescriptorPool();
            initDescriptorSets();
        }

        void destroy() {
            vkDestroyBuffer(context->device, vertexBuffer, nullptr);
            vkFreeMemory(context->device, vertexBufferMemory, nullptr);
            vkDestroyBuffer(context->device, indexBuffer, nullptr);
            vkFreeMemory(context->device, indexBufferMemory, nullptr);

            vkDestroyDescriptorPool(context->device, descriptorPool, nullptr);
        }

    private: 
        VulkanApplicationContext *context;
        VkDescriptorSetLayout *descriptorSetLayout;
        std::vector<VkBuffer> *sharedUniformBuffers;
        std::vector<VkDeviceMemory> *sharedUniformBuffersMemory;
        int swapChainSize;

        void initVertexBuffer() {
            VulkanBuffer::createVertexBuffer( *context, mesh.vertices, vertexBuffer, vertexBufferMemory);
        }

        void initIndexBuffer() {
            VulkanBuffer::createIndexBuffer( *context, mesh.indices, indexBuffer, indexBufferMemory);
        }

        void initDescriptorPool() {
            std::array<VkDescriptorPoolSize, 1> poolSizes{};
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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
                VkDescriptorBufferInfo sharedBufferInfo{};
                sharedBufferInfo.buffer = (*sharedUniformBuffers)[i];
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
                
                vkUpdateDescriptorSets(context->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
            }
        }
};

#endif