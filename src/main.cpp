#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <array>
#include <memory>
#include "utils/vulkan.h"
#include "app-context/VulkanApplicationContext.h"
#include "app-context/VulkanSwapchain.h"
#include "app-context/VulkanGlobal.h"
#include "utils/RootDir.h"
#include "memory/Buffer.h"
#include "utils/glm.h"
#include "utils/Camera.h"
#include "scene/Mesh.h"
#include "scene/Scene.h"
#include "scene/DrawableModel.h"
#include "render-context/ForwardRenderPass.h"
#include "render-context/FlatRenderPass.h"
#include "render-context/RenderSystem.h"
#include <thread>

#include "scene/Material.h"
// TODO: Organize includes!

const std::string path_prefix = std::string(ROOT_DIR) + "resources/";

float mouseOffsetX, mouseOffsetY;
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void processInput(GLFWwindow *window);

float deltaTime = 0.0f; // Time between current frame and last frame
float lastFrame = 0.0f; // Time of last frame
Camera camera(glm::vec3(3.0f, 1.0f, 0.0f));

const int MAX_FRAMES_IN_FLIGHT = 2;

/**
 *  This program renders 2 dogs and a light cube using vulkan API.
 */
class HelloDogApplication
{
public:
    void run()
    {
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    // A scene with two dogs and a light.
    std::shared_ptr<mcvkp::Scene> scene;
    // A scene with a screen quad.
    std::shared_ptr<mcvkp::Scene> postProcessScene;

    // UBO shared by all objects. Contains view/projection matrices and a light position.
    SharedUniformBufferObject sharedUbo;
    std::shared_ptr<mcvkp::BufferBundle> sharedUniformBufferBundle;

    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    // Fences to keep track of the images currently in the graphics queue.
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;

    // Initializing models, materials and scenes.
    void initScene()
    {
        using namespace mcvkp;

        scene = std::make_shared<Scene>(RenderPassType::eForward);

        /**
         * Creating buffers.
         */
        uint32_t descriptorSetsSize = VulkanGlobal::swapchainContext.swapChainImageViews.size();
        std::shared_ptr<BufferBundle> dogeBufferBundle = std::make_shared<mcvkp::BufferBundle>(descriptorSetsSize);
        std::shared_ptr<BufferBundle> cheemzBufferBundle = std::make_shared<mcvkp::BufferBundle>(descriptorSetsSize);

        // BufferBundle is a wrapper for a collection of buffers. Used to symultaniously update and render when several frames are in flight.
        BufferUtils::createBundle<UniformBufferObject>(dogeBufferBundle.get(), UniformBufferObject(glm::mat4(2.0f)),
                                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        BufferUtils::createBundle<UniformBufferObject>(cheemzBufferBundle.get(), UniformBufferObject(glm::mat4(1.0f)),
                                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        sharedUniformBufferBundle = std::make_shared<mcvkp::BufferBundle>(descriptorSetsSize);

        BufferUtils::createBundle<SharedUniformBufferObject>(sharedUniformBufferBundle.get(), sharedUbo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                             VMA_MEMORY_USAGE_CPU_TO_GPU);

        /**
         * Creating textures and materials.
         */
        std::shared_ptr<Texture> dogeTex = std::make_shared<Texture>(path_prefix + "/textures/Doge");
        std::shared_ptr<Texture> cheemzTex = std::make_shared<Texture>(path_prefix + "/textures/Cheems");

        std::shared_ptr<Material> dogeMaterial = std::make_shared<Material>(
            path_prefix + "/shaders/generated/textured-vert.spv",
            path_prefix + "/shaders/generated/textured-frag.spv");
        dogeMaterial->addBufferBundle(dogeBufferBundle, VK_SHADER_STAGE_VERTEX_BIT);
        dogeMaterial->addBufferBundle(sharedUniformBufferBundle, VK_SHADER_STAGE_VERTEX_BIT);
        dogeMaterial->addTexture(dogeTex, VK_SHADER_STAGE_FRAGMENT_BIT);

        std::shared_ptr<Material> cheemzMaterial = std::make_shared<Material>(
            path_prefix + "/shaders/generated/textured-vert.spv",
            path_prefix + "/shaders/generated/textured-frag.spv");
        cheemzMaterial->addBufferBundle(cheemzBufferBundle, VK_SHADER_STAGE_VERTEX_BIT);
        cheemzMaterial->addBufferBundle(sharedUniformBufferBundle, VK_SHADER_STAGE_VERTEX_BIT);
        cheemzMaterial->addTexture(cheemzTex, VK_SHADER_STAGE_FRAGMENT_BIT);

        std::shared_ptr<Material> lightCubeMaterial = std::make_shared<Material>(
            path_prefix + "/shaders/generated/untextured-vert.spv",
            path_prefix + "/shaders/generated/untextured-frag.spv");
        lightCubeMaterial->addBufferBundle(sharedUniformBufferBundle, VK_SHADER_STAGE_VERTEX_BIT);

        /**
         * Adding models to scene.
         */
        scene->addModel(std::make_shared<DrawableModel>(dogeMaterial, path_prefix + "/models/buffDoge.obj"));
        scene->addModel(std::make_shared<DrawableModel>(cheemzMaterial, path_prefix + "/models/cheems.obj"));
        scene->addModel(std::make_shared<DrawableModel>(lightCubeMaterial, path_prefix + "/models/cube.obj"));

        /**
         * Creating flat scene for post process.
         */
        postProcessScene = std::make_shared<Scene>(RenderPassType::eFlat);

        std::shared_ptr<Texture> screenTex = std::make_shared<Texture>(scene->getRenderPass()->getColorImage());
        std::shared_ptr<Material> screenMaterial = std::make_shared<Material>(
            path_prefix + "/shaders/generated/post-process-vert.spv",
            path_prefix + "/shaders/generated/post-process-frag.spv");
        screenMaterial->addTexture(screenTex, VK_SHADER_STAGE_FRAGMENT_BIT);
        postProcessScene->addModel(std::make_shared<DrawableModel>(screenMaterial, MeshType::ePlane));
    }

    void updateScene(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        sharedUbo.view = camera.GetViewMatrix();
        sharedUbo.proj = glm::perspective(glm::radians(45.0f), WIDTH / (float)HEIGHT, 0.1f, 10.0f);
        sharedUbo.proj[1][1] *= -1;
        sharedUbo.lightPos = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        VkDeviceSize bufferSize = sizeof(sharedUbo);

        void *data;
        vmaMapMemory(VulkanGlobal::context.allocator, sharedUniformBufferBundle->buffers[currentImage]->allocation, &data);
        memcpy(data, &sharedUbo, bufferSize);
        vmaUnmapMemory(VulkanGlobal::context.allocator, sharedUniformBufferBundle->buffers[currentImage]->allocation);
    }

    void createCommandBuffers()
    {
        mcvkp::RenderSystem::allocateCommandBuffers(commandBuffers, VulkanGlobal::swapchainContext.swapChainImageViews.size());

        for (size_t i = 0; i < commandBuffers.size(); i++)
        {
            mcvkp::RenderSystem::beginCommandBuffer(commandBuffers[i]);

            scene->writeRenderCommand(commandBuffers[i], i);

            postProcessScene->writeRenderCommand(commandBuffers[i], i);

            mcvkp::RenderSystem::endCommandBuffer(commandBuffers[i]);
        }
    }

    void createSyncObjects()
    {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(VulkanGlobal::swapchainContext.swapChainImageViews.size());

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(VulkanGlobal::context.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(VulkanGlobal::context.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(VulkanGlobal::context.device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
            {

                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    size_t currentFrame = 0;
    void drawFrame()
    {
        vkWaitForFences(VulkanGlobal::context.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(VulkanGlobal::context.device, VulkanGlobal::swapchainContext.swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // Check if a previous frame is using this image (i.e. there is its fence to wait on)
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        {
            vkWaitForFences(VulkanGlobal::context.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        // Mark the image as now being in use by this frame
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        updateScene(imageIndex);

        vkResetFences(VulkanGlobal::context.device, 1, &inFlightFences[currentFrame]);

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        mcvkp::RenderSystem::submit(&commandBuffers[imageIndex], 1, waitSemaphores, waitStages, signalSemaphores, inFlightFences[currentFrame]);

        mcvkp::RenderSystem::present(imageIndex, signalSemaphores, 1);

        // Commented this out for playing around with it later :)
        // vkQueueWaitIdle(VulkanGlobal::context.presentQueue);
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    int nbFrames = 0;
    float lastTime = 0;
    void mainLoop()
    {
        while (!glfwWindowShouldClose(VulkanGlobal::context.window))
        {
            float currentTime = (float)glfwGetTime();
            deltaTime = currentTime - lastFrame;
            nbFrames++;
            if (currentTime - lastTime >= 1.0)
            { // If last prinf() was more than 1 sec ago
                // printf and reset timer
                printf("%f ms/frame\n", 1000.0 / double(nbFrames));
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

    void initVulkan()
    {
        initScene();

        createCommandBuffers();
        createSyncObjects();
        glfwSetCursorPosCallback(VulkanGlobal::context.window, mouse_callback);
    }

    void cleanup()
    {
        vkFreeCommandBuffers(VulkanGlobal::context.device, VulkanGlobal::context.commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(VulkanGlobal::context.device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(VulkanGlobal::context.device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(VulkanGlobal::context.device, inFlightFences[i], nullptr);
        }

        glfwTerminate();
    }
};

int main()
{
    HelloDogApplication app;

    try
    {
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";
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
void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
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
