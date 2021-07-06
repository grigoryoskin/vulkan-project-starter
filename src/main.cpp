#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <array>
#include "utils/vulkan.h"
#include "app-context/VulkanApplicationContext.h"
#include "app-context/VulkanGlobal.h"
#include "app-context/VulkanSwapchain.h"
#include "utils/RootDir.h"
#include "memory/VulkanBuffer.h"
#include "utils/glm.h"
#include "utils/Camera.h"
#include "scene/mesh.h"
#include "scene/TexturedModel.h"
#include "scene/UntexturedModel.h"
#include "scene/ScreenQuadModel.h"
#include "render-context/OffscreenRenderContext.h"
#include "render-context/PostProcessRenderContext.h"
#include "pipeline/VulkanPipeline.h"
#include "pipeline/VulkanDescriptorSet.h"
// TODO: Organize includes!

const std::string path_prefix = std::string(ROOT_DIR) + "resources/";

float mouseOffsetX, mouseOffsetY;
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow *window);

float deltaTime = 0.0f; // Time between current frame and last frame
float lastFrame = 0.0f; // Time of last frame
Camera camera(glm::vec3(3.0f, 1.0f, 0.0f));

const int MAX_FRAMES_IN_FLIGHT = 2;

class HelloDogApplication {
public:
    void run() {
        initVulkan();
        mainLoop();
        cleanup();
    }
    
private:
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
    std::vector<VulkanMemory::VulkanBuffer<SharedUniformBufferObject> > sharedUniformBuffers;
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
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    // Fences to keep track of the images currently in the graphics queue.
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;

    // Initializing layouts and models.
    void initScene() {
        VulkanDescriptorSet::singeTextureLayout(singleTextureDescriptorLayout);
        VulkanDescriptorSet::untexturedLayout(lightCubeDescriptorLayout);
        VulkanDescriptorSet::screenQuadLayout(screenQuadDescriptorLayout);
        
        VkDeviceSize bufferSize = sizeof(SharedUniformBufferObject);
        sharedUniformBuffers.resize(swapchainContext.swapChainImageViews.size());

        for (size_t i = 0; i < swapchainContext.swapChainImageViews.size(); i++) {
            sharedUniformBuffers[i].allocate(bufferSize,
                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                             VMA_MEMORY_USAGE_CPU_TO_GPU);
        }
        
        dogeModel.init(&singleTextureDescriptorLayout,
                       path_prefix + "/models/buffDoge.obj",
                       path_prefix + "/textures/Doge",
                       &sharedUniformBuffers);
        cheemsModel.init(&singleTextureDescriptorLayout,
                         path_prefix + "/models/cheems.obj",
                         path_prefix + "/textures/Cheems",
                         &sharedUniformBuffers);
        dogModels.push_back(dogeModel);
        dogModels.push_back(cheemsModel);

        VulkanPipeline::createGraphicsPipeline(swapchainContext.swapChainExtent,
                                               &singleTextureDescriptorLayout,
                                               offscreenRenderContext.renderPass,
                                               path_prefix + "/shaders/generated/textured-vert.spv",
                                               path_prefix + "/shaders/generated/textured-frag.spv",
                                               texturedModelPipelineLayout,
                                               texturedModelPipeline);

        lightCubeModel.init(&lightCubeDescriptorLayout,
                            path_prefix + "/models/cube.obj",
                            &sharedUniformBuffers);

        VulkanPipeline::createGraphicsPipeline(swapchainContext.swapChainExtent,
                                               &lightCubeDescriptorLayout,
                                               offscreenRenderContext.renderPass,
                                               path_prefix + "/shaders/generated/untextured-vert.spv",
                                               path_prefix + "/shaders/generated/untextured-frag.spv",
                                               lightCubePipelineLayout,
                                               lightCubePipeline);

        // Creating screen quad and passing color attachment of offscreen render pass as a texture.
        screenQuadModel.init(&screenQuadDescriptorLayout,
                             &offscreenRenderContext.colorImage);          

        VulkanPipeline::createGraphicsPipeline(swapchainContext.swapChainExtent,
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
        vmaMapMemory(VulkanGlobal::context.allocator, sharedUniformBuffers[currentImage].allocation, &data);
        memcpy(data, &sharedUbo, bufferSize);
        vmaUnmapMemory(VulkanGlobal::context.allocator, sharedUniformBuffers[currentImage].allocation);
    }

    void createCommandBuffers() {
        commandBuffers.resize(postProcessRenderContext.swapChainFramebuffers.size());
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = VulkanGlobal::context.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(VulkanGlobal::context.device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
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
                model.drawCommand(commandBuffers[i], texturedModelPipelineLayout, i);
            }

            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, lightCubePipeline);
            lightCubeModel.drawCommand(commandBuffers[i], lightCubePipelineLayout, i);
            
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
            screenQuadModel.drawCommand(commandBuffers[i], screenQuadPipelineLayout, 0);
            
            vkCmdEndRenderPass(commandBuffers[i]);
            if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(swapchainContext.swapChainImageViews.size());

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(VulkanGlobal::context.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(VulkanGlobal::context.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(VulkanGlobal::context.device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    size_t currentFrame = 0;
    void drawFrame() {
        vkWaitForFences(VulkanGlobal::context.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(VulkanGlobal::context.device, swapchainContext.swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // Check if a previous frame is using this image (i.e. there is its fence to wait on)
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(VulkanGlobal::context.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        // Mark the image as now being in use by this frame
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        updateScene(imageIndex);
    
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(VulkanGlobal::context.device, 1, &inFlightFences[currentFrame]);


        if (vkQueueSubmit(VulkanGlobal::context.graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
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

        result = vkQueuePresentKHR(VulkanGlobal::context.presentQueue, &presentInfo);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        // Commented this out for playing around with it later :)
        //vkQueueWaitIdle(VulkanGlobal::context.presentQueue);
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    int nbFrames = 0;
    float lastTime = 0;
    void mainLoop() {
        while (!glfwWindowShouldClose(VulkanGlobal::context.window)) {
            float currentTime = (float)glfwGetTime();
            deltaTime = currentTime - lastFrame;
            nbFrames++;
            if ( currentTime - lastTime >= 1.0 ){ // If last prinf() was more than 1 sec ago
                // printf and reset timer
                printf("%f ms/frame\n", 1000.0/double(nbFrames));
                nbFrames = 0;
                lastTime = currentTime;
            }
            lastFrame = currentTime;

            processInput(VulkanGlobal::context.window);
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(VulkanGlobal::context.device);
    }

    void initVulkan() {
        swapchainContext.init();
        offscreenRenderContext.init(&swapchainContext);
        postProcessRenderContext.init(&swapchainContext);

        initScene();
        
        createCommandBuffers();
        createSyncObjects();
        glfwSetCursorPosCallback(VulkanGlobal::context.window, mouse_callback);
    }

    void cleanupSwapChain() {
        vkFreeCommandBuffers(VulkanGlobal::context.device, VulkanGlobal::context.commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

        for (size_t i = 0; i < swapchainContext.swapChainImageViews.size(); i++) {
            sharedUniformBuffers[i].destroy();
        }

        vkDestroyPipeline(VulkanGlobal::context.device, texturedModelPipeline, nullptr);
        vkDestroyPipelineLayout(VulkanGlobal::context.device, texturedModelPipelineLayout, nullptr);
        vkDestroyPipeline(VulkanGlobal::context.device, lightCubePipeline, nullptr);
        vkDestroyPipelineLayout(VulkanGlobal::context.device, lightCubePipelineLayout, nullptr);
        vkDestroyPipeline(VulkanGlobal::context.device, screenQuadPipeline, nullptr);
        vkDestroyPipelineLayout(VulkanGlobal::context.device, screenQuadPipelineLayout, nullptr);
        
        offscreenRenderContext.destroy();
        postProcessRenderContext.destroy();
        swapchainContext.destroy();
    }

    void cleanup() {
        cleanupSwapChain();

        vkDestroyDescriptorSetLayout(VulkanGlobal::context.device, singleTextureDescriptorLayout, nullptr);
        dogeModel.destroy();
        cheemsModel.destroy();
        vkDestroyDescriptorSetLayout(VulkanGlobal::context.device, lightCubeDescriptorLayout, nullptr);
        lightCubeModel.destroy();
        vkDestroyDescriptorSetLayout(VulkanGlobal::context.device, screenQuadDescriptorLayout, nullptr);
        screenQuadModel.destroy();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(VulkanGlobal::context.device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(VulkanGlobal::context.device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(VulkanGlobal::context.device, inFlightFences[i], nullptr);

        }

        //delete VulkanGlobal::context;
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

