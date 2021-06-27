# Vulkan starter project

This is my attempt to make a structured vulkan project to serve as a base for other vulkan programs.
I added a scene to demonstrate different uses cases:
 - Multiple objects sharing same layout and pipeline.
 - Separate objects using separete layouts.
 - Camera with keyboard controls.
 - Shared object buffer that includes view and projection transforms, and light position.
 - Offscreen render pass that renders the scene into a texture.
 - Post processing render pass that displays the rendered texture onto a quad with gamma correction done in fragment shader.

 ![Uploading ezgif-2-1d3e33644e21.gif…]()

 
It consists of the following parts: 
