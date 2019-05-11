#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <variant>
#include <vector>
#include <algorithm> 
#include <set>
#include <unordered_set>
#include <fstream>
#include <array>
#include <chrono>
#include <thread>

#include "Structs.h"
#include "GameOfLifeVulkan.h"

#if _WIN32
#include <conio.h>

#define GETOUT(ret) _getch(); return ret;
#endif

namespace po = boost::program_options;

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;
constexpr int32_t FPS = 100;

#define CHECK_RESULT(result, errormessage) if (std::holds_alternative<VkResult>(result)) \
                                           { \
                                              std::cout << errormessage << "VkResult = " << VkResultToString(std::get<VkResult>(result)) << std::endl; \
                                              GETOUT(1); \
                                           }

#define CHECK_RESULT_INTERNAL(result) if (std::holds_alternative<VkResult>(result)) \
                                      { \
                                         return std::get<VkResult>(result); \
                                      }

#define CHECK_RESULT_BOOL(result) if (std::holds_alternative<VkResult>(result)) \
                                  { \
                                    return false; \
                                  }

bool RenderInitialImage(const PhysicalDevice& physicalDevice, VkDevice device, VkQueue graphicsQueue, uint32_t* positions, uint32_t positionCount, const Image2D& image, const Buffer& quadHostBuffer, const Buffer& quadDeviceBuffer, VkDeviceSize indexOffset, const Swapchain& swapchain);

void error_callback(int error, const char* message)
{
  std::cout << "GLFW error: (" << error << "): " << message << std::endl;
}

void validate(boost::any& v, const std::vector<std::string>& values, std::vector<Position>*, int)
{
  static boost::regex r("(\\d+),(\\d+)");

  std::vector<Position> positions;

  for (auto& value : values)
  {
    boost::smatch match;
    if (boost::regex_match(value, match, r))
    {
      positions.push_back({ boost::lexical_cast<float>(match[1]), boost::lexical_cast<float>(match[2]) });
    }
  }

  v = positions;
}

bool ReadSettings(int argc, char** argv, Settings* settings);

int main(int argc, char** argv)
{
  Settings settings;
  if (!ReadSettings(argc, argv, &settings))
  {
    GETOUT(1);
  }

  glfwSetErrorCallback(error_callback);
  if (glfwInit() != GLFW_TRUE)
  {
    std::cout << "could not initialize glfw" << std::endl;
    GETOUT(1)
  }

  uint32_t count;
  auto ext = glfwGetRequiredInstanceExtensions(&count);

  if (!CheckVulkanVersion(VK_API_VERSION_1_1))
  {
    std::cout << "Vulkan Version is not high enough" << std::endl;
    GETOUT(1)
  }

  auto layers = CheckInstanceLayers({ "VK_LAYER_LUNARG_standard_validation" });
  CHECK_RESULT(layers, "could not get layers");

  std::vector<const char*> required = { ext, ext + count };
  // others?

  auto extensions = CheckInstanceExtensions(required);
  CHECK_RESULT(extensions, "could not get extensions");

  auto creation = CreateInstance("Game of Life", VK_MAKE_VERSION(0, 1, 0), VK_API_VERSION_1_1, std::get<std::vector<const char*>>(layers), std::get<std::vector<const char*>>(extensions));
  CHECK_RESULT(creation, "could not create instance");

  VkInstance instance = std::get<VkInstance>(creation);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  auto window = glfwCreateWindow(WIDTH, HEIGHT, "Game of Life", nullptr, nullptr);
  auto mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
  int width, height;
  glfwGetWindowSize(window, &width, &height);

  glfwSetWindowPos(window, (mode->width - width) / 2, (mode->height - height) / 2);

  auto onScroll = [](GLFWwindow* window, double xoffset, double yoffset)
  {
    std::cout << "[ " << xoffset << ", " << yoffset << " ]" << std::endl;
  };
  glfwSetScrollCallback(window, onScroll);

  VkSurfaceKHR surface;
  auto result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
  if (result != VK_SUCCESS)
  {
    std::cout << "could not create surface: VkResult = " << VkResultToString(result) << std::endl;
    GETOUT(1)
  }

  auto physicalDeviceSelection = GetSuitablePhysicalDevice(instance, surface, { VK_KHR_SWAPCHAIN_EXTENSION_NAME });
  CHECK_RESULT(physicalDeviceSelection, "could not select physical device");

  PhysicalDevice physicalDevice = std::get<PhysicalDevice>(physicalDeviceSelection);
  if (physicalDevice == nullptr)
  {
    std::cout << "no suitable device found" << std::endl;
    GETOUT(1);
  }



  VkPhysicalDeviceFeatures features = {};
  features.fragmentStoresAndAtomics = VK_TRUE;
  auto deviceCreation = CreateLogicalDevice(physicalDevice, &features);
  CHECK_RESULT(deviceCreation, "could not create logical device");

  VkDevice device = std::get<VkDevice>(deviceCreation);
  VkQueue graphicsQueue;
  VkQueue presentationQueue;
  vkGetDeviceQueue(device, physicalDevice.graphicsQueueIndex, 0, &graphicsQueue);
  vkGetDeviceQueue(device, physicalDevice.presentationQueueIndex, 0, &presentationQueue);

  auto swapchainCreation = CreateSwapchain(physicalDevice, surface, device, { WIDTH, HEIGHT });
  CHECK_RESULT(layers, "could not create swapchain");
  Swapchain swapchain = std::get<Swapchain>(swapchainCreation);

  for (size_t i = 0; i < swapchain.images.size(); i++)
  {
    auto imageView = CreateImageView2D(device, swapchain.images[i], swapchain.format);
    CHECK_RESULT(imageView, "could not create swapchain image");

    swapchain.imageViews[i] = std::get<VkImageView>(imageView);
  }

  Image2D image1, image2;

  VkImageUsageFlags imgUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  auto imageCreation = CreateImage2D(physicalDevice, device, VK_FORMAT_R8G8B8A8_UNORM, WIDTH, HEIGHT, imgUsage, VK_IMAGE_LAYOUT_UNDEFINED);
  CHECK_RESULT(imageCreation, "could not create image1");
  image1 = std::get<Image2D>(imageCreation);

  imageCreation = CreateImage2D(physicalDevice, device, VK_FORMAT_R8G8B8A8_UNORM, WIDTH, HEIGHT, imgUsage, VK_IMAGE_LAYOUT_UNDEFINED);
  CHECK_RESULT(imageCreation, "could not create image2");
  image2 = std::get<Image2D>(imageCreation);

  std::vector<Vertex> vertices =
  {
        { { -1.0f, -1.0f, 0.0f } },
        { {  1.0f, -1.0f, 0.0f } },
        { {  1.0f,  1.0f, 0.0f } },
        { { -1.0f,  1.0f, 0.0f } },
  };
  std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };

  const VkDeviceSize vertexSize = vertices.size() * sizeof(Vertex);
  const VkDeviceSize indexSize = indices.size() * sizeof(uint16_t);
  const VkDeviceSize bufferSize = vertexSize + indexSize;

  auto hostBufferCreation = CreateBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  CHECK_RESULT(hostBufferCreation, "could not create host data buffer");
  Buffer hostBuffer = std::get<Buffer>(hostBufferCreation);

  auto deviceBufferCreation = CreateBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  CHECK_RESULT(deviceBufferCreation, "could not create device data buffer");
  Buffer deviceBuffer = std::get<Buffer>(deviceBufferCreation);

  void* data;
  vkMapMemory(device, hostBuffer.memory, 0, vertexSize, 0, &data);
  memcpy(data, vertices.data(), vertexSize);
  vkUnmapMemory(device, hostBuffer.memory);

  vkMapMemory(device, hostBuffer.memory, vertexSize, indexSize, 0, &data);
  memcpy(data, indices.data(), indexSize);
  vkUnmapMemory(device, hostBuffer.memory);

  std::vector<uint32_t> positions(WIDTH * HEIGHT);

  auto duration = std::chrono::system_clock::now().time_since_epoch();
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

  boost::uniform_int<> distWidth(0, WIDTH - 1);
  boost::uniform_int<> distHeight(0, HEIGHT - 1);
  boost::mt19937 gen;
  gen.seed(uint32_t(millis));
  boost::variate_generator<boost::mt19937&, boost::uniform_int<> > dieWidth(gen, distWidth);
  boost::variate_generator<boost::mt19937&, boost::uniform_int<> > dieHeight(gen, distHeight);

  for (size_t i = 0; i < 500000; i++)
  {
    uint32_t index = dieWidth() + dieHeight() * WIDTH;

    positions[index] = 0xFFFFFFFF;
  }

  // Pulsar
  /*positions.push_back({ 5.0f, 3.0f });
  positions.push_back({ 6.0f, 3.0f });
  positions.push_back({ 7.0f, 3.0f });
  positions.push_back({ 3.0f, 5.0f });
  positions.push_back({ 3.0f, 6.0f });
  positions.push_back({ 3.0f, 7.0f });
  positions.push_back({ 5.0f, 8.0f });
  positions.push_back({ 6.0f, 8.0f });
  positions.push_back({ 7.0f, 8.0f });
  positions.push_back({ 8.0f, 5.0f });
  positions.push_back({ 8.0f, 6.0f });
  positions.push_back({ 8.0f, 7.0f });

  positions.push_back({ 10.0f, 5.0f });
  positions.push_back({ 10.0f, 6.0f });
  positions.push_back({ 10.0f, 7.0f });
  positions.push_back({ 11.0f, 8.0f });
  positions.push_back({ 12.0f, 8.0f });
  positions.push_back({ 13.0f, 8.0f });
  positions.push_back({ 15.0f, 5.0f });
  positions.push_back({ 15.0f, 6.0f });
  positions.push_back({ 15.0f, 7.0f });
  positions.push_back({ 11.0f, 3.0f });
  positions.push_back({ 12.0f, 3.0f });
  positions.push_back({ 13.0f, 3.0f });

  positions.push_back({ 11.0f, 10.0f });
  positions.push_back({ 12.0f, 10.0f });
  positions.push_back({ 13.0f, 10.0f });
  positions.push_back({ 10.0f, 11.0f });
  positions.push_back({ 10.0f, 12.0f });
  positions.push_back({ 10.0f, 13.0f });
  positions.push_back({ 11.0f, 15.0f });
  positions.push_back({ 12.0f, 15.0f });
  positions.push_back({ 13.0f, 15.0f });
  positions.push_back({ 15.0f, 11.0f });
  positions.push_back({ 15.0f, 12.0f });
  positions.push_back({ 15.0f, 13.0f });

  positions.push_back({ 5.0f, 15.0f });
  positions.push_back({ 6.0f, 15.0f });
  positions.push_back({ 7.0f, 15.0f });
  positions.push_back({ 3.0f, 11.0f });
  positions.push_back({ 3.0f, 12.0f });
  positions.push_back({ 3.0f, 13.0f });
  positions.push_back({ 5.0f, 10.0f });
  positions.push_back({ 6.0f, 10.0f });
  positions.push_back({ 7.0f, 10.0f });
  positions.push_back({ 8.0f, 11.0f });
  positions.push_back({ 8.0f, 12.0f });
  positions.push_back({ 8.0f, 13.0f });*/

  // Toad
  //positions.push_back({ 2.0f, 4.0f });
  //positions.push_back({ 3.0f, 4.0f });
  //positions.push_back({ 4.0f, 4.0f });
  //positions.push_back({ 3.0f, 3.0f });
  //positions.push_back({ 4.0f, 3.0f });
  //positions.push_back({ 5.0f, 3.0f });

  bool b = RenderInitialImage(physicalDevice, device, graphicsQueue, positions.data(), uint32_t(positions.size()), image1, hostBuffer, deviceBuffer, vertexSize, swapchain);
  if (!b)
  {
    std::cout << "could not render initial image" << std::endl;
    GETOUT(1);
  }

  auto samplerCreation = CreateSampler(device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, VK_TRUE);
  CHECK_RESULT(samplerCreation, "could not create sampler");
  VkSampler sampler = std::get<VkSampler>(samplerCreation);

  VkDescriptorSetLayoutBinding binding = CreateDescriptorSetLayoutBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  auto descriptorSetLayoutCreation = CreateDescriptorSetLayout(device, { binding });
  CHECK_RESULT(descriptorSetLayoutCreation, "could not create VkDescriptorSetLayout");
  VkDescriptorSetLayout descriptorSetLayout = std::get<VkDescriptorSetLayout>(descriptorSetLayoutCreation);

  VkDescriptorPoolSize size = CreateDescriptorPoolSize(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  auto descriptorPoolCreation = CreateDescriptorPool(device, 1, { size });
  CHECK_RESULT_BOOL(descriptorPoolCreation);
  VkDescriptorPool descriptorPool = std::get<VkDescriptorPool>(descriptorPoolCreation);

  auto piplineLayoutCreation = CreatePipelineLayout(device, { descriptorSetLayout }, {});
  CHECK_RESULT(piplineLayoutCreation, "could not create VkPipelineLayout");
  VkPipelineLayout pipelineLayout = std::get<VkPipelineLayout>(piplineLayoutCreation);

  VkDescriptorSet descriptorSet;
  result = AllocateDescriptorSets(device, descriptorPool, 1, &descriptorSetLayout, &descriptorSet);
  if (result != VK_SUCCESS)
  {
    return false;
  }

  VkDescriptorImageInfo image1Info = {};
  image1Info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image1Info.imageView = image1.view;
  image1Info.sampler = sampler;

  VkDescriptorImageInfo image2Info = {};
  image2Info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image2Info.imageView = image2.view;
  image2Info.sampler = sampler;

  Image2D *images[] = { &image2, &image1 };
  VkDescriptorImageInfo imageInfos[2] = { image2Info, image1Info };

  //auto attachment = CreateAttachementDescription(image1.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  auto attachment = CreateAttachementDescription(image1.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  std::vector<VkAttachmentReference> references = { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } };
  auto subpass = CreateSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS, references, nullptr);
  auto dependencies = CreateDefaultSubpassDependencies(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  auto renderPassCreation = CreateRenderPass(device, { attachment }, { subpass }, dependencies);
  CHECK_RESULT(renderPassCreation, "could not create renderPass (background draw)");
  auto renderPassGoL = std::get<VkRenderPass>(renderPassCreation);

  attachment = CreateAttachementDescription(image1.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  renderPassCreation = CreateRenderPass(device, { attachment }, { subpass }, dependencies);
  CHECK_RESULT(renderPassCreation, "could not create renderPass (present)");
  auto renderPass = std::get<VkRenderPass>(renderPassCreation);

  auto pipelineCreation = CreatePipeline(device, pipelineLayout, { WIDTH, HEIGHT }, renderPassGoL, "init.vert.spv", "gol.frag.spv");
  CHECK_RESULT(pipelineCreation, "could not create pipeline (background draw)");
  auto pipelineGoL = std::get<VkPipeline>(pipelineCreation);

  //pipelineCreation = CreatePipeline(device, pipelineLayout, { WIDTH, HEIGHT }, renderPass, "present.vert.spv", "present.frag.spv");
  //CHECK_RESULT(pipelineCreation, "could not create pipeline (present)");
  //auto pipeline = std::get<VkPipeline>(pipelineCreation);

  // create command buffer
  auto commandPoolCreation = CreateCommandPool(device, physicalDevice.graphicsQueueIndex);
  CHECK_RESULT(commandPoolCreation, "could not create command pool");
  auto commandPool = std::get<VkCommandPool>(commandPoolCreation);

  std::vector<VkCommandBuffer> commands;
  commands.resize(swapchain.images.size());

  result = AllocateCommandBuffer(device, commandPool, uint32_t(commands.size()), commands.data());
  if (result != VK_SUCCESS)
  {
    std::cout << "could not allocate command buffers" << std::endl;
    GETOUT(1);
  }

  VkCommandBuffer command = commands[0];

  VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
  VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
  semaphoreInfo.pNext = nullptr;
  semaphoreInfo.flags = 0;

  result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
  if (result != VK_SUCCESS)
  {
    GETOUT(1);
  }

  result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
  if (result != VK_SUCCESS)
  {
    GETOUT(1);
  }

  auto fenceCreation = CreateFence(device);
  VkFence fence = std::get<VkFence>(fenceCreation);

  std::vector<VkDescriptorImageInfo> descriptorImageInfos(1);

  auto start = std::chrono::system_clock::now();

  struct Control
  {
    int32_t fpsOffset = 0;
    bool paused = false;
  } control;


  glfwSetWindowUserPointer(window, &control);
  auto onKeyPressed = [](GLFWwindow* window, int key, int scancode, int action, int mods) -> void
  {
    Control* ctrl = (Control*)glfwGetWindowUserPointer(window);

    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
      ctrl->paused = !ctrl->paused;
    }

    if (key == GLFW_KEY_KP_ADD)
    {
      ctrl->fpsOffset += 1;
      if (ctrl->fpsOffset + FPS == 0 || ctrl->fpsOffset == FPS)
      {
        ctrl->fpsOffset += 1;
      }
    }

    if (key == GLFW_KEY_KP_SUBTRACT)
    {
      ctrl->fpsOffset -= 1;
      if (ctrl->fpsOffset + FPS == 0 || ctrl->fpsOffset == FPS)
      {
        ctrl->fpsOffset -= 1;
      }
    }
    std::cout << ctrl->fpsOffset << std::endl;
  };
  glfwSetKeyCallback(window, onKeyPressed);

  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
    {
      bool b = RenderInitialImage(physicalDevice, device, graphicsQueue, positions.data(), uint32_t(positions.size()), *images[1], hostBuffer, deviceBuffer, vertexSize, swapchain);
      if (!b)
      {
        std::cout << "could not render initial image" << std::endl;
        break;
      }
    }

    if (control.paused) continue;

    auto current = std::chrono::system_clock::now();
    auto d = current - start;
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(current - start);
    if (diff.count() <= (1000 / (FPS + control.fpsOffset)))
    {
      continue;
    }
    start = std::chrono::system_clock::now();

    vkResetCommandBuffer(command, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain.swapchain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    // swap infos
    VkDescriptorImageInfo info = imageInfos[0];
    imageInfos[0] = imageInfos[1];
    imageInfos[1] = info;

    Image2D* temp = images[0];
    images[0] = images[1];
    images[1] = temp;

    descriptorImageInfos[0] = imageInfos[0];
    auto writeDescriptorSet = CreateWriteDescriptorSet(descriptorSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {}, descriptorImageInfos);
    vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

    auto framebufferCreation = CreateFramebuffer(device, renderPass, WIDTH, HEIGHT, { images[1]->view });
    if (std::holds_alternative<VkResult>(framebufferCreation))
    {
      // bad things happened
      continue;
    }
    VkFramebuffer framebuffer = std::get<VkFramebuffer>(framebufferCreation);

    BeginCommandBuffer(command, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    BeginRenderPass(command, renderPassGoL, framebuffer, { WIDTH, HEIGHT }, { { 0.12f, 0.12f, 0.12f, 1.0f } });

    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineGoL);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(command, 0, 1, &deviceBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(command, deviceBuffer.buffer, vertexSize, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(command, 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(command);

    VkImage swapchainImage = swapchain.images[imageIndex];

    TransitionImageLayout(command, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkImageCopy imageRegion = {};
    imageRegion.srcOffset = { 0, 0, 0 };
    imageRegion.dstOffset = { 0, 0, 0 };
    imageRegion.extent = { WIDTH, HEIGHT, 1 };
    imageRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageRegion.srcSubresource.mipLevel = 0;
    imageRegion.srcSubresource.baseArrayLayer = 0;
    imageRegion.srcSubresource.layerCount = 1;
    imageRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageRegion.dstSubresource.mipLevel = 0;
    imageRegion.dstSubresource.baseArrayLayer = 0;
    imageRegion.dstSubresource.layerCount = 1;

    vkCmdCopyImage(command, images[1]->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);

    TransitionImageLayout(command, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    TransitionImageLayout(command, images[1]->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    vkEndCommandBuffer(command);

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
    VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.pNext = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence);

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain.swapchain;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentationQueue, &presentInfo);

    vkWaitForFences(device, 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    vkResetFences(device, 1, &fence);

    vkDestroyFramebuffer(device, framebuffer, nullptr);
  }



  FreeBuffer(device, hostBuffer);
  FreeBuffer(device, deviceBuffer);

  for (auto& imageView : swapchain.imageViews)
  {
    vkDestroyImageView(device, imageView, nullptr);
  }

  vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);

  glfwDestroyWindow(window);
  glfwTerminate();
  GETOUT(0)
}

bool RenderInitialImage(const PhysicalDevice& physicalDevice, VkDevice device, VkQueue graphicsQueue, uint32_t* positions, uint32_t positionCount, const Image2D& image, const Buffer& quadHostBuffer, const Buffer& quadDeviceBuffer, VkDeviceSize indexOffset, const Swapchain& swapchain)
{
  // define all needed handles, so it's easier to clean them up afterwards
  // the order is the order of allocation
  // so deallocate/free/destroy in reverse order
  VkCommandPool commandPool;
  VkCommandBuffer cmdBuffer;
  VkFence fence;

  VkDeviceSize initSize = sizeof(uint32_t) * WIDTH * HEIGHT;
  auto initBufferCreation = CreateBuffer(physicalDevice, device, initSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  CHECK_RESULT_BOOL(initBufferCreation);
  Buffer initBuffer = std::get<Buffer>(initBufferCreation);

  void* data;

  // copy position data
  VkDeviceSize positionSize = initSize;
  vkMapMemory(device, initBuffer.memory, 0, positionSize, 0, &data);
  memcpy(data, positions, positionSize);
  vkUnmapMemory(device, initBuffer.memory);


  // create command buffer
  auto commandPoolCreation = CreateCommandPool(device, physicalDevice.graphicsQueueIndex);
  CHECK_RESULT_BOOL(commandPoolCreation);
  commandPool = std::get<VkCommandPool>(commandPoolCreation);

  auto result = AllocateCommandBuffer(device, commandPool, 1, &cmdBuffer);
  if (result != VK_SUCCESS)
  {
    return false;
  }

  // record command buffer
  result = BeginCommandBuffer(cmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  if (result != VK_SUCCESS)
  {
    return false;
  }

  // copy quad data from host to device
  VkBufferCopy bufferRegion = {};
  bufferRegion.dstOffset = 0;
  bufferRegion.srcOffset = 0;
  bufferRegion.size = quadDeviceBuffer.size;

  vkCmdCopyBuffer(cmdBuffer, quadHostBuffer.buffer, quadDeviceBuffer.buffer, 1, &bufferRegion);

  TransitionImageLayout(cmdBuffer, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkBufferImageCopy bufferImageRegion = {};
  bufferImageRegion.bufferOffset = 0;
  bufferImageRegion.bufferRowLength = 0;
  bufferImageRegion.bufferImageHeight = 0;
  bufferImageRegion.imageOffset = { 0, 0, 0 };
  bufferImageRegion.imageExtent = { WIDTH, HEIGHT, 1 };
  bufferImageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  bufferImageRegion.imageSubresource.mipLevel = 0;
  bufferImageRegion.imageSubresource.baseArrayLayer = 0;
  bufferImageRegion.imageSubresource.layerCount = 1;

  vkCmdCopyBufferToImage(cmdBuffer, initBuffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageRegion);

  TransitionImageLayout(cmdBuffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

  for (uint32_t i = 0; i < swapchain.images.size(); i++)
  {
    VkImage swapchainImage = swapchain.images[i];

    TransitionImageLayout(cmdBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkImageCopy imageRegion = {};
    imageRegion.srcOffset = { 0, 0, 0 };
    imageRegion.dstOffset = { 0, 0, 0 };
    imageRegion.extent = { WIDTH, HEIGHT, 1 };
    imageRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageRegion.srcSubresource.mipLevel = 0;
    imageRegion.srcSubresource.baseArrayLayer = 0;
    imageRegion.srcSubresource.layerCount = 1;
    imageRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageRegion.dstSubresource.mipLevel = 0;
    imageRegion.dstSubresource.baseArrayLayer = 0;
    imageRegion.dstSubresource.layerCount = 1;

    vkCmdCopyImage(cmdBuffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);

    TransitionImageLayout(cmdBuffer, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
  }

  TransitionImageLayout(cmdBuffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

  result = vkEndCommandBuffer(cmdBuffer);
  if (result != VK_SUCCESS)
  {
    return false;
  }

  // create fence for host sync
  auto fenceCreation = CreateFence(device);
  CHECK_RESULT_BOOL(fenceCreation);
  fence = std::get<VkFence>(fenceCreation);

  VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };
  VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submitInfo.pNext = nullptr;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdBuffer;
  submitInfo.pWaitDstStageMask = waitStages;

  result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence);
  if (result != VK_SUCCESS)
  {
    return false;
  }

  result = vkWaitForFences(device, 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
  if (result != VK_SUCCESS)
  {
    return false;
  }

  vkQueueWaitIdle(graphicsQueue);

  //cleanup
  vkDestroyFence(device, fence, nullptr);
  vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
  vkDestroyCommandPool(device, commandPool, nullptr);
  FreeBuffer(device, initBuffer);
  return true;
}

bool ReadSettings(int argc, char** argv, Settings* settings)
{
  po::variables_map vm;
  po::options_description options("Allowed options");
  options.add_options()
    ("Help,h", "produce help message")
    ("WindowWidth,w", po::value<uint32_t>(&settings->windowWidth)->default_value(WIDTH), "sets the window width")
    ("WindowHeight,h", po::value<uint32_t>(&settings->windowHeight)->default_value(HEIGHT), "sets the window height")
    ("FullScreen,f", po::value<bool>(&settings->fullScreen)->implicit_value(false), "if set, the window will be fullscreen with the given resolution")
    ("ImageWidth,i", po::value<uint32_t>(&settings->imageWidth)->default_value(WIDTH), "sets the image's width (the resolution of \"Game of Life\")")
    ("ImageHeight,j", po::value<uint32_t>(&settings->imageHeight)->default_value(HEIGHT), "sets the image's height (the resolution of \"Game of Life\")")
    ("UseFile,u", po::value<std::string>(), "Uses the given file filled with x and y coordinates (separated with ',') as initial pixel positions")
    ("Random,r", po::value<uint32_t>(), "Creates the given amount of random initial positions")
    ("Lua,l", po::value<std::string>(), "Reads the configuration from the lua file")
    ("Pixels,p", po::value<std::vector<Position>>(&settings->positions)->multitoken()->zero_tokens()->composing(), "positions of pixels which will be set initialilly to kick of \"Game of Life\"");

  std::cout << options << "\n";

  po::store(po::command_line_parser(argc, argv).options(options).run(), vm);

  if (vm.count("Help"))
  {
    std::cout << options << "\n";
    return false;
  }

  try
  {
    po::notify(vm);
  }
  catch (const boost::program_options::required_option & e)
  {

    return false;
  }
  catch (std::exception& e)
  {
    std::cerr << "Error: " << e.what() << "\n";
    return false;
  }
  catch (...)
  {
    std::cerr << "Unknown error!" << "\n";
    return false;
  }

  if (vm.count("UseFile"))
  {
    std::vector<std::string> lines;

    std::string line;
    std::ifstream f(vm["UseFile"].as<std::string>());
    if (!f.is_open())
    {
      perror("error while opening file");
      return false;
    }

    while (std::getline(f, line))
    {
      // remove all whitespaces
      // thx: https://stackoverflow.com/a/83538
      line.erase(std::remove_if(line.begin(), line.end(), std::isspace), line.end());

      if (line.length() > 0)
      {
        lines.push_back(line);
      }
    }

    if (f.bad())
    {
      perror("error while reading file");
      return false;
    }

    boost::any result;
    validate(result, lines, nullptr, 0);

    settings->positions = boost::any_cast<std::vector<Position>>(result);
  }
  else if (vm.count("Random"))
  {
    uint32_t count = vm["Random"].as<uint32_t>();
    settings->positions.resize(count);

    boost::mt19937 gen;

    auto duration = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    gen.seed(uint32_t(millis));

    boost::uniform_int<> distWidth(0, WIDTH - 1);
    boost::uniform_int<> distHeight(0, HEIGHT - 1);
    boost::variate_generator<boost::mt19937&, boost::uniform_int<> > dieWidth(gen, distWidth);
    boost::variate_generator<boost::mt19937&, boost::uniform_int<> > dieHeight(gen, distHeight);

    for (size_t i = 0; i < count; i++)
    {
      settings->positions[i] = { float(dieWidth()), float(dieHeight()) };
    }
  }


  return true;
}