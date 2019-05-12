#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

struct Position {
  uint32_t x, y;
};

struct Settings
{
  uint32_t windowWidth;
  uint32_t windowHeight;
  bool fullScreen;
  uint32_t imageWidth;
  uint32_t imageHeight;
  std::vector<Position> positions;
};

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
  glm::vec3 position;
  glm::vec2 uv;
};

struct Ubo
{
  glm::mat4 view;
  glm::mat4 projection;
};

struct Camera
{
  VkExtent2D extent;
  float fov;
  float near;
  float far;

  glm::vec3 position;
  glm::vec3 lookAt;
};