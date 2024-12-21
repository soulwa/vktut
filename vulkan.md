vk notes

## history
- evolution: eg. mobile GPUs having different arch (for energy/space reqs, eg. tiled rendering)
- vk designed for programmable graphics card archs
- fix multithreading support (CPU bottlenecks)
- fix shader compilation - standardized bytecode (SPIR-V?) with single compiler
- unifies graphics and compute functionality

## drawing a tri
1. instance (app params + API ext); physical device to use for operations (can query features here)
2. logical devices (describe physical device features)
  - most ops submitted to vkqueue (draw, memory ops) and executed asynchronously
  - queue allocated from a queue family- each family supports specific operations
3. window - GLFW or SDL
  - window surface vksurfaceKHR
  - swap chain vkswapchainKHR
  - KHR postfix: vulkan extension. 
  - window surface:: vulkan is platform agnostic, this specifies interaction with a window
    - reference to native window handle eg. HWND
  - **swap chain** is a collection of render targets
    - ensure the image we're rendering to is different from one displayed on screen
    - important to make sure only complete images are shown
    - every time we want to draw a frame, ask swap chain to provide image to render to 
    - when finished drawing a frame, image is returned to swap chain to be presented in the future
    - number of render targets (?) and conditions to present finished imgs depends on modes, eg. vsync/double buffer or triple buffer
  - can render directly to a display w/o window manager - use to eg. create own window manager
4. image view
  - once we get an image from the swap chain, wrap in vkimageview and vkframebuffer
    - image view references specific part of an image to be used
    - framebuffer references views for color, depth, and stencil targets
    - could be many different images in swap chain: create image view, framebuffer for each and select appropriate one at draw time
5. render passes: describe images & contents, how to use - only describes, framebuffer binds
6. pipeline: VkPipeline
  - configurable state of graphics - viewport size, depth buffer operation, programmable state (VkShaderModule, created from bytecode)
  - which render targets used in pipeline (specify from render pass)
  - all pipelines (even minor changes, like vertex layout or shaders) require explicit instantiation
    - some things change dynamically eg. viewport size and clear color 
  - all state must be explicit
  - basically ahead of time compilation- more optimization opportunities; runtime is more predictable
7. submit ops to a command buffer, allocated from a command pool
  - for triangle:
    1. begin render pass
    2. bind graphics pipeline
    3. draw 3 verts
    4. end the render pass
  - image in the framebuffer depends on which image the swapchain will give us, so we need to record a command buffer for each possible image and select the right one at draw time
8. main loop:
  1. acquire image with vkAcquireNextImage KHR
  2. select command buffer for image with vkQueueSubmit
  3. return image to swap chain with vkQueuePresentKHR
  - need to use semaphores to ensure correct execution order, since queue ops are async

tl;dr:
```
- Create a VkInstance
- Select a supported graphics card (VkPhysicalDevice)
- Create a VkDevice and VkQueue for drawing and presentation
- Create a window, window surface and swap chain
- Wrap the swap chain images into VkImageView
- Create a render pass that specifies the render targets and usage
- Create framebuffers for the render pass
- Set up the graphics pipeline
- Allocate and record a command buffer with the draw commands for every possible swap chain image
- Draw frames by acquiring images, submitting the right draw command buffer and returning the images back to the swap chain
```

validation: by default, limited error checking. driver will crash instead of error code or just fail silently
- validation layers can exist during development
- standard set from LunarG
- registre callback to get debug messages from layers
- can be a lot easier to debug than gl/d3d!

## dev env
vulkan: `pacman -S vulkan-devel`
sdl3: `yay -S sdl3` (from aur)
glm: `pacman -S glm` (installed)
glslc: `pacman -S shaderc` (installed)

we're using SDL3 instead of glfw - simplifies the build process a bit it seems. we only need -lSDL3, instead of all the other linking requirements 

now im curious - how would we be able to run this application on an end user's machine?
- statically link SDL3?
- distribute the .so files with the application?

## drawing a triangle

base code:

- i won't be using all of this - exceptions.. bleh
  - maybe its the "right" way to handle vk errors early, but seems inappropriate for validation type stuff

won't be using RAII, but recommended to wrap Vk classes with c++ classes that will handle deallocating vk objects. then could do stuff in ctor/dtor, or use uniq/smart ptrs. (meh)
- it might be of some benefit for me to use this stuff in c++ just to see how it is.. but just seems like lame Rust (tacked on runtime costs vs. normal ptrs, no compile-time enforce)

vkCreate/Allocate paired with vkDestroy/Free
- shared param is `pAllocator` - can pass a custom memory allocator.

instance: 

application info has version/api version info, but can also pass along the engine name if the driver knows how to optimize something for that engine (simialr for application, maybe?)
- we also have pnext for future app info - would this be added statically though? or just intrusive linked list on the appinfo struct?

vkinstancecreateinfo tells the driver what extension and validation layers to use
- need to get extension count adn names from sdl 
- layers to enable will be empty for now

can enumerate through possible extensions with vkEnumerate... but need to query it first to get capacity

bonus: check if the sdl extensions are all supported (string in set)

validation layers:
vulkan is deliberately not doing much safety checking for minimal driver overhead (eg. wrong enum can crash or be undef behavior)
- easy to eg. use a new gpu feature and forget to request it

a **validation layer** optionally hooks into function calls to check these things! eg values of parameters, memory leaks, thread safety, logging, profiling

lunarG SDK has common errors (likely just those against the spec?)

check similarly to extensions if all layers are available and pass forward to createapp

can set up a callback to hook into debug messages (since not all of them are fatal)

severity levels
- verbose :: diagnostic msg
- info :: eg. creation of resource
- warning :: behavior which is not an error but likely a bug
- error :: behavior which is invalid and may crash

type levels
- general :: unrelated to spec or perf
- validation :: violates spec or possible mistake
- performance :: potentially non-optimal use of vulkan

can filter out the types and severities which a handler gets

user data could eg. pass a pointer to bits of your application

more ways to configure: `https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap50.html#VK_EXT_debug_utils`

extension functions are not automatically loaded- need to look up its address.
- could we do reflection or macro stuff here?

problem: can't debug the instance creation and destruction
- see `https://github.com/KhronosGroup/Vulkan-Docs/blob/main/appendices/VK_EXT_debug_utils.adoc#examples` -- can create a separate messenger for these 2 calls specifically by assigning to pnext

can use `vk_layer_settings.txt` to customize the layer settings
- vendors have their own set of best practices recommended

physical device:

physical devices are implicitly destroyed with the instance

in general with vkEnumerate - have to double query to get the size. can we make some assumptions here, though?
- could we just have some large void* buffer that we're passing around or something, and then just use a slice of that? seems wasteful (but i guess it's startup.. so who cares)

when checking a physical device, we can query for **properties** and **features**
- properties incl. name, type, vendor, supported vulkan vers.
- features incl. texture compression, 64b float, multi-viewport rendering (for eg. VR)

can check if devices are suitable, or rate its suitability and pick the highest rated device (eg. if we have to fallback to an iGPU)

queue families:

every op in vulkan (drawing, unloading textures, etc.) require commands submitted to a queue
- different types of commands originate from different families
- each family only allows subset of commands
- ex. queue family which only allows processing compute commands, or memory transfer related commands

need to check what queue families are supported by the device and which supports the commands we want to use

me: why so many "Copying old device 0 into new device 0"?

logical device:

similar to instance creation - describes features to use, queues to create
- can create multiple logical devices from the same physical device

don't really need more than 1 queue- can create command buffers on multiple threads, submit on main thread w single call
- me: what if we want more than 1 queue for more than 1 queue family?

can assign priority [0, 1] to queues

device create info - similar to instance w extensions, validation layers, but requires it to be device specific now. eg. `VK_KHR_swapchain` - may not be supported by compute only GPUs.

window surface:

vulkan is platform agnostic, doesn't interface with the window system on its own.
- need to use WSI extensions. SDL already enables the surface extension for us.
- surface has to be created after instance, since it can influence physical device selection.
- technically optional: unlike opengl, don't need to create an invisible window for off-screen rendering.

need to actually see if the device supports presenting images to the surface we created

for this program we're treating graphics and presentation queues as different.
- could add logic to prefer both in the same queue- better perf.
- how much does this improve perf by? since it's just 2 handles to the same resource

swapchain:

no "default framebuffer" like opengl
- we have to own the buffers we will render to before displaying them on a screen.
- the **swap chain** is this infrastructure!

queue of images waiting to be presented
- we will acquire an image here, draw to it, and then return it to the queue
