#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

std::string errorString(VkResult errorCode) {
  switch (errorCode) {
#define STR(r)                                                                 \
  case VK_##r:                                                                 \
    return #r
    STR(NOT_READY);
    STR(TIMEOUT);
    STR(EVENT_SET);
    STR(EVENT_RESET);
    STR(INCOMPLETE);
    STR(ERROR_OUT_OF_HOST_MEMORY);
    STR(ERROR_OUT_OF_DEVICE_MEMORY);
    STR(ERROR_INITIALIZATION_FAILED);
    STR(ERROR_DEVICE_LOST);
    STR(ERROR_MEMORY_MAP_FAILED);
    STR(ERROR_LAYER_NOT_PRESENT);
    STR(ERROR_EXTENSION_NOT_PRESENT);
    STR(ERROR_FEATURE_NOT_PRESENT);
    STR(ERROR_INCOMPATIBLE_DRIVER);
    STR(ERROR_TOO_MANY_OBJECTS);
    STR(ERROR_FORMAT_NOT_SUPPORTED);
    STR(ERROR_SURFACE_LOST_KHR);
    STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
    STR(SUBOPTIMAL_KHR);
    STR(ERROR_OUT_OF_DATE_KHR);
    STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
    STR(ERROR_VALIDATION_FAILED_EXT);
    STR(ERROR_INVALID_SHADER_NV);
#undef STR
  default:
    return "UNKNOWN_ERROR";
  }
}

#define VK_CHECK_RESULT(f)                                                     \
  {                                                                            \
    VkResult res = (f);                                                        \
    if (res != VK_SUCCESS) {                                                   \
      std::cout << "Fatal : VkResult is \"" << errorString(res) << "\" in "    \
                << __FILE__ << " at line " << __LINE__ << "\n";                \
      assert(res == VK_SUCCESS);                                               \
    }                                                                          \
  }

int main(int argc, const char **argv) {
  if (argc != 2) {
    std::cout << "Missing arg\n";
    return -1;
  }

  int copyType = 0;
  if (std::strcmp(argv[1], "memcpy") == 0) {
    std::cout << "=== Using memcpy ===" << std::endl;
    copyType = 0;
  } else if (std::strcmp(argv[1], "dumb") == 0) {
    std::cout << "=== Using dumb-copy ===" << std::endl;
    copyType = 1;
  } else if (std::strcmp(argv[1], "reference") == 0) {
    std::cout << "=== Using reference ===" << std::endl;
    copyType = -1;
  } else {
    std::cout << "Unknown arg\n";
    return -1;
  }

  /////////////////////
  // Create Instance //

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Vulkan memory test";
  appInfo.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pApplicationInfo = &appInfo;

  uint32_t layerCount = 1;
  const char *validationLayers[] = {"VK_LAYER_LUNARG_standard_validation"};

  // Check if layers are available
  uint32_t instanceLayerCount;
  vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
  std::vector<VkLayerProperties> instanceLayers(instanceLayerCount);
  vkEnumerateInstanceLayerProperties(&instanceLayerCount,
                                     instanceLayers.data());

  bool layersAvailable = true;
  for (auto layerName : validationLayers) {
    bool layerAvailable = false;
    for (auto instanceLayer : instanceLayers) {
      if (std::strcmp(instanceLayer.layerName, layerName) == 0) {
        layerAvailable = true;
        break;
      }
    }
    if (!layerAvailable) {
      layersAvailable = false;
      break;
    }
  }

  const char *validationExt = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
  if (layersAvailable) {
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
    instanceCreateInfo.enabledLayerCount = layerCount;
    instanceCreateInfo.enabledExtensionCount = 1;
    instanceCreateInfo.ppEnabledExtensionNames = &validationExt;
  }
  VkInstance instance;
  VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

  ///////////////////
  // Create Device //
  // Physical device (always use first)
  uint32_t deviceCount = 0;
  VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
  std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
  VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &deviceCount,
                                             physicalDevices.data()));
  VkPhysicalDevice physicalDevice = physicalDevices[0];

  VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
  std::cout << "GPU: " << deviceProperties.deviceName << "\n";

  VkDeviceQueueCreateInfo queueCreateInfo = {};
  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
  VkDevice device;
  VK_CHECK_RESULT(
      vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));

  ////////////
  // Memory //
  VmaAllocator allocator;
  VmaAllocatorCreateInfo allocatorCreateInfo{
      .physicalDevice = physicalDevice,
      .device = device,
      .instance = instance,
  };
  VK_CHECK_RESULT(vmaCreateAllocator(&allocatorCreateInfo, &allocator));

  VkBufferCreateInfo bufferCreateInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = 5 * 1024 * 1024,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  VmaAllocationCreateInfo allocationCreateInfo{
      .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .usage = VMA_MEMORY_USAGE_GPU_TO_CPU,
  };
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
  VK_CHECK_RESULT(vmaCreateBuffer(allocator, &bufferCreateInfo,
                                  &allocationCreateInfo, &buffer, &allocation,
                                  &allocationInfo));

  //////////////////////
  // Performance test //
  std::vector<std::byte> data(allocationInfo.size);
  std::vector<std::byte> referenceSrc(allocationInfo.size);

  auto start = std::chrono::high_resolution_clock::now();
  if (copyType == 0) {
    std::memcpy(data.data(), allocationInfo.pMappedData, data.size());
  } else if (copyType == 1) {
    std::byte *mapped = (std::byte *)allocationInfo.pMappedData;
    for (std::int32_t i = 0; i < data.size(); i++) {
      data[i] = mapped[i];
    }
  } else if (copyType == -1) {
    std::memcpy(data.data(), referenceSrc.data(), data.size());
  }
  auto end = std::chrono::high_resolution_clock::now();

  auto durationUs =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  std::cout << "Duration:  " << durationUs << " us\n";
  std::cout << "Bandwidth: "
            << (allocationInfo.size * 1E-6) / (durationUs * 1E-6) << " MB/s\n";

  return 0;
}