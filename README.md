# Vulkan starter project

This is my attempt to make a structured vulkan project to serve as a base for other vulkan programs.

The project consists of the following parts: 
 - [Application context](https://github.com/grigoryoskin/vulkan-project-starter/blob/master/src/app-context/VulkanApplicationContext.h) - A wrapper for instance, device, queues and command pool.
 - [Swap chain](https://github.com/grigoryoskin/vulkan-project-starter/blob/master/src/app-context/VulkanSwapchain.h) - Manages swap chain and its images.
 - Render context - wrapper for render pass, framebuffer and its attachments. This project has [offscreen render context](https://github.com/grigoryoskin/vulkan-project-starter/blob/master/src/render-context/OffscreenRenderContext.h) for rendering a scene onto a texture and a [post process render context](https://github.com/grigoryoskin/vulkan-project-starter/blob/master/src/render-context/PostProcessRenderContext.h) for displaying the texture onto a screen.
 - [Models](https://github.com/grigoryoskin/vulkan-project-starter/tree/master/src/scene/models) contain vertex, index buffers and descriptor sets.

Demo scene in [main.cpp](https://github.com/grigoryoskin/vulkan-project-starter/blob/master/src/main.cpp) demonstrates how this parts work together. It contains multiple objects with shared and separate pipelenes, movable camera,offscreen render pass, post process render pass.

![ezgif-4-99e2f6d18489](https://user-images.githubusercontent.com/44236259/123562250-7c233a00-d7e8-11eb-9fee-a86363358d0b.gif)

## TODOs: 
- [ ] Organize header files and includes.
- [ ] Use Vulkan Memory Allocator.
- [ ] Make ApplicationContext into a global singleton.
- [ ] Add multisampling.
- [ ] Use fences for GPU - CPU synchronization.
- [ ] Support swapchain recreation on resize.

## How to run
This is an instruction for mac os, but it should work for other systems too, since all the dependencies come from git submodules and build with cmake.
1. Download and install [Vulkan SDK] (https://vulkan.lunarg.com)
2. Pull glfw, glm, stb and obj loader:
```
git submudule init
git submodule update
```
3. Create a buld folder and step into it.
```
mkdir build
cd build
```
4. Run cmake. It will create `makefile` in build folder.
```
cmake -S ../ -B ./
```
5. Create an executable with makefile.
```
make
```
6. Compile shaders. You might want to run this with sudo if you dont have permissions for write.
```
sh ../compile.sh
```
7. Run the executable.
```
./vulkan
```
