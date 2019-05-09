#pragma once

#include <variant>
#include <vector>

#include <vulkan/vulkan.h>

#include "Structs.h"

template<typename t>
using VulkanCreation = std::variant<t, VkResult>;

const char* VkResultToString(VkResult result);

bool CheckVulkanVersion(uint32_t minVersion);

VulkanCreation<std::vector<const char*>> CheckInstanceLayers(const std::vector<const char*>& layerNames);
VulkanCreation<std::vector<const char*>> CheckInstanceExtensions(const std::vector<const char*>& extensionNames);
VulkanCreation<VkInstance> CreateInstance(const char* applicationName, uint32_t applicationVersion, uint32_t apiVersion, const std::vector<const char*>& layers, const std::vector<const char*>& extensions);

VulkanCreation<PhysicalDevice> GetSuitablePhysicalDevice(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& requiredExtensions);
VulkanCreation<VkDevice> CreateLogicalDevice(PhysicalDevice physicalDevice, VkPhysicalDeviceFeatures *features);
VulkanCreation<Swapchain> CreateSwapchain(PhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device, VkExtent2D defaultExtent);

VulkanCreation<Image2D> CreateImage2D(PhysicalDevice physicalDevice, VkDevice device, VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage, VkImageLayout layout);
VulkanCreation<VkImageView> CreateImageView2D(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectMast = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t baseMipLevel = 0, uint32_t levelCount = 1, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1);

std::vector<char> ReadFile(const std::string& fileName);
VulkanCreation<VkShaderModule> CreateShaderModule(VkDevice device, const std::vector<char>& code);
VulkanCreation<VkPipelineLayout> CreatePipelineLayout(VkDevice device, const std::vector<VkDescriptorSetLayout>& layouts, const std::vector<VkPushConstantRange>& pushConstants);
VulkanCreation<VkPipeline> CreatePipeline(VkDevice device, VkPipelineLayout layout, VkExtent2D extent, VkRenderPass renderPass, const std::string& vertexShaderFileName, const std::string& fragmentShaderFileName);

VulkanCreation<VkDescriptorSetLayout> CreateDescriptorSetLayout(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings);

VulkanCreation<Buffer> CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize requiredSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

VkDescriptorSetLayoutBinding CreateDescriptorSetLayoutBinding(uint32_t binding, uint32_t count, VkDescriptorType type, VkShaderStageFlags stages);
VkDescriptorPoolSize CreateDescriptorPoolSize(uint32_t count, VkDescriptorType type);
VulkanCreation<VkDescriptorPool> CreateDescriptorPool(VkDevice device, uint32_t maxSets, const std::vector<VkDescriptorPoolSize>& sizes);
VkResult AllocateDescriptorSets(VkDevice device, VkDescriptorPool pool, uint32_t count, VkDescriptorSetLayout* layouts, VkDescriptorSet* sets);

VkDescriptorBufferInfo CreateDescriptorBufferInfo(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
VkWriteDescriptorSet CreateWriteDescriptorSet(VkDescriptorSet descriptorSet, uint32_t binding, uint32_t index, VkDescriptorType type, const std::vector<VkDescriptorBufferInfo>& buffers, const std::vector<VkDescriptorImageInfo>& images);

VkAttachmentDescription CreateAttachementDescription(VkFormat format, VkImageLayout initialLayout, VkImageLayout finalLayout);
VkSubpassDescription CreateSubpass(VkPipelineBindPoint bindPoint, const std::vector<VkAttachmentReference>& colorAttachments, VkAttachmentReference *depthStencilAttachment);
std::vector<VkSubpassDependency> CreateDefaultSubpassDependencies(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcFlags, VkAccessFlags dstFlags);
VulkanCreation<VkRenderPass> CreateRenderPass(VkDevice device, const std::vector<VkAttachmentDescription>& attachments, const std::vector<VkSubpassDescription>& subpasses, const std::vector<VkSubpassDependency>& dependencies);
VulkanCreation<VkFramebuffer> CreateFramebuffer(VkDevice device, VkRenderPass renderPass, uint32_t width, uint32_t height, const std::vector<VkImageView>& attachments);

VulkanCreation<VkCommandPool> CreateCommandPool(VkDevice device, uint32_t queueIndex);
VkResult AllocateCommandBuffer(VkDevice device, VkCommandPool pool, uint32_t count, VkCommandBuffer* buffer, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
VkResult BeginCommandBuffer(VkCommandBuffer cmd, VkCommandBufferUsageFlags usage);
void BeginRenderPass(VkCommandBuffer cmd, VkRenderPass renderPass, VkFramebuffer framebuffer, VkExtent2D extent, const std::vector<VkClearValue>& clearValues);
void TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

VulkanCreation<VkFence> CreateFence(VkDevice device);

void FreeBuffer(VkDevice device, const Buffer& buffer);
void FreeImage(VkDevice device, const Image2D& image);

VulkanCreation<VkSampler> CreateSampler(VkDevice device, VkFilter filter, VkSamplerAddressMode mode, uint32_t anisotropyLevel, VkBool32 unnormalizedCoords);