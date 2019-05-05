#pragma once

#include <vector>

#include <vulkan/vulkan.h>

struct PhysicalDevice
{
  PhysicalDevice(VkPhysicalDevice device) : device(device)
  {

  }

  PhysicalDevice(std::nullptr_t)
  {
    device = nullptr;
  }

  VkPhysicalDevice device;
  VkPhysicalDeviceProperties properties;
  VkPhysicalDeviceFeatures features;
  uint32_t graphicsQueueIndex;
  uint32_t presentationQueueIndex;
  bool hasAllQueues;
  std::vector<const char*> supportedExtensions;
  bool hasAllRequiredExtensions;
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;

  bool operator == (std::nullptr_t)
  {
    return device == nullptr;
  }

  operator VkPhysicalDevice() const
  {
    return device;
  }
};

struct Swapchain
{
  VkSwapchainKHR swapchain;
  VkFormat format;
  VkExtent2D extent;
  std::vector<VkImage> images;
  std::vector<VkImageView> imageViews;
};

struct Buffer
{
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDeviceSize size;
  VkBufferUsageFlags usage;
  VkMemoryPropertyFlags properties;
  uint32_t memoryTypeIndex;
};

struct Image2D
{
  VkImage image;
  VkImageView view;
  VkDeviceMemory memory;
  VkFormat format;
  uint32_t width;
  uint32_t height;
  VkImageUsageFlags usage;
  VkImageLayout layout;
  VkDeviceSize size;
  VkMemoryPropertyFlags properties;
  uint32_t memoryTypeIndex;
};

struct Vertex {
  float position[3];
};

struct Position {
  float x, y;
};