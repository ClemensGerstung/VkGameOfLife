#include "Structs.h"
#include "GameOfLifeVulkan.h"

#include <array>
#include <set>
#include <fstream>

const char* VkResultToString(VkResult result)
{
  switch (result)
  {
  case VK_SUCCESS:
    return "VK_SUCCESS";
  case VK_NOT_READY:
    return "VK_NOT_READY";
  case VK_TIMEOUT:
    return "VK_TIMEOUT";
  case VK_EVENT_SET:
    return "VK_EVENT_SET";
  case VK_EVENT_RESET:
    return "VK_EVENT_RESET";
  case VK_INCOMPLETE:
    return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED:
    return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST:
    return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED:
    return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT:
    return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS:
    return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_FRAGMENTED_POOL:
    return "VK_ERROR_FRAGMENTED_POOL";
  case VK_ERROR_OUT_OF_POOL_MEMORY:
    return "VK_ERROR_OUT_OF_POOL_MEMORY";
  case VK_ERROR_INVALID_EXTERNAL_HANDLE:
    return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
  case VK_ERROR_SURFACE_LOST_KHR:
    return "VK_ERROR_SURFACE_LOST_KHR";
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
  case VK_SUBOPTIMAL_KHR:
    return "VK_SUBOPTIMAL_KHR";
  case VK_ERROR_OUT_OF_DATE_KHR:
    return "VK_ERROR_OUT_OF_DATE_KHR";
  case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
    return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
  case VK_ERROR_VALIDATION_FAILED_EXT:
    return "VK_ERROR_VALIDATION_FAILED_EXT";
  case VK_ERROR_INVALID_SHADER_NV:
    return "VK_ERROR_INVALID_SHADER_NV";
  case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
    return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
  case VK_ERROR_FRAGMENTATION_EXT:
    return "VK_ERROR_FRAGMENTATION_EXT";
  case VK_ERROR_NOT_PERMITTED_EXT:
    return "VK_ERROR_NOT_PERMITTED_EXT";
  case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:
    return "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT";
  case VK_RESULT_RANGE_SIZE:
    return "VK_RESULT_RANGE_SIZE";
  case VK_RESULT_MAX_ENUM:
    return "VK_RESULT_MAX_ENUM";
  default:
    return "Unknown Result";
  }
}

bool CheckVulkanVersion(uint32_t minVersion)
{
  uint32_t version;
  auto result = vkEnumerateInstanceVersion(&version);

  if (result == VK_SUCCESS && version >= minVersion)
  {
    // TODO: logging
    return true;
  }

  // TODO: logging
  return false;
}

VulkanCreation<std::vector<const char*>> CheckInstanceLayers(const std::vector<const char*>& layerNames)
{
  std::vector<const char*> required = { layerNames };
  uint32_t count;
  std::vector<VkLayerProperties> layers;
  auto result = vkEnumerateInstanceLayerProperties(&count, nullptr);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  layers.resize(count);
  result = vkEnumerateInstanceLayerProperties(&count, layers.data());
  if (result != VK_SUCCESS)
  {
    return result;
  }

  for (int64_t i = int64_t(required.size() - 1); i >= 0; --i)
  {
    const char* name = required[i];
    bool found = false;
    for (auto& layer : layers)
    {
      if (strcmp(name, layer.layerName) == 0)
      {
        found = true;
        break;
      }
    }

    if (!found)
    {
      required.erase((required.begin() + i));
    }
  }

  return required;
}

VulkanCreation<std::vector<const char*>> CheckInstanceExtensions(const std::vector<const char*>& extensionNames)
{
  std::vector<const char*> required = { extensionNames };
  uint32_t count;
  std::vector<VkExtensionProperties> extensions = {};
  auto result = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  extensions.resize(count);
  result = vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
  if (result != VK_SUCCESS)
  {
    return result;
  }

  for (int64_t i = int64_t(required.size() - 1); i >= 0; --i)
  {
    const char* name = required[i];
    bool found = false;
    for (auto& extension : extensions)
    {
      if (strcmp(name, extension.extensionName) == 0)
      {
        found = true;
        break;
      }
    }

    if (!found)
    {
      auto r = required.erase((std::begin(required) + i));
    }
  }

  return required;
}

VulkanCreation<VkInstance> CreateInstance(const char* applicationName, uint32_t applicationVersion, uint32_t apiVersion, const std::vector<const char*>& layers, const std::vector<const char*>& extensions)
{
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pNext = nullptr;
  appInfo.pEngineName = "LowPolyEngine";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
  appInfo.pApplicationName = applicationName;
  appInfo.applicationVersion = applicationVersion;
  appInfo.apiVersion = apiVersion;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pNext = nullptr;
  createInfo.flags = 0;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();
  createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
  createInfo.ppEnabledLayerNames = layers.data();

  VkInstance instance;
  VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

  if (result != VK_SUCCESS)
  {
    return result;
  }

  return instance;
}

VulkanCreation<PhysicalDevice> GetSuitablePhysicalDevice(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& requiredExtensions)
{
  uint32_t count;
  std::vector<VkPhysicalDevice> physicalDevices;
  VkResult result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  physicalDevices.resize(count);
  result = vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data());
  if (result != VK_SUCCESS)
  {
    return result;
  }

  for (auto physicalDevice : physicalDevices)
  {
    PhysicalDevice device = { physicalDevice };
    vkGetPhysicalDeviceProperties(physicalDevice, &device.properties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &device.features);
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &device.capabilities);

    uint32_t formatCount;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    if (result != VK_SUCCESS)
    {
      return result;
    }

    if (formatCount != 0) {
      device.formats.resize(formatCount);
      result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, device.formats.data());
      if (result != VK_SUCCESS)
      {
        return result;
      }
    }

    uint32_t presentModeCount;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    if (result != VK_SUCCESS)
    {
      return result;
    }

    if (presentModeCount != 0) {
      device.presentModes.resize(presentModeCount);
      result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, device.presentModes.data());
      if (result != VK_SUCCESS)
      {
        return result;
      }
    }

    if (device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
      uint32_t queueCount;
      vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);
      std::vector<VkQueueFamilyProperties> queues(queueCount);
      vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queues.data());

      // init them to the queue count + 1 so I can check if I got all needed queues
      device.graphicsQueueIndex = queueCount + 1;
      device.presentationQueueIndex = queueCount + 1;

      int index = 0;
      for (auto queue : queues)
      {
        if (queue.queueFlags & VK_QUEUE_GRAPHICS_BIT && queue.queueCount > 0)
        {
          device.graphicsQueueIndex = index;
        }

        VkBool32 presentSupport = false;
        result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, index, surface, &presentSupport);
        if (result != VK_SUCCESS)
        {
          return result;
        }

        if (presentSupport && queue.queueCount > 0)
        {
          device.presentationQueueIndex = index;
        }

        index++;
      }

      if (device.graphicsQueueIndex != queueCount + 1 &&
        device.presentationQueueIndex != queueCount + 1)
      {
        device.hasAllQueues = true;
        uint32_t extCount;
        result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        if (result != VK_SUCCESS)
        {
          return result;
        }

        std::vector<VkExtensionProperties> extensions(extCount);
        result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, extensions.data());
        if (result != VK_SUCCESS)
        {
          return result;
        }

        std::set<std::string> required(requiredExtensions.begin(), requiredExtensions.end());

        for (auto extension : extensions)
        {
          for (auto required : requiredExtensions)
          {
            if (strcmp(required, extension.extensionName) == 0)
            {
              device.supportedExtensions.push_back(required);
            }
          }

          required.erase(extension.extensionName);
        }

        device.hasAllRequiredExtensions = required.empty();
      }
    }

    if (device.hasAllRequiredExtensions &&
      device.hasAllQueues &&
      !device.formats.empty() &&
      !device.presentModes.empty())
    {
      return device;
      break;
    }
  }

  return nullptr;
}

VulkanCreation<VkDevice> CreateLogicalDevice(PhysicalDevice physicalDevice, VkPhysicalDeviceFeatures *features)
{
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> queueIndices = { physicalDevice.graphicsQueueIndex, physicalDevice.presentationQueueIndex };
  float prio = 1.0f;
  for (auto& index : queueIndices)
  {
    VkDeviceQueueCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.pNext = nullptr;
    createInfo.pQueuePriorities = &prio;
    createInfo.queueCount = 1;
    createInfo.queueFamilyIndex = index;
    queueCreateInfos.push_back(createInfo);
  }

  VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
  createInfo.flags = 0;
  createInfo.pNext = 0;
  createInfo.queueCreateInfoCount = uint32_t(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.enabledExtensionCount = uint32_t(physicalDevice.supportedExtensions.size());
  createInfo.ppEnabledExtensionNames = physicalDevice.supportedExtensions.data();
  createInfo.enabledLayerCount = 0;
  createInfo.ppEnabledLayerNames = nullptr;
  createInfo.pEnabledFeatures = features;

  VkDevice device;
  auto result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  return device;
}

VulkanCreation<Swapchain> CreateSwapchain(PhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device, VkExtent2D defaultExtent)
{
  Swapchain swapchain;
  VkSurfaceFormatKHR surfaceFormat = physicalDevice.formats[0];
  VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
  VkExtent2D extent = defaultExtent;

  if (physicalDevice.formats.size() == 1 && physicalDevice.formats[0].format == VK_FORMAT_UNDEFINED)
  {
    surfaceFormat = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
  }

  for (const auto& availableFormat : physicalDevice.formats)
  {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      surfaceFormat = availableFormat;
      break;
    }
  }

  for (const auto& availablePresentMode : physicalDevice.presentModes)
  {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
    {
      bestMode = availablePresentMode;
    }
    else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
    {
      bestMode = availablePresentMode;
    }
  }

  if (physicalDevice.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    extent = physicalDevice.capabilities.currentExtent;
  }
  else
  {
    extent.width = std::max(physicalDevice.capabilities.minImageExtent.width, std::min(physicalDevice.capabilities.maxImageExtent.width, extent.width));
    extent.height = std::max(physicalDevice.capabilities.minImageExtent.height, std::min(physicalDevice.capabilities.maxImageExtent.height, extent.height));
  }

  uint32_t imageCount = physicalDevice.capabilities.minImageCount + 1;
  if (physicalDevice.capabilities.maxImageCount > 0 && imageCount > physicalDevice.capabilities.maxImageCount)
  {
    imageCount = physicalDevice.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
  createInfo.flags = 0;
  createInfo.pNext = nullptr;
  createInfo.surface = surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  createInfo.preTransform = physicalDevice.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = bestMode;
  createInfo.clipped = VK_TRUE;

  createInfo.oldSwapchain = VK_NULL_HANDLE;

  uint32_t queueFamilyIndices[] = { physicalDevice.graphicsQueueIndex, physicalDevice.presentationQueueIndex };

  if (physicalDevice.graphicsQueueIndex != physicalDevice.presentationQueueIndex)
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  }
  else
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  auto result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain.swapchain);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  swapchain.format = surfaceFormat.format;
  swapchain.extent = extent;

  uint32_t swapchainImageCount;
  result = vkGetSwapchainImagesKHR(device, swapchain.swapchain, &swapchainImageCount, nullptr);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  swapchain.images.resize(swapchainImageCount);
  swapchain.imageViews.resize(swapchainImageCount);
  result = vkGetSwapchainImagesKHR(device, swapchain.swapchain, &swapchainImageCount, swapchain.images.data());
  if (result != VK_SUCCESS)
  {
    return result;
  }

  return swapchain;
}

VulkanCreation<Image2D> CreateImage2D(PhysicalDevice physicalDevice, VkDevice device, VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage, VkImageLayout layout)
{
  Image2D image = {};
  image.format = format;
  image.width = width;
  image.height = height;
  image.usage = usage;
  image.layout = layout;

  VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
  imageCreateInfo.flags = 0;
  imageCreateInfo.pNext = nullptr;
  imageCreateInfo.format = format;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.extent.width = width;
  imageCreateInfo.extent.height = height;
  imageCreateInfo.extent.depth = 1;
  imageCreateInfo.mipLevels = 1;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCreateInfo.usage = usage;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCreateInfo.queueFamilyIndexCount = 0;
  imageCreateInfo.pQueueFamilyIndices = nullptr;
  imageCreateInfo.initialLayout = layout;

  auto result = vkCreateImage(device, &imageCreateInfo, nullptr, &image.image);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  VkMemoryRequirements req;
  vkGetImageMemoryRequirements(device, image.image, &req);
  image.memoryTypeIndex = findMemoryType(physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  image.size = req.size;

  VkMemoryAllocateInfo imageAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
  imageAllocInfo.pNext = nullptr;
  imageAllocInfo.allocationSize = req.size;
  imageAllocInfo.memoryTypeIndex = image.memoryTypeIndex;

  result = vkAllocateMemory(device, &imageAllocInfo, nullptr, &image.memory);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  result = vkBindImageMemory(device, image.image, image.memory, 0);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  auto imageViewCreation = CreateImageView2D(device, image.image, format);
  if (std::holds_alternative<VkResult>(imageViewCreation))
  {
    return std::get<VkResult>(imageViewCreation);
  }

  image.view = std::get<VkImageView>(imageViewCreation);

  return image;
}

VulkanCreation<VkImageView> CreateImageView2D(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectMask, uint32_t baseMipLevel, uint32_t levelCount, uint32_t baseArrayLayer, uint32_t layerCount)
{
  VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  createInfo.flags = 0;
  createInfo.pNext = nullptr;
  createInfo.image = image;
  createInfo.format = format;
  createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.subresourceRange.aspectMask = aspectMask;
  createInfo.subresourceRange.baseMipLevel = baseMipLevel;
  createInfo.subresourceRange.levelCount = levelCount;
  createInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
  createInfo.subresourceRange.layerCount = layerCount;

  VkImageView view;
  auto result = vkCreateImageView(device, &createInfo, nullptr, &view);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  return view;
}

std::vector<char> ReadFile(const std::string& fileName)
{
  std::vector<char> buffer(0);
  std::ifstream file(fileName, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    return buffer;
  }

  size_t fileSize = (size_t)file.tellg();
  buffer.resize(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

VulkanCreation<VkShaderModule> CreateShaderModule(VkDevice device, const std::vector<char>& code)
{
  VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
  createInfo.flags = 0;
  createInfo.pNext = nullptr;
  createInfo.codeSize = uint32_t(code.size());
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  VkShaderModule shader;
  auto result = vkCreateShaderModule(device, &createInfo, nullptr, &shader);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  return shader;
}

VulkanCreation<VkPipelineLayout> CreatePipelineLayout(VkDevice device, const std::vector<VkDescriptorSetLayout>& layouts, const std::vector<VkPushConstantRange>& pushConstants)
{
  VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  createInfo.flags = 0;
  createInfo.pNext = nullptr;
  createInfo.pPushConstantRanges = pushConstants.data();
  createInfo.pushConstantRangeCount = uint32_t(pushConstants.size());
  createInfo.setLayoutCount = uint32_t(layouts.size());
  createInfo.pSetLayouts = layouts.data();

  VkPipelineLayout layout;
  auto result = vkCreatePipelineLayout(device, &createInfo, nullptr, &layout);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  return layout;
}

VulkanCreation<VkDescriptorSetLayout> CreateDescriptorSetLayout(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings)
{
  VkDescriptorSetLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  createInfo.flags = 0;
  createInfo.pNext = nullptr;
  createInfo.bindingCount = uint32_t(bindings.size());
  createInfo.pBindings = bindings.data();

  VkDescriptorSetLayout layout;
  auto result = vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &layout);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  return layout;
}

VulkanCreation<Buffer> CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize requiredSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
  Buffer buffer = {};

  VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferCreateInfo.flags = 0;
  bufferCreateInfo.pNext = nullptr;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  bufferCreateInfo.size = requiredSize;
  bufferCreateInfo.pQueueFamilyIndices = nullptr;
  bufferCreateInfo.queueFamilyIndexCount = 0;
  bufferCreateInfo.usage = usage;

  auto result = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer.buffer);
  if (result != VK_SUCCESS)
  {
    return result;
  }
  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(device, buffer.buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

  result = vkAllocateMemory(device, &allocInfo, nullptr, &buffer.memory);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  result = vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  buffer.memoryTypeIndex = allocInfo.memoryTypeIndex;
  buffer.usage = usage;
  buffer.properties = properties;
  buffer.size = memRequirements.size;

  return buffer;
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  return std::numeric_limits<uint32_t>::max();
}

VulkanCreation<VkPipeline> CreatePipeline(VkDevice device, VkPipelineLayout layout, VkExtent2D extent, VkRenderPass renderPass, const std::string& vertexShaderFileName, const std::string& fragmentShaderFileName)
{
  auto vertexShaderCode = ReadFile(vertexShaderFileName);
  auto fragmentShaderCode = ReadFile(fragmentShaderFileName);
  VkShaderModule fragmentShader, vertexShader;

  auto vertexShaderCreation = CreateShaderModule(device, vertexShaderCode);
  if (std::holds_alternative<VkResult>(vertexShaderCreation))
  {
    return std::get<VkResult>(vertexShaderCreation);
  }
  vertexShader = std::get<VkShaderModule>(vertexShaderCreation);

  auto fragmentShaderCreation = CreateShaderModule(device, fragmentShaderCode);
  if (std::holds_alternative<VkResult>(fragmentShaderCreation))
  {
    return std::get<VkResult>(fragmentShaderCreation);
  }
  fragmentShader = std::get<VkShaderModule>(fragmentShaderCreation);

  VkPipelineShaderStageCreateInfo vertShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
  vertShaderStageInfo.flags = 0;
  vertShaderStageInfo.pNext = nullptr;
  vertShaderStageInfo.pName = "main";
  vertShaderStageInfo.module = vertexShader;
  vertShaderStageInfo.pSpecializationInfo = nullptr;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

  VkPipelineShaderStageCreateInfo fragShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
  fragShaderStageInfo.flags = 0;
  fragShaderStageInfo.pNext = nullptr;
  fragShaderStageInfo.pName = "main";
  fragShaderStageInfo.module = fragmentShader;
  fragShaderStageInfo.pSpecializationInfo = nullptr;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkPipelineShaderStageCreateInfo stageInfos[] = { vertShaderStageInfo, fragShaderStageInfo };

  VkVertexInputBindingDescription bindingDescription = {};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions = {};
  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[0].offset = 0;

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
  vertexInputInfo.flags = 0;
  vertexInputInfo.pNext = nullptr;
  vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(attributeDescriptions.size());
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport = {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = float(extent.width);
  viewport.height = float(extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor = {};
  scissor.offset = { 0, 0 };
  scissor.extent = extent;

  VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = stageInfos;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = layout;
  pipelineInfo.renderPass = renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  VkPipeline pipeline;
  auto result = vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &pipeline);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  vkDestroyShaderModule(device, vertexShader, nullptr);
  vkDestroyShaderModule(device, fragmentShader, nullptr);

  return pipeline;
}

VkDescriptorSetLayoutBinding CreateDescriptorSetLayoutBinding(uint32_t binding, uint32_t count, VkDescriptorType type, VkShaderStageFlags stages)
{
  VkDescriptorSetLayoutBinding setLayoutBinding = {};
  setLayoutBinding.binding = binding;
  setLayoutBinding.descriptorCount = count;
  setLayoutBinding.descriptorType = type;
  setLayoutBinding.stageFlags = stages;

  return setLayoutBinding;
}

VkDescriptorPoolSize CreateDescriptorPoolSize(uint32_t count, VkDescriptorType type)
{
  VkDescriptorPoolSize size = {};
  size.descriptorCount = count;
  size.type = type;

  return size;
}

VulkanCreation<VkDescriptorPool> CreateDescriptorPool(VkDevice device, uint32_t maxSets, const std::vector<VkDescriptorPoolSize>& sizes)
{
  VkDescriptorPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
  createInfo.flags = 0;
  createInfo.pNext = nullptr;
  createInfo.pPoolSizes = sizes.data();
  createInfo.poolSizeCount = uint32_t(sizes.size());
  createInfo.maxSets = maxSets;

  VkDescriptorPool descriptorPool;
  auto result = vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  return descriptorPool;
}

VkResult AllocateDescriptorSets(VkDevice device, VkDescriptorPool pool, uint32_t count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets)
{
  VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  allocInfo.pNext = nullptr;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = count;
  allocInfo.pSetLayouts = layouts;

  return vkAllocateDescriptorSets(device, &allocInfo, sets);
}

VkDescriptorBufferInfo CreateDescriptorBufferInfo(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = buffer;
  bufferInfo.offset = offset;
  bufferInfo.range = range;

  return bufferInfo;
}

VkWriteDescriptorSet CreateWriteDescriptorSet(VkDescriptorSet descriptorSet, uint32_t binding, uint32_t index, VkDescriptorType type, const std::vector<VkDescriptorBufferInfo>& buffers, const std::vector<VkDescriptorImageInfo>& images)
{
  VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
  descriptorWrite.pNext = nullptr;
  descriptorWrite.dstSet = descriptorSet;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
  descriptorWrite.descriptorType = type;
  descriptorWrite.descriptorCount = uint32_t(buffers.size() + images.size());
  descriptorWrite.pBufferInfo = buffers.data();
  descriptorWrite.pImageInfo = images.data();
  
  return descriptorWrite;
}

VkAttachmentDescription CreateAttachementDescription(VkFormat format, VkImageLayout initialLayout, VkImageLayout finalLayout)
{
  VkAttachmentDescription desc = {};
  desc.flags = 0;
  desc.format = format;
  desc.samples = VK_SAMPLE_COUNT_1_BIT;
  desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  desc.initialLayout = initialLayout;
  desc.finalLayout = finalLayout;

  return desc;
}

VkSubpassDescription CreateSubpass(VkPipelineBindPoint bindPoint, const std::vector<VkAttachmentReference>& colorAttachments, VkAttachmentReference *depthStencilAttachment)
{
  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = bindPoint;
  subpass.colorAttachmentCount = uint32_t(colorAttachments.size());
  subpass.pColorAttachments = colorAttachments.data();
  subpass.pDepthStencilAttachment = depthStencilAttachment;

  return subpass;
}

std::vector<VkSubpassDependency> CreateDefaultSubpassDependencies(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcFlags, VkAccessFlags dstFlags)
{
  std::vector<VkSubpassDependency> dependencies(2);

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = srcStage;
  dependencies[0].dstStageMask = dstStage;
  dependencies[0].srcAccessMask = srcFlags;
  dependencies[0].dstAccessMask = dstFlags;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = dstStage;
  dependencies[1].dstStageMask = srcStage;
  dependencies[1].srcAccessMask = dstFlags;
  dependencies[1].dstAccessMask = srcFlags;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  return dependencies;
}

VulkanCreation<VkRenderPass> CreateRenderPass(VkDevice device, const std::vector<VkAttachmentDescription>& attachments, const std::vector<VkSubpassDescription>& subpasses, const std::vector<VkSubpassDependency>& dependencies)
{
  VkRenderPassCreateInfo renderPassCreateInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
  renderPassCreateInfo.pNext = nullptr;
  renderPassCreateInfo.flags = 0;
  renderPassCreateInfo.attachmentCount = uint32_t(attachments.size());
  renderPassCreateInfo.pAttachments = attachments.data();
  renderPassCreateInfo.subpassCount = uint32_t(subpasses.size());
  renderPassCreateInfo.pSubpasses = subpasses.data();
  renderPassCreateInfo.dependencyCount = uint32_t(dependencies.size());
  renderPassCreateInfo.pDependencies = dependencies.data();

  VkRenderPass renderPass;
  auto result = vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  return renderPass;
}

VulkanCreation<VkFramebuffer> CreateFramebuffer(VkDevice device, VkRenderPass renderPass, uint32_t width, uint32_t height, const std::vector<VkImageView>& attachments)
{
  VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
  framebufferCreateInfo.flags = 0;
  framebufferCreateInfo.pNext = nullptr;
  framebufferCreateInfo.renderPass = renderPass;
  framebufferCreateInfo.attachmentCount = uint32_t(attachments.size());
  framebufferCreateInfo.pAttachments = attachments.data();
  framebufferCreateInfo.width = width;
  framebufferCreateInfo.height = height;
  framebufferCreateInfo.layers = 1;

  VkFramebuffer framebuffer;
  auto result = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  return framebuffer;
}

VulkanCreation<VkCommandPool> CreateCommandPool(VkDevice device, uint32_t queueIndex)
{
  VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  commandPoolCreateInfo.pNext = nullptr;
  commandPoolCreateInfo.queueFamilyIndex = queueIndex;

  VkCommandPool commandPool;
  auto result = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  return commandPool;
}

VkResult AllocateCommandBuffer(VkDevice device, VkCommandPool pool, uint32_t count, VkCommandBuffer* buffer, VkCommandBufferLevel level)
{
  VkCommandBufferAllocateInfo cmdBufferAllocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  cmdBufferAllocInfo.pNext = nullptr;
  cmdBufferAllocInfo.commandPool = pool;
  cmdBufferAllocInfo.level = level;
  cmdBufferAllocInfo.commandBufferCount = count;

  return vkAllocateCommandBuffers(device, &cmdBufferAllocInfo, buffer);
}

VkResult BeginCommandBuffer(VkCommandBuffer cmd, VkCommandBufferUsageFlags usage)
{
  VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  beginInfo.flags = usage;
  beginInfo.pNext = nullptr;
  beginInfo.pInheritanceInfo = nullptr;

  return vkBeginCommandBuffer(cmd, &beginInfo);
}

void BeginRenderPass(VkCommandBuffer cmd, VkRenderPass renderPass, VkFramebuffer framebuffer, VkExtent2D extent, const std::vector<VkClearValue>& clearValues)
{
  VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
  renderPassInfo.renderPass = renderPass;
  renderPassInfo.framebuffer = framebuffer;
  renderPassInfo.renderArea.offset = { 0, 0 };
  renderPassInfo.renderArea.extent = extent;
  renderPassInfo.clearValueCount = uint32_t(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
  VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = srcAccess;
  barrier.dstAccessMask = dstAccess;

  vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VulkanCreation<VkFence> CreateFence(VkDevice device)
{
  VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
  fenceCreateInfo.flags = 0;
  fenceCreateInfo.pNext = nullptr;

  VkFence fence;
  auto result = vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);
  if (result != VK_SUCCESS)
  {
    return result;
  }

  return fence;
}

void FreeBuffer(VkDevice device, const Buffer& buffer)
{
  vkFreeMemory(device, buffer.memory, nullptr);
  vkDestroyBuffer(device, buffer.buffer, nullptr);
}

void FreeImage(VkDevice device, const Image2D& image)
{
  vkDestroyImageView(device, image.view, nullptr);
  vkFreeMemory(device, image.memory, nullptr);
  vkDestroyImage(device, image.image, nullptr);
}
