#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <cstdint>
#include <cstring>
#include <optional>
#include <set>
#include <vector>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <iostream>

// validation layer
VkDebugUtilsMessengerCreateInfoEXT make_debug_msgr_create_info();

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT typ,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger);
void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator);

// queue families for physical devices
struct QueueFamilyIndices {
  std::optional<uint32_t> graphics_family;
  std::optional<uint32_t> present_family;

  bool is_complete() {
    return graphics_family.has_value() && present_family.has_value();
  }
};

QueueFamilyIndices find_queue_families(VkPhysicalDevice device,
                                       VkSurfaceKHR surface);

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char *> validation_layers = {
    "VK_LAYER_KHRONOS_validation",
};
#ifdef NDEBUG
const bool enable_validation = false;
#else
const bool enable_validation = true;
#endif

int main() {
  // intialization
  // -- of the window
  bool initialized = SDL_Init(SDL_INIT_VIDEO);

  SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
  SDL_Window *window = SDL_CreateWindow("Vulkan", WIDTH, HEIGHT, flags);

  // -- of vulkan stuff
  // ---- initalize the instance
  VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr, // initialize explicitly
      .pApplicationName = "hello triangle",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "no engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  // ------ get extensions from SDL
  uint32_t required_ext_count = 0;
  auto sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&required_ext_count);
  std::vector<const char *> required_exts(sdl_extensions,
                                          sdl_extensions + required_ext_count);
  // ------ check validation layers
  uint32_t enabled_layer_count = 0;
  const char *const *enabled_layer_names = nullptr;
  // TODO: might not be the cleanest thing, but we have the variable anyways...
  VkDebugUtilsMessengerCreateInfoEXT instance_debug_msgr =
      make_debug_msgr_create_info();
  void *pnext = nullptr;

  if (enable_validation) {
    std::cout << "validation turned on\n";

    // -- extend the required extensions to include debug/validation stuff
    required_ext_count += 1;
    required_exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // -- check that all validation layers are available
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    for (const char *layer_name : validation_layers) {
      bool found_layer = false;
      for (const auto &layer_prop : available_layers) {
        if (strcmp(layer_name, layer_prop.layerName) == 0) {
          found_layer = true;
          break;
        }
      }
      if (!found_layer) {
        std::cout << "validation layer " << layer_name
                  << " requested, but not available!\n";
        return 1;
      }
    }

    // -- set relevant fields for instance create info
    // safe only because `validation_layers` is global
    enabled_layer_count = static_cast<uint32_t>(validation_layers.size());
    enabled_layer_names = validation_layers.data();
    pnext = &instance_debug_msgr;
  }

  // ------ actually create the instance
  VkInstanceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = pnext,
      .pApplicationInfo = &app_info,
      .enabledLayerCount = enabled_layer_count,
      .ppEnabledLayerNames = enabled_layer_names,
      .enabledExtensionCount = required_ext_count,
      .ppEnabledExtensionNames = required_exts.data(),
  };

  VkInstance instance;
  if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
    std::cerr << "failed to create VkInstance!" << std::endl;
    return 1;
  }

  // ---- setup the debug messenger
  VkDebugUtilsMessengerEXT debug_msgr = nullptr;
  if (enable_validation) {
    auto create_info = make_debug_msgr_create_info();
    if (CreateDebugUtilsMessengerEXT(instance, &create_info, nullptr,
                                     &debug_msgr) != VK_SUCCESS) {
      std::cerr << "failed to setup debug messenger!" << std::endl;
      return 1;
    }
  }

  // ---- checking the available extensions
  uint32_t extension_count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
  std::vector<VkExtensionProperties> extensions(extension_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                         extensions.data());
  std::cout << "available extensions:\n";
  for (const auto &ext : extensions) {
    std::cout << '\t' << ext.extensionName << '\n';
  }

  // ---- set up surface for window
  VkSurfaceKHR surface;
  if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
    std::cerr << "failed to create window surface!" << std::endl;
    return 1;
  }

  // ---- pick a physical device
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
  if (device_count == 0) {
    std::cerr << "failed to find GPUs with vulkan support!" << std::endl;
    return 1;
  }
  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

  QueueFamilyIndices phys_device_qfi;
  for (const auto &device : devices) {
    phys_device_qfi = find_queue_families(device, surface);
    if (phys_device_qfi.is_complete()) {
      physical_device = device;
      break;
    }
  }

  if (physical_device == VK_NULL_HANDLE) {
    std::cerr << "failed to find a suitable GPU!" << std::endl;
    return 1;
  }

  // ---- set up a logical device
  VkDevice device;

  // ------ set up for the device queues
  // NOTE: reusing the queue family indices here since it's guaranteed to be the
  // same as the device we selected.
  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  std::set<uint32_t> unique_queue_families = {
      phys_device_qfi.graphics_family.value(),
      phys_device_qfi.present_family.value()};
  float priority = 1.0f;
  for (uint32_t queue_family : unique_queue_families) {
    queue_create_infos.push_back(VkDeviceQueueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    });
  }

  // ------ physical device features
  VkPhysicalDeviceFeatures device_features{};

  // ------ create the logical device
  uint32_t device_enabled_layer_count = 0;
  const char *const *device_enabled_layer_names = nullptr;
  uint32_t device_enabled_ext_count = 0;
  if (enable_validation) {
    // safe only because `validation_layers` is global
    device_enabled_layer_count = validation_layers.size();
    device_enabled_layer_names = validation_layers.data();
  }
  VkDeviceCreateInfo device_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
      .pQueueCreateInfos = queue_create_infos.data(),
      .enabledLayerCount = device_enabled_layer_count,
      .ppEnabledLayerNames = device_enabled_layer_names,
      .enabledExtensionCount = device_enabled_ext_count,
      .ppEnabledExtensionNames = nullptr,
      .pEnabledFeatures = &device_features,
  };

  if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) !=
      VK_SUCCESS) {
    std::cerr << "failed to create logical device!" << std::endl;
    return 1;
  }

  // ------ get a handle to the graphics queue
  VkQueue graphics_queue;
  vkGetDeviceQueue(device, phys_device_qfi.graphics_family.value(), 0,
                   &graphics_queue);

  // ------ get a handle to the present queue
  VkQueue present_queue;
  vkGetDeviceQueue(device, phys_device_qfi.present_family.value(), 0,
                   &present_queue);

  // main loop
  glm::mat4 matrix;
  glm::vec4 vec;
  glm::vec4 test = matrix * vec;

  bool quit = false;
  while (!quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_EVENT_QUIT:
        quit = true;
        break;
      default:
        break;
      }
    }
  }

  // cleanup
  // -- of vulkan stuff
  if (enable_validation) {
    DestroyDebugUtilsMessengerEXT(instance, debug_msgr, nullptr);
  }
  vkDestroyDevice(device, nullptr);
  SDL_Vulkan_DestroySurface(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);

  // -- of SDL stuff
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}

// create debug messenger create info
VkDebugUtilsMessengerCreateInfoEXT make_debug_msgr_create_info() {
  return VkDebugUtilsMessengerCreateInfoEXT{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debug_callback,
      .pUserData = nullptr,
  };
}

// debug message handler callback
static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
               VkDebugUtilsMessageTypeFlagsEXT typ,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data) {
  std::cerr << "[validation] " << callback_data->pMessage << std::endl;

  return VK_FALSE;
}

// wrappers to lookup extension functions to create and destroy debug messengers
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

QueueFamilyIndices find_queue_families(VkPhysicalDevice device,
                                       VkSurfaceKHR surface) {
  QueueFamilyIndices indices;

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           queue_families.data());

  for (int i = 0; i < queue_families.size(); i++) {
    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphics_family = i;
    }

    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
    if (present_support) {
      indices.present_family = i;
    }

    if (indices.is_complete()) {
      break;
    }
  }

  return indices;
}
