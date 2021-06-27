#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>
#include <cstdint> // Necessary for UINT32_MAX
#include <algorithm> // Necessary for std::min/std::max
#include <fstream>
#include <array>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>
#include "app-context/VulkanApplicationContext.h"
#include "scene/models/mesh.h"
#include "utils/CommandUtils.h"
#include "utils/RootDir.h"
#include "utils/readfile.h"
#include "utils/BufferUtils.h"
#include "utils/ImageUtils.h"
#include "utils/Camera.h"
#include "app-context/VulkanSwapchain.h"
#include "scene/models/TexturedModel.h"
#include "scene/models/UntexturedModel.h"
#include "render-context/OffscreenRenderContext.h"
#include "pipeline/VulkanPipeline.h"
#include "pipeline/VulkanDescriptorSet.h"
#include "render-context/PostProcessRenderContext.h"
#include "scene/models/ScreenQuadModel.h"
// TODO: Organize includes!

const std::string path_prefix = std::string(ROOT_DIR) + "resources/";

float mouseOffsetX, mouseOffsetY;
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow *window);

float deltaTime = 0.0f; // Time between current frame and last frame
float lastFrame = 0.0f; // Time of last frame
Camera camera(glm::vec3(3.0f, 1.0f, 0.0f));

class HelloDogApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
    
private:
    GLFWwindow* window;

    // Application context - manages device, surface, queues and command pool.
    VulkanApplicationContext appContext;
    // Swapchain context - holds swapchain and its images and image views.
    VulkanSwapchain swapchainContext;

    /**
     * MAIN SCENE
     * 
     * 2 models sharing a pipeline, 1 model with it's own pipeline.
     * Rendered to a texture with an offscreen render pass. 
     */

    // Offscreen render context for rendering the main scene. Has one framebuffer attached to color and depth image.
    OffscreenRenderContext offscreenRenderContext;

    // Descriptor layouts for models in the main scene. 
    VkDescriptorSetLayout singleTextureDescriptorLayout;
    VkDescriptorSetLayout lightCubeDescriptorLayout;

    // Pipelines for models in the main scene. 
    VkPipelineLayout texturedModelPipelineLayout;
    VkPipeline texturedModelPipeline;
    VkPipelineLayout lightCubePipelineLayout;
    VkPipeline lightCubePipeline;

    // Models in the main scene.
    // Dog models.
    TexturedVulkanModel dogeModel;
    TexturedVulkanModel cheemsModel;
    std::vector<TexturedVulkanModel> dogModels;
    // Light cube model.
    UntexturedVulkanModel lightCubeModel;

    // Uniform buffers for models in the main scene.
    std::vector<VkBuffer> sharedUniformBuffers;
    std::vector<VkDeviceMemory> sharedUniformBuffersMemory;
    SharedUniformBufferObject sharedUbo{};
    UniformBufferObject dogeUbo{};
    UniformBufferObject cheemsUbo{};

    /**
     * POST PROCESS
     * 
     * Main scene rendered to a texture gets displayed on a screen quad with some processing done in the fragment shader.
     */

    // Post process render pass, has framebuffer for each swapchain image.
    PostProcessRenderContext postProcessRenderContext;

    // Layout for screen quad. Basically just a texture sampler for acessing rendered image.
    VkDescriptorSetLayout screenQuadDescriptorLayout;
    VkPipelineLayout screenQuadPipelineLayout;
    VkPipeline screenQuadPipeline;
    //Screen quad model - just a quad covering the screen.
    ScreenQuadVulkanModel screenQuadModel;

    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;

    // Initializing layouts and models.
    void initScene() {
        VulkanDescriptorSet::singeTextureLayout(appContext, singleTextureDescriptorLayout);
        VulkanDescriptorSet::untexturedLayout(appContext, lightCubeDescriptorLayout);
        VulkanDescriptorSet::screenQuadLayout(appContext, screenQuadDescriptorLayout);
        
        VkDeviceSize bufferSize = sizeof(SharedUniformBufferObject);
        sharedUniformBuffers.resize(swapchainContext.swapChainImageViews.size());
        sharedUniformBuffersMemory.resize(swapchainContext.swapChainImageViews.size());

        for (size_t i = 0; i < swapchainContext.swapChainImageViews.size(); i++) {
            VulkanBuffer::createBuffer(
                appContext,
                bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                sharedUniformBuffers[i],
                sharedUniformBuffersMemory[i]);
        }
        dogeModel.init(&appContext,
                      &singleTextureDescriptorLayout,
                      swapchainContext.swapChainImages.size(),
                      path_prefix + "/models/buffDoge.obj",
                      path_prefix + "/textures/Doge",
                      &sharedUniformBuffers,
                      &sharedUniformBuffersMemory);
        cheemsModel.init(&appContext,
                         &singleTextureDescriptorLayout,
                         swapchainContext.swapChainImages.size(),
                         path_prefix + "/models/cheems.obj",
                         path_prefix + "/textures/Cheems",
                         &sharedUniformBuffers,
                         &sharedUniformBuffersMemory);
        dogModels.push_back(dogeModel);
        dogModels.push_back(cheemsModel);

        VulkanPipeline::createGraphicsPipeline(appContext,
                                               swapchainContext.swapChainExtent,
                                               &singleTextureDescriptorLayout,
                                               offscreenRenderContext.renderPass,
                                               path_prefix + "/shaders/generated/textured-vert.spv",
                                               path_prefix + "/shaders/generated/textured-frag.spv",
                                               texturedModelPipelineLayout,
                                               texturedModelPipeline);

        lightCubeModel.init(&appContext,
                            &lightCubeDescriptorLayout,
                            swapchainContext.swapChainImages.size(),
                            path_prefix + "/models/cube.obj",
                            &sharedUniformBuffers,
                            &sharedUniformBuffersMemory);

        VulkanPipeline::createGraphicsPipeline(appContext,
                                               swapchainContext.swapChainExtent,
                                               &lightCubeDescriptorLayout,
                                               offscreenRenderContext.renderPass,
                                               path_prefix + "/shaders/generated/untextured-vert.spv",
                                               path_prefix + "/shaders/generated/untextured-frag.spv",
                                               lightCubePipelineLayout,
                                               lightCubePipeline);

        // Creating screen quad and passing color attachment of offscreen rendre pass as a texture.
        screenQuadModel.init(&appContext,
                            &screenQuadDescriptorLayout,
                            swapchainContext.swapChainImages.size(),
                            &offscreenRenderContext.colorImage,
                            &offscreenRenderContext.colorImageMemory,
                            &offscreenRenderContext.colorImageView);          

        VulkanPipeline::createGraphicsPipeline(appContext,
                                               swapchainContext.swapChainExtent,
                                               &screenQuadDescriptorLayout,
                                               postProcessRenderContext.renderPass,
                                               path_prefix + "/shaders/generated/post-process-vert.spv",
                                               path_prefix + "/shaders/generated/post-process-frag.spv",
                                               screenQuadPipelineLayout,
                                               screenQuadPipeline);

        
    }

    void updateScene(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        
        dogeUbo.model = glm::mat4(1.0f); 
        
        dogeModel.updateUniformBuffer(dogeUbo, currentImage);
        cheemsModel.updateUniformBuffer(dogeUbo, currentImage);

        sharedUbo.view = camera.GetViewMatrix();
        sharedUbo.proj = glm::perspective(glm::radians(45.0f), WIDTH / (float) HEIGHT, 0.1f, 10.0f);
        sharedUbo.proj[1][1] *= -1;
        sharedUbo.lightPos = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::vec4(1.0f,1.0f,1.0f,0.0f);
        VkDeviceSize bufferSize = sizeof(sharedUbo);

        void* data;
        vkMapMemory(appContext.device, sharedUniformBuffersMemory[currentImage], 0, bufferSize, 0, &data);
        memcpy(data, &sharedUbo, (size_t) bufferSize);
        vkUnmapMemory(appContext.device, sharedUniformBuffersMemory[currentImage]);
    }

    void createCommandBuffers() {
        commandBuffers.resize(postProcessRenderContext.swapChainFramebuffers.size());
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = appContext.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(appContext.device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        for (size_t i = 0; i < commandBuffers.size(); i++) {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0; // Optional
            beginInfo.pInheritanceInfo = nullptr; // Optional

            if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = offscreenRenderContext.renderPass;
            renderPassInfo.framebuffer = offscreenRenderContext.framebuffer;
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = swapchainContext.swapChainExtent;
            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = {1.0f, 0.5f, 1.0f, 1.0f};
            clearValues[1].depthStencil = {1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();
            
            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, texturedModelPipeline);

            for(TexturedVulkanModel model : dogModels) {
                VkBuffer vertexBuffers[] = {model.vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffers[i], model.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, texturedModelPipelineLayout, 0, 1, &model.descriptorSets[i], 0, nullptr);
                vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(model.mesh.indices.size()), 1, 0, 0, 0);
            }

            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, lightCubePipeline);

            VkBuffer vertexBuffers[] = {lightCubeModel.vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffers[i], lightCubeModel.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, lightCubePipelineLayout, 0, 1, &lightCubeModel.descriptorSets[i], 0, nullptr);
            vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(lightCubeModel.mesh.indices.size()), 1, 0, 0, 0);
            
            vkCmdEndRenderPass(commandBuffers[i]);

            VkRenderPassBeginInfo postProcessRenderPassInfo{};
            postProcessRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            postProcessRenderPassInfo.renderPass = postProcessRenderContext.renderPass;
            postProcessRenderPassInfo.framebuffer = postProcessRenderContext.swapChainFramebuffers[i];
            postProcessRenderPassInfo.renderArea.offset = {0, 0};
            postProcessRenderPassInfo.renderArea.extent = swapchainContext.swapChainExtent;

            postProcessRenderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            postProcessRenderPassInfo.pClearValues = clearValues.data();
            vkCmdBeginRenderPass(commandBuffers[i], &postProcessRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, screenQuadPipeline);
            VkBuffer screenQuadVertexBuffers[] = {screenQuadModel.vertexBuffer};
            VkDeviceSize screenQuadoffsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, screenQuadVertexBuffers, screenQuadoffsets);
            vkCmdBindIndexBuffer(commandBuffers[i], screenQuadModel.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, screenQuadPipelineLayout, 0, 1, &screenQuadModel.descriptorSets[i], 0, nullptr);
            vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(screenQuadModel.mesh.indices.size()), 1, 0, 0, 0);

            vkCmdEndRenderPass(commandBuffers[i]);
            if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    void createSemaphores() {
            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            if (vkCreateSemaphore(appContext.device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
                vkCreateSemaphore(appContext.device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS) {

                throw std::runtime_error("failed to create semaphores!");
            }
    }

    void drawFrame() {
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(appContext.device, swapchainContext.swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateScene(imageIndex);
    
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        if (vkQueueSubmit(appContext.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = {swapchainContext.swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;

        result = vkQueuePresentKHR(appContext.presentQueue, &presentInfo);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        vkQueueWaitIdle(appContext.presentQueue);
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            float currentTime = (float)glfwGetTime();
            deltaTime = currentTime - lastFrame;
            lastFrame = currentTime;

            processInput(window);
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(appContext.device);
    }

    void initWindow() {
            glfwInit();
            // This tells glfw not to use opengl.
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        
            window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        appContext.init(window);
        swapchainContext.init(&appContext);
        offscreenRenderContext.init(&appContext, &swapchainContext);
        postProcessRenderContext.init(&appContext, &swapchainContext);

        initScene();
        
        createCommandBuffers();
        createSemaphores();
        glfwSetCursorPosCallback(window, mouse_callback);
    }

    void cleanupSwapChain() {
        vkFreeCommandBuffers(appContext.device, appContext.commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

        for (size_t i = 0; i < swapchainContext.swapChainImageViews.size(); i++) {
                vkDestroyBuffer(appContext.device, sharedUniformBuffers[i], nullptr);
                vkFreeMemory(appContext.device, sharedUniformBuffersMemory[i], nullptr);
        }
    
        vkDestroyPipeline(appContext.device, texturedModelPipeline, nullptr);
        vkDestroyPipelineLayout(appContext.device, texturedModelPipelineLayout, nullptr);
        vkDestroyPipeline(appContext.device, lightCubePipeline, nullptr);
        vkDestroyPipelineLayout(appContext.device, lightCubePipelineLayout, nullptr);
        vkDestroyPipeline(appContext.device, screenQuadPipeline, nullptr);
        vkDestroyPipelineLayout(appContext.device, screenQuadPipelineLayout, nullptr);
        
        offscreenRenderContext.destroy();
        postProcessRenderContext.destroy();
        swapchainContext.destroy();
    }

    void cleanup() {
        cleanupSwapChain();

        vkDestroyDescriptorSetLayout(appContext.device, singleTextureDescriptorLayout, nullptr);
        dogeModel.destroy();
        cheemsModel.destroy();
        vkDestroyDescriptorSetLayout(appContext.device, lightCubeDescriptorLayout, nullptr);
        lightCubeModel.destroy();
        vkDestroyDescriptorSetLayout(appContext.device, screenQuadDescriptorLayout, nullptr);
        screenQuadModel.destroy();

        vkDestroySemaphore(appContext.device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(appContext.device, imageAvailableSemaphore, nullptr);

        appContext.destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
    }

};

int main() {
    HelloDogApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

float lastX = 400, lastY = 300;
bool firstMouse = true;
void mouse_callback(GLFWwindow* window, double xpos, double ypos){
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

