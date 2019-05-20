#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
#include "Camera.h"
#include "GameOfLifeVulkan.h"

#if _WIN32
#include <conio.h>

#define GETOUT(ret) _getch(); return ret;
#endif

namespace po = boost::program_options;

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;
constexpr int32_t FPS = 15;

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

bool RenderInitialImage(const PhysicalDevice& physicalDevice, VkDevice device, VkQueue graphicsQueue, uint32_t* positions, uint32_t positionCount, const Image2D& image, const Buffer& quadHostBuffer, const Buffer& quadDeviceBuffer, VkDeviceSize indexOffset, const Settings& settings);

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
      positions.push_back({ boost::lexical_cast<uint32_t>(match[1]), boost::lexical_cast<uint32_t>(match[2]) });
    }
  }

  v = positions;
}

bool ReadSettings(int argc, char** argv, Settings* settings);

std::ostream& operator<<(std::ostream& out, const glm::vec4& g)
{
  return out << "[" << g.x << ", " << g.y << ", " << g.z << ", " << g.w << "]";
}

std::ostream& operator<<(std::ostream& out, const glm::vec3& g)
{
  return out << "[" << g.x << ", " << g.y << ", " << g.z << "]";
}

int main(int argc, char** argv)
{
  PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;

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
  required.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  // others?

  auto extensions = CheckInstanceExtensions(required);
  CHECK_RESULT(extensions, "could not get extensions");

  auto creation = CreateInstance("Game of Life", VK_MAKE_VERSION(0, 1, 0), VK_API_VERSION_1_1, std::get<std::vector<const char*>>(layers), std::get<std::vector<const char*>>(extensions));
  CHECK_RESULT(creation, "could not create instance");

  VkInstance instance = std::get<VkInstance>(creation);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  auto window = glfwCreateWindow(settings.windowWidth, settings.windowHeight, "Game of Life", nullptr, nullptr);
  auto mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

  glfwSetWindowPos(window, (mode->width - settings.windowWidth) / 2, (mode->height - settings.windowHeight) / 2);

  VkSurfaceKHR surface;
  auto result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
  if (result != VK_SUCCESS)
  {
    std::cout << "could not create surface: VkResult = " << VkResultToString(result) << std::endl;
    GETOUT(1)
  }

  auto physicalDeviceSelection = GetSuitablePhysicalDevice(instance, surface, { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME });
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

  vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR");
  if (!vkCmdPushDescriptorSetKHR)
  {
    std::cout << "could not get function vkCmdPushDescriptorSetKHR" << std::endl;
    GETOUT(1);
  }

  PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2KHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR"));
  if (!vkGetPhysicalDeviceProperties2KHR)
  {
    std::cout << "could not get function vkCmdPushDescriptorSetKHR" << std::endl;
    GETOUT(1);
  }

  VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProps = {};
  VkPhysicalDeviceProperties2KHR deviceProps2 = {};
  pushDescriptorProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
  deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
  deviceProps2.pNext = &pushDescriptorProps;
  vkGetPhysicalDeviceProperties2KHR(physicalDevice, &deviceProps2);

  auto swapchainCreation = CreateSwapchain(physicalDevice, surface, device, { settings.windowWidth, settings.windowHeight });
  CHECK_RESULT(layers, "could not create swapchain");
  Swapchain swapchain = std::get<Swapchain>(swapchainCreation);

  for (size_t i = 0; i < swapchain.images.size(); i++)
  {
    auto imageView = CreateImageView2D(device, swapchain.images[i], swapchain.format);
    CHECK_RESULT(imageView, "could not create swapchain image");

    swapchain.imageViews[i] = std::get<VkImageView>(imageView);
  }

  Ubo ubo = {};

  Camera camera = { swapchain.extent, { 0, 0, -10 } };
  ubo.mat = camera.WorldToScreenMatrix();

  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

  struct Control
  {
    int32_t fpsOffset = 0;
    bool paused = false;
    glm::vec2 lastMousePos;
    Camera* cam;
    Settings* settings;
  } control;
  control.cam = &camera;
  control.settings = &settings;

  glfwSetWindowUserPointer(window, &control);

  auto onScroll = [](GLFWwindow* window, double xoffset, double yoffset)
  {
    Control* ctrl = (Control*)glfwGetWindowUserPointer(window);
    ctrl->cam->Move({ 0, 0, float(yoffset) / 4.0f });
  };
  glfwSetScrollCallback(window, onScroll);


  auto onMouseMove = [](GLFWwindow* window, double xpos, double ypos)
  {
    Control* ctrl = (Control*)glfwGetWindowUserPointer(window);
    
    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS)
    {
      auto screen = glm::vec2(float(xpos), float(ypos));

      auto last = ctrl->cam->ScreenToWorld(uint32_t(ctrl->lastMousePos.x), uint32_t(ctrl->lastMousePos.y));
      auto world = ctrl->cam->ScreenToWorld(uint32_t(screen.x), uint32_t(screen.y));
      
      auto delta = last - world;
      //std::cout << delta << std::endl;
      delta.z = 0.0;

      ctrl->cam->Move(delta);

      ctrl->lastMousePos = screen;
    }
    else
    {
      ctrl->lastMousePos.x = float(xpos);
      ctrl->lastMousePos.y = float(ypos);
    }
  };
  glfwSetCursorPosCallback(window, onMouseMove);

  Image2D image1, image2;

  VkImageUsageFlags imgUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  auto imageCreation = CreateImage2D(physicalDevice, device, VK_FORMAT_R8G8B8A8_UNORM, settings.imageWidth, settings.imageHeight, imgUsage, VK_IMAGE_LAYOUT_UNDEFINED);
  CHECK_RESULT(imageCreation, "could not create image1");
  image1 = std::get<Image2D>(imageCreation);

  imageCreation = CreateImage2D(physicalDevice, device, VK_FORMAT_R8G8B8A8_UNORM, settings.imageWidth, settings.imageHeight, imgUsage, VK_IMAGE_LAYOUT_UNDEFINED);
  CHECK_RESULT(imageCreation, "could not create image2");
  image2 = std::get<Image2D>(imageCreation);


  auto uboBufferCreation = CreateBuffer(physicalDevice, device, sizeof(Ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  CHECK_RESULT(uboBufferCreation, "could not create ubo buffer");
  Buffer uboBuffer = std::get<Buffer>(uboBufferCreation);

  float imageOffsetX = 0.0f, imageOffsetY = 0.0f;
  //float ratio = float(settings.imageWidth) / float(settings.imageHeight);
  float ratio = float(settings.imageHeight) / float(settings.imageWidth);
  float rx = std::max(float(settings.imageWidth) / float(settings.windowWidth), 1.0f);
  float ry = std::max(float(settings.imageHeight) / float(settings.windowHeight), 1.0f);

  if (settings.imageWidth < settings.windowWidth)
  {
    imageOffsetX = 1.0f - (float(settings.windowWidth - settings.imageWidth) / float(settings.windowWidth));
  }

  if (settings.imageHeight < settings.windowHeight)
  {
    imageOffsetY = 1.0f - (float(settings.windowHeight - settings.imageHeight) / float(settings.windowHeight));
  }

  std::vector<Vertex> vertices =
  {
    // fullscreen quad
    { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
    { {  1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
    { {  1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } },
    { { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } },

    //image rendering quad
    //{ { -ratio, -1.0f, 0.0f}, {1.0f, 0.0f}},
    //{ {  ratio, -1.0f, 0.0f}, {0.0f, 0.0f}},
    //{ {  ratio,  1.0f, 0.0f}, {0.0f, 1.0f}},
    //{ { -ratio,  1.0f, 0.0f}, {1.0f, 1.0f}},
    { { -1.0f, -ratio * 2.0f, 0.0f}, {1.0f, 0.0f}},
    { {  1.0f, -ratio * 2.0f, 0.0f}, {0.0f, 0.0f}},
    { {  1.0f,  ratio * 2.0f, 0.0f}, {0.0f, 1.0f}},
    { { -1.0f,  ratio * 2.0f, 0.0f}, {1.0f, 1.0f}}
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

  std::vector<uint32_t> positions(settings.imageWidth * settings.imageHeight);

  for (auto& pos : settings.positions)
  {
    uint32_t index = pos.x + pos.y * settings.imageWidth;

    positions[index] = 0xFFFFFFFF;
  }

  bool b = RenderInitialImage(physicalDevice, device, graphicsQueue, positions.data(), uint32_t(positions.size()), image1, hostBuffer, deviceBuffer, vertexSize, settings);
  if (!b)
  {
    std::cout << "could not render initial image" << std::endl;
    GETOUT(1);
  }

  auto samplerCreation = CreateSampler(device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, VK_TRUE);
  CHECK_RESULT(samplerCreation, "could not create sampler");
  VkSampler sampler = std::get<VkSampler>(samplerCreation);

  samplerCreation = CreateSampler(device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, VK_FALSE);
  CHECK_RESULT(samplerCreation, "could not create sampler");
  VkSampler presentSampler = std::get<VkSampler>(samplerCreation);

  VkDescriptorSetLayoutBinding binding = CreateDescriptorSetLayoutBinding(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  auto descriptorSetLayoutCreation = CreateDescriptorSetLayout(device, { binding });
  CHECK_RESULT(descriptorSetLayoutCreation, "could not create VkDescriptorSetLayout");
  VkDescriptorSetLayout descriptorSetLayout = std::get<VkDescriptorSetLayout>(descriptorSetLayoutCreation);

  auto piplineLayoutCreation = CreatePipelineLayout(device, { descriptorSetLayout }, {});
  CHECK_RESULT(piplineLayoutCreation, "could not create VkPipelineLayout");
  VkPipelineLayout pipelineLayout = std::get<VkPipelineLayout>(piplineLayoutCreation);

  VkDescriptorSetLayoutBinding uboBinding = CreateDescriptorSetLayoutBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
  VkDescriptorSetLayoutBinding textureBinding = CreateDescriptorSetLayoutBinding(1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

  descriptorSetLayoutCreation = CreateDescriptorSetLayout(device, { uboBinding, textureBinding });
  CHECK_RESULT(descriptorSetLayoutCreation, "could not create VkDescriptorSetLayout");
  VkDescriptorSetLayout presentDescriptorSetLayout = std::get<VkDescriptorSetLayout>(descriptorSetLayoutCreation);

  piplineLayoutCreation = CreatePipelineLayout(device, { presentDescriptorSetLayout }, {});
  CHECK_RESULT(piplineLayoutCreation, "could not create VkPipelineLayout");
  VkPipelineLayout presentPipelineLayout = std::get<VkPipelineLayout>(piplineLayoutCreation);

  VkDescriptorImageInfo golImageDescriptor = {};
  golImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  golImageDescriptor.imageView = image2.view;
  golImageDescriptor.sampler = sampler;

  VkDescriptorImageInfo presentImageDescriptor = {};
  presentImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  presentImageDescriptor.imageView = image1.view;
  presentImageDescriptor.sampler = presentSampler;

  VkDescriptorBufferInfo uboBufferInfo = {};
  uboBufferInfo.offset = 0;
  uboBufferInfo.range = sizeof(Ubo);
  uboBufferInfo.buffer = uboBuffer.buffer;

  Image2D *images[] = { &image2, &image1 };

  auto attachment = CreateAttachementDescription(image1.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  //auto attachment = CreateAttachementDescription(image1.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  std::vector<VkAttachmentReference> references = { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } };
  auto subpass = CreateSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS, references, nullptr);
  auto dependencies = CreateDefaultSubpassDependencies(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  auto renderPassCreation = CreateRenderPass(device, { attachment }, { subpass }, dependencies);
  CHECK_RESULT(renderPassCreation, "could not create renderPass (background draw)");
  auto renderPassGoL = std::get<VkRenderPass>(renderPassCreation);

  attachment = CreateAttachementDescription(swapchain.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  renderPassCreation = CreateRenderPass(device, { attachment }, { subpass }, dependencies);
  CHECK_RESULT(renderPassCreation, "could not create renderPass (present)");
  auto renderPass = std::get<VkRenderPass>(renderPassCreation);

  auto pipelineCreation = CreatePipeline(device, pipelineLayout, { settings.imageWidth, settings.imageHeight }, renderPassGoL, "init.vert.spv", "gol.frag.spv");
  CHECK_RESULT(pipelineCreation, "could not create pipeline (background draw)");
  auto pipelineGoL = std::get<VkPipeline>(pipelineCreation);

  pipelineCreation = CreatePipeline(device, presentPipelineLayout, { settings.windowWidth, settings.windowHeight }, renderPass, "present.vert.spv", "present.frag.spv");
  CHECK_RESULT(pipelineCreation, "could not create pipeline (present)");
  auto pipeline = std::get<VkPipeline>(pipelineCreation);

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

  std::vector<VkDescriptorImageInfo> descriptorImageInfos = { golImageDescriptor };

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

  auto start = std::chrono::system_clock::now();
  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
    {
      bool b = RenderInitialImage(physicalDevice, device, graphicsQueue, positions.data(), uint32_t(positions.size()), *images[1], hostBuffer, deviceBuffer, vertexSize, settings);
      if (!b)
      {
        std::cout << "could not render initial image" << std::endl;
        break;
      }
    }

    if (control.paused) continue;

    auto current = std::chrono::system_clock::now();
    auto d = current - start;
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(d);


    // write ubo
    ubo.mat = camera.WorldToScreenMatrix();

    void* uboData;
    vkMapMemory(device, uboBuffer.memory, 0, sizeof(Ubo), 0, &uboData);
    memcpy(uboData, &ubo, sizeof(Ubo));
    vkUnmapMemory(device, uboBuffer.memory);

    vkResetCommandBuffer(command, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain.swapchain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    auto framebufferCreation = CreateFramebuffer(device, renderPass, settings.windowWidth, settings.windowHeight, { swapchain.imageViews[imageIndex] });
    if (std::holds_alternative<VkResult>(framebufferCreation))
    {
      // bad things happened
      continue;
    }
    VkFramebuffer frontbuffer = std::get<VkFramebuffer>(framebufferCreation);
    VkFramebuffer backbuffer = VK_NULL_HANDLE;


    VkDeviceSize offsets[1] = { 0 };
    BeginCommandBuffer(command, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    if (diff.count() >= (1000 / (FPS + control.fpsOffset)))
    {
      // swap infos
      Image2D* temp = images[0];
      images[0] = images[1];
      images[1] = temp;

      descriptorImageInfos[0].imageView = images[0]->view;
      presentImageDescriptor.imageView = images[1]->view;

      auto framebufferCreation = CreateFramebuffer(device, renderPassGoL, settings.imageWidth, settings.imageHeight, { images[1]->view });
      if (std::holds_alternative<VkResult>(framebufferCreation))
      {
        // bad things happened
        continue;
      }
      backbuffer = std::get<VkFramebuffer>(framebufferCreation);

      auto writeDescriptorSet = CreateWriteDescriptorSet(VK_NULL_HANDLE, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {}, descriptorImageInfos);

      BeginRenderPass(command, renderPassGoL, backbuffer, { settings.imageWidth, settings.imageHeight }, { { 0.0f, 0.0f, 0.0f, 1.0f } });

      vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineGoL);
      vkCmdPushDescriptorSetKHR(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &writeDescriptorSet);

      vkCmdBindVertexBuffers(command, 0, 1, &deviceBuffer.buffer, offsets);
      vkCmdBindIndexBuffer(command, deviceBuffer.buffer, vertexSize, VK_INDEX_TYPE_UINT16);

      vkCmdDrawIndexed(command, 6, 1, 0, 0, 0);

      vkCmdEndRenderPass(command);

      start = std::chrono::system_clock::now();
    }

    std::array<VkWriteDescriptorSet, 2> writeDescriptorSets = {};
    writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[0].dstSet = 0;
    writeDescriptorSets[0].dstBinding = 0;
    writeDescriptorSets[0].descriptorCount = 1;
    writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSets[0].pBufferInfo = &uboBufferInfo;

    writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[1].dstSet = 0;
    writeDescriptorSets[1].dstBinding = 1;
    writeDescriptorSets[1].descriptorCount = 1;
    writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSets[1].pImageInfo = &presentImageDescriptor;

    BeginRenderPass(command, renderPass, frontbuffer, { settings.windowWidth, settings.windowHeight }, { { 0.12f, 0.12f, 0.12f, 1.0f } });
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdPushDescriptorSetKHR(command, VK_PIPELINE_BIND_POINT_GRAPHICS, presentPipelineLayout, 0, 2, writeDescriptorSets.data());

    offsets[0] = { sizeof(Vertex) * 4 }; // skip first 4 vertices
    vkCmdBindVertexBuffers(command, 0, 1, &deviceBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(command, deviceBuffer.buffer, vertexSize, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(command, 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(command);

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

    vkDestroyFramebuffer(device, frontbuffer, nullptr);

    if (backbuffer != VK_NULL_HANDLE)
    {
      vkDestroyFramebuffer(device, backbuffer, nullptr);
    }
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

bool RenderInitialImage(const PhysicalDevice& physicalDevice, VkDevice device, VkQueue graphicsQueue, uint32_t* positions, uint32_t positionCount, const Image2D& image, const Buffer& quadHostBuffer, const Buffer& quadDeviceBuffer, VkDeviceSize indexOffset, const Settings& settings)
{
  // define all needed handles, so it's easier to clean them up afterwards
  // the order is the order of allocation
  // so deallocate/free/destroy in reverse order
  VkCommandPool commandPool;
  VkCommandBuffer cmdBuffer;
  VkFence fence;

  VkDeviceSize initSize = sizeof(uint32_t) * positionCount;
  auto initBufferCreation = CreateBuffer(physicalDevice, device, initSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  CHECK_RESULT_BOOL(initBufferCreation);
  Buffer initBuffer = std::get<Buffer>(initBufferCreation);



  // copy position data
  void* data;
  vkMapMemory(device, initBuffer.memory, 0, initSize, 0, &data);
  memcpy(data, positions, initSize);
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
  bufferImageRegion.imageExtent = { settings.imageWidth, settings.imageHeight, 1 };
  bufferImageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  bufferImageRegion.imageSubresource.mipLevel = 0;
  bufferImageRegion.imageSubresource.baseArrayLayer = 0;
  bufferImageRegion.imageSubresource.layerCount = 1;

  vkCmdCopyBufferToImage(cmdBuffer, initBuffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageRegion);

  TransitionImageLayout(cmdBuffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

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

  //std::cout << options << "\n";

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

    boost::uniform_int<> widthDistribution(0, settings->imageWidth - 1);
    boost::uniform_int<> heightDistribution(0, settings->imageHeight - 1);
    boost::variate_generator<boost::mt19937&, boost::uniform_int<> > widthRandom(gen, widthDistribution);
    boost::variate_generator<boost::mt19937&, boost::uniform_int<> > heightRandom(gen, heightDistribution);

    for (size_t i = 0; i < count; i++)
    {
      settings->positions[i] = { uint32_t(widthRandom()), uint32_t(heightRandom()) };
    }
  }


  return true;
}
