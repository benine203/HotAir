#pragma once
static_assert(__cplusplus >= 202002L, "C++20 required");

#include "../args.hpp"
#include "platformGfx.hpp"
#include <functional>
#include <iostream>
#include <vulkan/vulkan.hpp>

/**
 *   Needs a platform-specific implementation to abstract-over details like
 *   windowing/system-specific details (geometry, surface, etc.)
 */
struct VulkanGfxBase {
protected:
  PlatformGfx *platformVkImpl;

  /* createInstance */
  vk::Instance instance;

  /* pickPhysicalDevice */
  std::vector<vk::PhysicalDevice> devices;
  vk::PhysicalDevice physicalDevice;

  /* createDevice */
  vk::Device device;
  vk::Queue queue;
  vk::Queue transferQueue;
  vk::Queue presentQueue;
  vk::Queue computeQueue;

  /**
   * filled (and consumed) by createDevice
   * consumed by createSwapchain and createCommandPools
   */
  struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> transferFamily;
    std::optional<uint32_t> computeFamily;

    [[nodiscard]] auto isComplete() const {
      return graphicsFamily.has_value() && presentFamily.has_value() &&
             transferFamily.has_value() && computeFamily.has_value();
    }
  } queueFamilyIndices; // also consumed by create fn for Swapchain and
                        // commandPools

  /* user-supplied callback */
  vk::SurfaceKHR *surface; // this is a pointer because it's created by the
                           // platform-specific code

  /* createSwapchain */
  vk::SwapchainKHR swapchain;
  vk::Format format;
  vk::Extent2D extent;
  std::vector<vk::Image> images;

  /* createImageViews */
  std::vector<vk::ImageView> imageViews;

  /* createRenderPass */
  vk::RenderPass renderPass;

  /* createGraphicsPipeline */
  vk::PipelineLayout pipelineLayout;
  vk::Pipeline pipeline;

  /* createFramebuffers */
  std::vector<vk::Framebuffer> framebuffers; // as many as there are images

  /* createCommandPools */
  // vk::CommandPool commandPool;
  struct CommandPools {
    vk::CommandPool graphics;
    vk::CommandPool transfer;
    vk::CommandPool present;
    vk::CommandPool compute;
  } commandPools;

  /* createCommandBuffers */
  // std::vector<vk::CommandBuffer> commandBuffers;
  struct CommandBuffers {
    std::vector<vk::CommandBuffer> graphics;
    std::vector<vk::CommandBuffer> transfer;
    std::vector<vk::CommandBuffer> present;
    std::vector<vk::CommandBuffer> compute;
  } commandBuffers;

  /* createSyncObjects */
  vk::Semaphore imageAvailableSemaphore;
  vk::Semaphore renderFinishedSemaphore;
  vk::Fence inFlightFence;

public:
  // VulkanGfxBase() = default;
  VulkanGfxBase(PlatformGfx *platformGfxImpl)
      : platformVkImpl(platformGfxImpl) {}

  virtual ~VulkanGfxBase() {
    auto const verbose = Args::verbose();

    destroy();

    if (device) {
      device.destroy();
      device = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: Vulkan device destroyed\n", __FILE__,
                                 __LINE__);
      }
    }

    if (instance) {
      instance.destroy();
      instance = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: Vulkan instance destroyed\n", __FILE__,
                                 __LINE__);
      }
    }
  }

  void destroy() {
    auto const verbose = Args::verbose();

    if (device) {
      device.waitIdle();
    }

    if (inFlightFence) {
      device.destroyFence(inFlightFence);
      inFlightFence = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: inFlightFence destroyed\n", __FILE__,
                                 __LINE__);
      }
    }

    if (renderFinishedSemaphore) {
      device.destroySemaphore(renderFinishedSemaphore);
      renderFinishedSemaphore = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: renderFinishedSemaphore destroyed\n",
                                 __FILE__, __LINE__);
      }
    }

    if (imageAvailableSemaphore) {
      device.destroySemaphore(imageAvailableSemaphore);
      imageAvailableSemaphore = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: imageAvailableSemaphore destroyed\n",
                                 __FILE__, __LINE__);
      }
    }

    if (!commandBuffers.graphics.empty()) {
      device.freeCommandBuffers(commandPools.graphics,
                                commandBuffers.graphics.size(),
                                commandBuffers.graphics.data());
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: {} graphics command buffer(s) freed\n",
                                 __FILE__, __LINE__,
                                 commandBuffers.graphics.size());
      }
      commandBuffers.graphics.clear();
    }

    if (!commandBuffers.transfer.empty()) {
      device.freeCommandBuffers(commandPools.transfer,
                                commandBuffers.transfer.size(),
                                commandBuffers.transfer.data());
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: {} transfer command buffer(s) freed\n",
                                 __FILE__, __LINE__,
                                 commandBuffers.transfer.size());
      }
      commandBuffers.transfer.clear();
    }

    if (!commandBuffers.present.empty()) {
      device.freeCommandBuffers(commandPools.present,
                                commandBuffers.present.size(),
                                commandBuffers.present.data());
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: {} present command buffer(s) freed\n",
                                 __FILE__, __LINE__,
                                 commandBuffers.present.size());
      }
      commandBuffers.present.clear();
    }

    if (!commandBuffers.compute.empty()) {
      device.freeCommandBuffers(commandPools.compute,
                                commandBuffers.compute.size(),
                                commandBuffers.compute.data());
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: {} compute command buffer(s) freed\n",
                                 __FILE__, __LINE__,
                                 commandBuffers.compute.size());
      }
      commandBuffers.compute.clear();
    }

    if (commandPools.compute) {
      device.destroyCommandPool(commandPools.compute);
      commandPools.compute = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: compute command pool destroyed\n",
                                 __FILE__, __LINE__);
      }
    }

    if (commandPools.present) {
      device.destroyCommandPool(commandPools.present);
      commandPools.present = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: present command pool destroyed\n",
                                 __FILE__, __LINE__);
      }
    }

    if (commandPools.transfer) {
      device.destroyCommandPool(commandPools.transfer);
      commandPools.transfer = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: transfer command pool destroyed\n",
                                 __FILE__, __LINE__);
      }
    }

    if (commandPools.graphics) {
      device.destroyCommandPool(commandPools.graphics);
      commandPools.graphics = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: graphics command pool destroyed\n",
                                 __FILE__, __LINE__);
      }
    }

    if (pipeline) {
      device.destroyPipeline(pipeline);
      pipeline = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: pipeline destroyed\n", __FILE__,
                                 __LINE__);
      }
    }

    if (pipelineLayout) {
      device.destroyPipelineLayout(pipelineLayout);
      pipelineLayout = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: pipeline layout destroyed\n", __FILE__,
                                 __LINE__);
      }
    }

    if (!framebuffers.empty()) {
      for (auto &framebuffer : framebuffers) {
        device.destroyFramebuffer(framebuffer);
      }
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: {} framebuffers destroyed\n", __FILE__,
                                 __LINE__, framebuffers.size());
      }
      framebuffers.clear();
    }

    if (renderPass) {
      device.destroyRenderPass(renderPass);
      renderPass = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: render pass destroyed\n", __FILE__,
                                 __LINE__);
      }
    }

    if (!imageViews.empty()) {
      for (auto &imageView : imageViews) {
        device.destroyImageView(imageView);
      }
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: {} image views destroyed\n", __FILE__,
                                 __LINE__, imageViews.size());
      }
      imageViews.clear();
    }

    if (swapchain) {
      device.destroySwapchainKHR(swapchain);
      swapchain = nullptr;
      if (verbose > 1) {
        std::cerr << std::format("{}:{}: swapchain destroyed\n", __FILE__,
                                 __LINE__);
      }
    }
  }

  void init(const std::function<vk::SurfaceKHR *()> create_surface_fn) {
    createInstance();

    pickPhysicalDevice();

    if (!surface) {
      if (!create_surface_fn)
        throw std::runtime_error{std::format(
            "{}:{}: create_surface fn must be a valid cb", __FILE__, __LINE__)};
      surface = create_surface_fn();
    } else {
      if (Args::verbose() > 0) {
        std::cerr << std::format(
            "{}:{}: skipping platform vk Surface (re-)creation\n", __FILE__,
            __LINE__);
      }
    }

    if (Args::verbose() > 1) {
      auto const capabilities =
          physicalDevice.getSurfaceCapabilitiesKHR(*surface);

      std::cerr << "Surface capabilities:\n";
      std::cerr << "  minImageCount: " << capabilities.minImageCount << '\n';
      std::cerr << "  maxImageCount: " << capabilities.maxImageCount << '\n';
      std::cerr << "  currentExtent: " << capabilities.currentExtent.width
                << 'x' << capabilities.currentExtent.height << '\n';
      std::cerr << "  minImageExtent: " << capabilities.minImageExtent.width
                << 'x' << capabilities.minImageExtent.height << '\n';
      std::cerr << "  maxImageExtent: " << capabilities.maxImageExtent.width
                << 'x' << capabilities.maxImageExtent.height << '\n';
      std::cerr << "  maxImageArrayLayers: " << capabilities.maxImageArrayLayers
                << '\n';
      std::cerr << "  supportedTransforms: "
                << vk::to_string(capabilities.supportedTransforms) << '\n';
      std::cerr << "  currentTransform: "
                << vk::to_string(capabilities.currentTransform) << '\n';
      std::cerr << "  supportedCompositeAlpha: "
                << vk::to_string(capabilities.supportedCompositeAlpha) << '\n';
      std::cerr << "  supportedUsageFlags: "
                << vk::to_string(capabilities.supportedUsageFlags) << '\n';
    }

    createDevice();

    createSwapchain();

    createImageViews();

    createRenderPass();

    // createGraphicsPipeline();

    createFramebuffers();

    createCommandPools();

    createCommandBuffers();

    createSyncObjects();
  }

private:
  /**
   *  Create a Vulkan instance
   *  Instance is the connection between the application and the Vulkan library
   */
  void createInstance() {
    if (instance) {
      if (Args::verbose() > 0) {
        std::cerr << std::format("{}:{}:{}: skipping vk instance creation\n",
                                 __FILE__, __LINE__, __PRETTY_FUNCTION__);
      }
      return;
    }

    vk::ApplicationInfo appInfo("HotAir", VK_MAKE_VERSION(1, 0, 0), "Baloon",
                                VK_MAKE_VERSION(1, 0, 0));

    auto extensions = vk::enumerateInstanceExtensionProperties();

    if (Args::verbose() > 1) {
      std::cerr << "available extensions:\n";
      for (auto const &extension : extensions) {
        std::cerr << extension.extensionName << '\n';
      }
    }

    auto names = std::vector<const char *>(extensions.size());
    for (auto i = 0u; i < extensions.size(); ++i) {
      names[i] = extensions[i].extensionName;
    }

    auto createInfo = vk::InstanceCreateInfo();
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = names.size();
    createInfo.ppEnabledExtensionNames = names.data();

    instance = vk::createInstance(createInfo);
    if (!instance) {
      throw std::runtime_error("failed to create instance");
    }

    std::cerr << "Base Vulkan instance created\n";
  }

  /**
   * Select a physical device to use
   * Preconditions: vk instance created
   * @TODO: Add a way to select a device based on user preferences, now just
   *        find a GPU with geometry shader support
   */
  void pickPhysicalDevice() {
    assert(instance);

    if (physicalDevice) {
      if (Args::verbose() > 0) {
        std::cerr << std::format(
            "{}:{}:{}: skipping physical device selection\n", __FILE__,
            __LINE__, __PRETTY_FUNCTION__);
      }
      return;
    }

    devices = instance.enumeratePhysicalDevices();
    if (devices.empty()) {
      throw std::runtime_error("failed to find GPUs with Vulkan support");
    }

    auto const device = std::ranges::find_if(devices, [](auto const &device) {
      auto const properties = device.getProperties();
      auto const features = device.getFeatures();

      if (Args::verbose() > 0) {
        std::cerr << "Device properties:\n";
        std::cerr << "  Device name: " << properties.deviceName << '\n';
        std::cerr << "  Device type: " << vk::to_string(properties.deviceType)
                  << '\n';
        std::cerr << "  API version: " << properties.apiVersion << '\n';
        std::cerr << "  Driver version: " << properties.driverVersion << '\n';
        std::cerr << "  Vendor ID: " << properties.vendorID << '\n';
        std::cerr << "  Device ID: " << properties.deviceID << '\n';
        std::cerr << "  Pipeline cache UUID: ";
        for (auto const &byte : properties.pipelineCacheUUID) {
          std::cerr << std::format("{:02x}", byte);
        }
        std::cerr << '\n';
      }

      if (Args::verbose() > 1) {
        std::cerr << "Device features:\n";
        std::cerr << "  robustBufferAccess: " << features.robustBufferAccess
                  << '\n';
        std::cerr << "  fullDrawIndexUint32: " << features.fullDrawIndexUint32
                  << '\n';
        std::cerr << "  imageCubeArray: " << features.imageCubeArray << '\n';
        std::cerr << "  independentBlend: " << features.independentBlend
                  << '\n';
        std::cerr << "  geometryShader: " << features.geometryShader << '\n';
        std::cerr << "  tessellationShader: " << features.tessellationShader
                  << '\n';
        std::cerr << "  sampleRateShading: " << features.sampleRateShading
                  << '\n';
        std::cerr << "  dualSrcBlend: " << features.dualSrcBlend << '\n';
        std::cerr << "  logicOp: " << features.logicOp << '\n';
        std::cerr << "  multiDrawIndirect: " << features.multiDrawIndirect
                  << '\n';
        std::cerr << "  drawIndirectFirstInstance: "
                  << features.drawIndirectFirstInstance << '\n';
        std::cerr << "  depthClamp: " << features.depthClamp << '\n';
        std::cerr << "  depthBiasClamp: " << features.depthBiasClamp << '\n';
      }

      return (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ||
              properties.deviceType ==
                  vk::PhysicalDeviceType::eIntegratedGpu) &&
             features.geometryShader;
    });

    if (device == devices.end()) {
      throw std::runtime_error("failed to find a suitable GPU");
    }

    physicalDevice = *device;
    std::cerr << std::format("{}:{}: Physical device selected: {}\n", __FILE__,
                             __LINE__,
                             physicalDevice.getProperties().deviceName.data());
  }

  void createImageViews() {
    imageViews.resize(images.size());

    for (auto i = 0u; i < images.size(); ++i) {
      auto createInfo = vk::ImageViewCreateInfo();
      createInfo.image = images[i];
      createInfo.viewType = vk::ImageViewType::e2D;
      createInfo.format = format;
      createInfo.components.r = vk::ComponentSwizzle::eIdentity;
      createInfo.components.g = vk::ComponentSwizzle::eIdentity;
      createInfo.components.b = vk::ComponentSwizzle::eIdentity;
      createInfo.components.a = vk::ComponentSwizzle::eIdentity;
      createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;

      imageViews[i] = device.createImageView(createInfo);
      if (!imageViews[i]) {
        throw std::runtime_error("failed to create image views");
      }
    }

    if (Args::verbose() > 0)
      std::cerr << std::format("{}:{}: Image views created for {} images\n",
                               __FILE__, __LINE__, images.size());
  }

  void createRenderPass() {
    auto colorAttachment = vk::AttachmentDescription();
    colorAttachment.format = format;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    auto colorAttachmentRef = vk::AttachmentReference();
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    auto subpass = vk::SubpassDescription();
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    auto dependency = vk::SubpassDependency();
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead |
                               vk::AccessFlagBits::eColorAttachmentWrite;

    auto renderPassInfo = vk::RenderPassCreateInfo();
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    renderPass = device.createRenderPass(renderPassInfo);
    if (!renderPass) {
      throw std::runtime_error("failed to create render pass");
    }

    if (Args::verbose() > 0)
      std::cerr << std::format("{}:{}: Render pass created\n", __FILE__,
                               __LINE__);
  }

  void createGraphicsPipeline() {
    // auto vertShaderCode = std::vector<char>{
    //   #include "shaders/vert.spv"
    // };
    // auto fragShaderCode = std::vector<char>{
    //   #include "shaders/frag.spv"
    // };

    // auto vertShaderModule = createShaderModule(vertShaderCode);
    // auto fragShaderModule = createShaderModule(fragShaderCode);

    // auto vertShaderStageInfo = vk::PipelineShaderStageCreateInfo();
    // vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    // vertShaderStageInfo.module = vertShaderModule;
    // vertShaderStageInfo.pName = "main";

    // auto fragShaderStageInfo = vk::PipelineShaderStageCreateInfo();
    // fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    // fragShaderStageInfo.module = fragShaderModule;
    // fragShaderStageInfo.pName = "main";

    // auto shaderStages = std::vector{vertShaderStageInfo,
    // fragShaderStageInfo};
    auto shaderStages = std::vector<vk::PipelineShaderStageCreateInfo>();

    auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo();
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo();
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = false;

    auto viewport = vk::Viewport();
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    auto scissor = vk::Rect2D();
    scissor.offset = vk::Offset2D(0, 0);
    scissor.extent = extent;

    auto viewportState = vk::PipelineViewportStateCreateInfo();
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    auto rasterizer = vk::PipelineRasterizationStateCreateInfo();
    rasterizer.depthClampEnable = false;
    rasterizer.rasterizerDiscardEnable = false;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = false;

    auto multisampling = vk::PipelineMultisampleStateCreateInfo();
    multisampling.sampleShadingEnable = false;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState();
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = false;

    auto colorBlending = vk::PipelineColorBlendStateCreateInfo();
    colorBlending.logicOpEnable = false;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);
    if (!pipelineLayout) {
      throw std::runtime_error("failed to create pipeline layout");
    }

    auto pipelineInfo = vk::GraphicsPipelineCreateInfo();
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.empty() ? nullptr : shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = nullptr;

    if (auto pipeline_result =
            device.createGraphicsPipeline(nullptr, pipelineInfo);
        pipeline_result.result != vk::Result::eSuccess) {
      throw std::runtime_error(
          std::format("failed to create graphics pipeline: {}\n",
                      vk::to_string(pipeline_result.result)));
    } else {
      pipeline = pipeline_result.value;
    }

    // device.destroyShaderModule(vertShaderModule);
    // device.destroyShaderModule(fragShaderModule);

    if (Args::verbose() > 0)
      std::cerr << std::format("{}:{}: Graphics pipeline created\n", __FILE__,
                               __LINE__);
  }

  void createFramebuffers() {
    framebuffers.resize(imageViews.size());

    for (auto i = 0u; i < imageViews.size(); ++i) {
      auto attachments = std::vector{imageViews[i]};

      auto framebufferInfo = vk::FramebufferCreateInfo();
      framebufferInfo.renderPass = renderPass;
      framebufferInfo.attachmentCount = attachments.size();
      framebufferInfo.pAttachments = attachments.data();
      framebufferInfo.width = extent.width;
      framebufferInfo.height = extent.height;
      framebufferInfo.layers = 1;

      framebuffers[i] = device.createFramebuffer(framebufferInfo);
      if (!framebuffers[i]) {
        throw std::runtime_error("failed to create framebuffer");
      }
    }

    if (Args::verbose() > 0)
      std::cerr << std::format("{}:{}: Framebuffers created for {} images\n",
                               __FILE__, __LINE__, imageViews.size());
  }

  void createCommandPools() {
    // auto const &queueFamilyIndices = this->queueFamilyIndices;

    auto graphicsPoolInfo = vk::CommandPoolCreateInfo();
    graphicsPoolInfo.queueFamilyIndex = *queueFamilyIndices.graphicsFamily;
    graphicsPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    commandPools.graphics = device.createCommandPool(graphicsPoolInfo);
    if (!commandPools.graphics) {
      throw std::runtime_error("failed to create graphics command pool");
    }

    auto transferPoolInfo = vk::CommandPoolCreateInfo();
    transferPoolInfo.queueFamilyIndex = *queueFamilyIndices.transferFamily;
    transferPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    commandPools.transfer = device.createCommandPool(transferPoolInfo);
    if (!commandPools.transfer) {
      throw std::runtime_error("failed to create transfer command pool");
    }

    auto presentPoolInfo = vk::CommandPoolCreateInfo();
    presentPoolInfo.queueFamilyIndex = *queueFamilyIndices.presentFamily;
    presentPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    commandPools.present = device.createCommandPool(presentPoolInfo);
    if (!commandPools.present) {
      throw std::runtime_error("failed to create present command pool");
    }

    auto computePoolInfo = vk::CommandPoolCreateInfo();
    computePoolInfo.queueFamilyIndex = *queueFamilyIndices.computeFamily;
    computePoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    commandPools.compute = device.createCommandPool(computePoolInfo);
    if (!commandPools.compute) {
      throw std::runtime_error("failed to create compute command pool");
    }

    if (Args::verbose() > 0)
      std::cerr << std::format("{}:{}: Command pools created\n", __FILE__,
                               __LINE__);
  }

  void createCommandBuffers() {
    auto const queueFamilyIndices = this->queueFamilyIndices;
    auto const framebuffers = this->framebuffers;

    auto graphicsCommandBufferInfo = vk::CommandBufferAllocateInfo();
    graphicsCommandBufferInfo.commandPool = commandPools.graphics;
    graphicsCommandBufferInfo.level = vk::CommandBufferLevel::ePrimary;
    graphicsCommandBufferInfo.commandBufferCount = framebuffers.size();

    commandBuffers.graphics = device.allocateCommandBuffers(
        graphicsCommandBufferInfo); // one for each framebuffer
    if (commandBuffers.graphics.empty()) {
      throw std::runtime_error("failed to allocate graphics command buffers");
    }

    auto transferCommandBufferInfo = vk::CommandBufferAllocateInfo();
    transferCommandBufferInfo.commandPool = commandPools.transfer;
    transferCommandBufferInfo.level = vk::CommandBufferLevel::ePrimary;
    transferCommandBufferInfo.commandBufferCount = 1;

    commandBuffers.transfer = device.allocateCommandBuffers(
        transferCommandBufferInfo); // one for each framebuffer
    if (commandBuffers.transfer.empty()) {
      throw std::runtime_error("failed to allocate transfer command buffers");
    }

    auto presentCommandBufferInfo = vk::CommandBufferAllocateInfo();
    presentCommandBufferInfo.commandPool = commandPools.present;
    presentCommandBufferInfo.level = vk::CommandBufferLevel::ePrimary;
    presentCommandBufferInfo.commandBufferCount = 1;

    commandBuffers.present = device.allocateCommandBuffers(
        presentCommandBufferInfo); // one for each framebuffer
    if (commandBuffers.present.empty()) {
      throw std::runtime_error("failed to allocate present command buffers");
    }

    auto computeCommandBufferInfo = vk::CommandBufferAllocateInfo();
    computeCommandBufferInfo.commandPool = commandPools.compute;
    computeCommandBufferInfo.level = vk::CommandBufferLevel::ePrimary;
    computeCommandBufferInfo.commandBufferCount = 1;

    commandBuffers.compute = device.allocateCommandBuffers(
        computeCommandBufferInfo); // one for each framebuffer
    if (commandBuffers.compute.empty()) {
      throw std::runtime_error("failed to allocate compute command buffers");
    }

    if (Args::verbose() > 0)
      std::cerr << std::format("{}:{}: Command buffers allocated\n", __FILE__,
                               __LINE__);
  }

  void createSyncObjects() {
    auto imageAvailableSemaphoreInfo = vk::SemaphoreCreateInfo();
    imageAvailableSemaphore =
        device.createSemaphore(imageAvailableSemaphoreInfo);
    if (!imageAvailableSemaphore) {
      throw std::runtime_error("failed to create image available semaphore");
    }

    auto renderFinishedSemaphoreInfo = vk::SemaphoreCreateInfo();
    renderFinishedSemaphore =
        device.createSemaphore(renderFinishedSemaphoreInfo);
    if (!renderFinishedSemaphore) {
      throw std::runtime_error("failed to create render finished semaphore");
    }

    auto inFlightFenceInfo = vk::FenceCreateInfo();
    inFlightFenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
    inFlightFence = device.createFence(inFlightFenceInfo);
    if (!inFlightFence) {
      throw std::runtime_error("failed to create in flight fence");
    }

    if (Args::verbose() > 0)
      std::cerr << std::format("{}:{}: Sync objects created\n", __FILE__,
                               __LINE__);
  }

  /**
   * Create the Vk swapchain (the chain of images that are presented to screen.
   * Supports separate graphics and present queues.
   * Preconditions:
   *  - physical device selected
   *  - surface acquired from platform (to query capabilities, formats and
   *    present modes)
   *  - queue family indices are complete, device created, queues initialized
   */
  void createSwapchain() {

    auto const capabilities =
        physicalDevice.getSurfaceCapabilitiesKHR(*surface);

    // this is the extent of the swapchain images
    auto const extent = [&capabilities,
                         platformVkImpl = this->platformVkImpl]() {
      if (capabilities.currentExtent.width !=
          std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
      }

      auto const [geo_width, geo_height] = platformVkImpl->get_geometry();

      auto const width =
          std::clamp(geo_width, capabilities.minImageExtent.width,
                     capabilities.maxImageExtent.width);
      auto const height =
          std::clamp(geo_height, capabilities.minImageExtent.height,
                     capabilities.maxImageExtent.height);

      return vk::Extent2D(width, height);
    }();

    this->extent = extent;
    if (Args::verbose() > 0) {
      std::cerr << std::format("Swapchain extent: {}x{}\n", extent.width,
                               extent.height);
    }

    auto const formats = physicalDevice.getSurfaceFormatsKHR(*surface);
    auto const presentModes =
        physicalDevice.getSurfacePresentModesKHR(*surface);

    // try to find a suitable format
    auto const format = std::ranges::find_if(formats, [](auto const &format) {
      return format.format == vk::Format::eB8G8R8A8Srgb &&
             format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });

    if (format == formats.end()) {
      throw std::runtime_error("failed to find suitable surface format");
    }

    this->format = format->format;

    if (Args::verbose() > 0) {
      for (auto const &pmode : presentModes) {
        std::cerr << "Present mode " << vk::to_string(pmode) << " supported\n";
      }
    }

    auto const presentMode =
        std::ranges::find_if(presentModes, [](auto const &presentMode) {
          return presentMode == vk::PresentModeKHR::eFifo; // vsync
        });

    if (presentMode == presentModes.end()) {
      throw std::runtime_error("failed to find suitable present mode");
    }

    // we need at least one more image than the minimum
    auto const imageCount = std::clamp(
        capabilities.minImageCount + 1, capabilities.minImageCount,
        capabilities.maxImageCount == 0 ? 3 : capabilities.maxImageCount);

    auto createInfo = vk::SwapchainCreateInfoKHR{};
    createInfo.surface = *surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = format->format;
    createInfo.imageColorSpace = format->colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    if (!queueFamilyIndices.isComplete()) {
      throw std::runtime_error("queue family indices are not complete");
    }

    // we need to specify the queue families that will access the images
    if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily) {
      createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices =
          new uint32_t[2]{*queueFamilyIndices.graphicsFamily,
                          *queueFamilyIndices.presentFamily};
    } else {
      // if the queue families are the same, we can use exclusive sharing mode
      createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = *presentMode;
    createInfo.clipped = true;

    swapchain = device.createSwapchainKHR(createInfo);
    if (!swapchain) {
      throw std::runtime_error("failed to create swap chain");
    }

    // now we have the swapchain, we can get the images
    images = device.getSwapchainImagesKHR(swapchain);
    if (images.empty()) {
      throw std::runtime_error("failed to get swap chain images");
    }

    if (Args::verbose() > 0)
      std::cerr << std::format("Swapchain created with {} images\n",
                               images.size());
  }

  /**
   *  Create the Vk logical device
   *   - Also save queue Family indices for later use in creating the swapchain
   *   - Initializes the graphics/transfer/present/compute queues
   *  Preconditions: physical device selected, surface acquired from platform
   */
  void createDevice() {

    if (device) {
      if (Args::verbose() > 0) {
        std::cerr << std::format(
            "{}:{}:{}: skipping logical device re-creation\n", __FILE__,
            __LINE__, __PRETTY_FUNCTION__);
      }
      return;
    }

    auto const queueFamilyProperties =
        physicalDevice.getQueueFamilyProperties();

    for (auto i = 0u; i < queueFamilyProperties.size(); ++i) {
      if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) {
        queueFamilyIndices.graphicsFamily = i;
      }

      auto const presentSupport =
          physicalDevice.getSurfaceSupportKHR(i, *surface);
      if (presentSupport) {
        queueFamilyIndices.presentFamily = i;
      }

      // check for transfer and compute queues
      if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eTransfer) {
        queueFamilyIndices.transferFamily = i;
      }

      if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eCompute) {
        queueFamilyIndices.computeFamily = i;
      }

      if (queueFamilyIndices.isComplete()) {
        break;
      }
    }

    if (Args::verbose() > 0) {
      std::cerr << "Queue family properties:\n";
      for (auto const &properties : queueFamilyProperties) {
        std::cerr << "  Queue count: " << properties.queueCount << '\n';
        std::cerr << "  Queue flags: " << vk::to_string(properties.queueFlags)
                  << '\n';
        std::cerr << "  Timestamp valid bits: " << properties.timestampValidBits
                  << '\n';
        std::cerr << "  Min image transfer granularity: "
                  << "width=" << properties.minImageTransferGranularity.width
                  << ", height="
                  << properties.minImageTransferGranularity.height
                  << ", depth=" << properties.minImageTransferGranularity.depth
                  << '\n';
      }
    }

    float const queuePriority = 1.0f;
    auto queueCreateInfos = std::vector<vk::DeviceQueueCreateInfo>();

    if (queueFamilyIndices.graphicsFamily.has_value()) {
      auto queueCreateInfo = vk::DeviceQueueCreateInfo();
      queueCreateInfo.queueFamilyIndex = *queueFamilyIndices.graphicsFamily;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
    }

    if (queueFamilyIndices.presentFamily.has_value() &&
        queueFamilyIndices.presentFamily != queueFamilyIndices.graphicsFamily) {
      auto queueCreateInfo = vk::DeviceQueueCreateInfo();
      queueCreateInfo.queueFamilyIndex = *queueFamilyIndices.presentFamily;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
    }

    if (queueFamilyIndices.transferFamily.has_value() &&
        queueFamilyIndices.transferFamily !=
            queueFamilyIndices.graphicsFamily &&
        queueFamilyIndices.transferFamily != queueFamilyIndices.presentFamily) {
      auto queueCreateInfo = vk::DeviceQueueCreateInfo();
      queueCreateInfo.queueFamilyIndex = *queueFamilyIndices.transferFamily;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
    }

    if (queueFamilyIndices.computeFamily.has_value() &&
        queueFamilyIndices.computeFamily != queueFamilyIndices.graphicsFamily &&
        queueFamilyIndices.computeFamily != queueFamilyIndices.presentFamily &&
        queueFamilyIndices.computeFamily != queueFamilyIndices.transferFamily) {
      auto queueCreateInfo = vk::DeviceQueueCreateInfo();
      queueCreateInfo.queueFamilyIndex = *queueFamilyIndices.computeFamily;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
    }

    auto const exts =
        std::vector{"VK_KHR_swapchain"}; // required extension
                                         // createSwapchain requires it

    auto deviceCreateInfo = vk::DeviceCreateInfo{};
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.setEnabledExtensionCount(exts.size());
    deviceCreateInfo.setPpEnabledExtensionNames(exts.data());

    device = physicalDevice.createDevice(deviceCreateInfo);
    if (!device) {
      throw std::runtime_error("failed to create logical device");
    }

    queue = device.getQueue(queueFamilyIndices.graphicsFamily.value(), 0);
    if (!queue) {
      throw std::runtime_error("failed to get queue");
    }

    transferQueue =
        device.getQueue(queueFamilyIndices.transferFamily.value(), 0);
    if (!transferQueue) {
      throw std::runtime_error("failed to get transfer queue");
    }

    presentQueue = device.getQueue(queueFamilyIndices.presentFamily.value(), 0);
    if (!presentQueue) {
      throw std::runtime_error("failed to get present queue");
    }

    computeQueue = device.getQueue(queueFamilyIndices.computeFamily.value(), 0);
    if (!computeQueue) {
      throw std::runtime_error("failed to get compute queue");
    }

    if (Args::verbose() > 0)
      std::cerr << "Logical device created, queues acquired\n";
  }
};