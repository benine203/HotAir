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
  vk::SurfaceKHR *surface{}; // this is a pointer because it's created by the
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
      for (auto &image_view : imageViews) {
        device.destroyImageView(image_view);
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

    if (surface == nullptr) {
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

    vk::ApplicationInfo app_info("HotAir", VK_MAKE_VERSION(1, 0, 0), "Baloon",
                                 VK_MAKE_VERSION(1, 0, 0));

    auto extensions = vk::enumerateInstanceExtensionProperties();

    if (Args::verbose() > 1) {
      std::cerr << "available extensions:\n";
      for (auto const &extension : extensions) {
        std::cerr << extension.extensionName << '\n';
      }
    }

    auto names = std::vector<const char *>(extensions.size());
    for (auto i = 0U; i < extensions.size(); ++i) {
      names[i] = extensions[i].extensionName;
    }

    auto create_info = vk::InstanceCreateInfo();
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = names.size();
    create_info.ppEnabledExtensionNames = names.data();

    instance = vk::createInstance(create_info);
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

    for (auto i = 0U; i < images.size(); ++i) {
      auto create_info = vk::ImageViewCreateInfo();
      create_info.image = images[i];
      create_info.viewType = vk::ImageViewType::e2D;
      create_info.format = format;
      create_info.components.r = vk::ComponentSwizzle::eIdentity;
      create_info.components.g = vk::ComponentSwizzle::eIdentity;
      create_info.components.b = vk::ComponentSwizzle::eIdentity;
      create_info.components.a = vk::ComponentSwizzle::eIdentity;
      create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
      create_info.subresourceRange.baseMipLevel = 0;
      create_info.subresourceRange.levelCount = 1;
      create_info.subresourceRange.baseArrayLayer = 0;
      create_info.subresourceRange.layerCount = 1;

      imageViews[i] = device.createImageView(create_info);
      if (!imageViews[i]) {
        throw std::runtime_error("failed to create image views");
      }
    }

    if (Args::verbose() > 0)
      std::cerr << std::format("{}:{}: Image views created for {} images\n",
                               __FILE__, __LINE__, images.size());
  }

  void createRenderPass() {
    auto color_attachment = vk::AttachmentDescription();
    color_attachment.format = format;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    auto color_attachment_ref = vk::AttachmentReference();
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    auto subpass = vk::SubpassDescription();
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    auto dependency = vk::SubpassDependency();
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead |
                               vk::AccessFlagBits::eColorAttachmentWrite;

    auto render_pass_info = vk::RenderPassCreateInfo();
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    renderPass = device.createRenderPass(render_pass_info);
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
    auto shader_stages = std::vector<vk::PipelineShaderStageCreateInfo>();

    auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo();
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;

    auto input_assembly = vk::PipelineInputAssemblyStateCreateInfo();
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    input_assembly.primitiveRestartEnable = vk::Bool32{false};

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

    auto viewport_state = vk::PipelineViewportStateCreateInfo();
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    auto rasterizer = vk::PipelineRasterizationStateCreateInfo();
    rasterizer.depthClampEnable = vk::Bool32{false};
    rasterizer.rasterizerDiscardEnable = vk::Bool32{false};
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = vk::Bool32{false};

    auto multisampling = vk::PipelineMultisampleStateCreateInfo();
    multisampling.sampleShadingEnable = vk::Bool32{false};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    auto color_blend_attachment = vk::PipelineColorBlendAttachmentState();
    color_blend_attachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    color_blend_attachment.blendEnable = vk::Bool32{false};

    auto color_blending = vk::PipelineColorBlendStateCreateInfo();
    color_blending.logicOpEnable = vk::Bool32{false};
    color_blending.logicOp = vk::LogicOp::eCopy;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    auto pipeline_layout_info = vk::PipelineLayoutCreateInfo();
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pSetLayouts = nullptr;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    pipelineLayout = device.createPipelineLayout(pipeline_layout_info);
    if (!pipelineLayout) {
      throw std::runtime_error("failed to create pipeline layout");
    }

    auto pipeline_info = vk::GraphicsPipelineCreateInfo();
    pipeline_info.stageCount = shader_stages.size();
    pipeline_info.pStages =
        shader_stages.empty() ? nullptr : shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = pipelineLayout;
    pipeline_info.renderPass = renderPass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = nullptr;

    if (auto pipeline_result =
            device.createGraphicsPipeline(nullptr, pipeline_info);
        pipeline_result.result == vk::Result::eSuccess) {
      pipeline = pipeline_result.value;
    } else {
      throw std::runtime_error(
          std::format("failed to create graphics pipeline: {}\n",
                      vk::to_string(pipeline_result.result)));
    }

    // device.destroyShaderModule(vertShaderModule);
    // device.destroyShaderModule(fragShaderModule);

    if (Args::verbose() > 0)
      std::cerr << std::format("{}:{}: Graphics pipeline created\n", __FILE__,
                               __LINE__);
  }

  void createFramebuffers() {
    framebuffers.resize(imageViews.size());

    for (auto i = 0U; i < imageViews.size(); ++i) {
      auto attachments = std::vector{imageViews[i]};

      auto framebuffer_info = vk::FramebufferCreateInfo();
      framebuffer_info.renderPass = renderPass;
      framebuffer_info.attachmentCount = attachments.size();
      framebuffer_info.pAttachments = attachments.data();
      framebuffer_info.width = extent.width;
      framebuffer_info.height = extent.height;
      framebuffer_info.layers = 1;

      framebuffers[i] = device.createFramebuffer(framebuffer_info);
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

    auto graphics_pool_info = vk::CommandPoolCreateInfo();
    graphics_pool_info.queueFamilyIndex = *queueFamilyIndices.graphicsFamily;
    graphics_pool_info.flags =
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    commandPools.graphics = device.createCommandPool(graphics_pool_info);
    if (!commandPools.graphics) {
      throw std::runtime_error("failed to create graphics command pool");
    }

    auto transfer_pool_info = vk::CommandPoolCreateInfo();
    transfer_pool_info.queueFamilyIndex = *queueFamilyIndices.transferFamily;
    transfer_pool_info.flags =
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    commandPools.transfer = device.createCommandPool(transfer_pool_info);
    if (!commandPools.transfer) {
      throw std::runtime_error("failed to create transfer command pool");
    }

    auto present_pool_info = vk::CommandPoolCreateInfo();
    present_pool_info.queueFamilyIndex = *queueFamilyIndices.presentFamily;
    present_pool_info.flags =
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    commandPools.present = device.createCommandPool(present_pool_info);
    if (!commandPools.present) {
      throw std::runtime_error("failed to create present command pool");
    }

    auto compute_pool_info = vk::CommandPoolCreateInfo();
    compute_pool_info.queueFamilyIndex = *queueFamilyIndices.computeFamily;
    compute_pool_info.flags =
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    commandPools.compute = device.createCommandPool(compute_pool_info);
    if (!commandPools.compute) {
      throw std::runtime_error("failed to create compute command pool");
    }

    if (Args::verbose() > 0)
      std::cerr << std::format("{}:{}: Command pools created\n", __FILE__,
                               __LINE__);
  }

  void createCommandBuffers() {
    auto const queue_family_indices = this->queueFamilyIndices;
    auto const framebuffers = this->framebuffers;

    auto graphics_command_buffer_info = vk::CommandBufferAllocateInfo();
    graphics_command_buffer_info.commandPool = commandPools.graphics;
    graphics_command_buffer_info.level = vk::CommandBufferLevel::ePrimary;
    graphics_command_buffer_info.commandBufferCount = framebuffers.size();

    commandBuffers.graphics = device.allocateCommandBuffers(
        graphics_command_buffer_info); // one for each framebuffer
    if (commandBuffers.graphics.empty()) {
      throw std::runtime_error("failed to allocate graphics command buffers");
    }

    auto transfer_command_buffer_info = vk::CommandBufferAllocateInfo();
    transfer_command_buffer_info.commandPool = commandPools.transfer;
    transfer_command_buffer_info.level = vk::CommandBufferLevel::ePrimary;
    transfer_command_buffer_info.commandBufferCount = 1;

    commandBuffers.transfer = device.allocateCommandBuffers(
        transfer_command_buffer_info); // one for each framebuffer
    if (commandBuffers.transfer.empty()) {
      throw std::runtime_error("failed to allocate transfer command buffers");
    }

    auto present_command_buffer_info = vk::CommandBufferAllocateInfo();
    present_command_buffer_info.commandPool = commandPools.present;
    present_command_buffer_info.level = vk::CommandBufferLevel::ePrimary;
    present_command_buffer_info.commandBufferCount = 1;

    commandBuffers.present = device.allocateCommandBuffers(
        present_command_buffer_info); // one for each framebuffer
    if (commandBuffers.present.empty()) {
      throw std::runtime_error("failed to allocate present command buffers");
    }

    auto compute_command_buffer_info = vk::CommandBufferAllocateInfo();
    compute_command_buffer_info.commandPool = commandPools.compute;
    compute_command_buffer_info.level = vk::CommandBufferLevel::ePrimary;
    compute_command_buffer_info.commandBufferCount = 1;

    commandBuffers.compute = device.allocateCommandBuffers(
        compute_command_buffer_info); // one for each framebuffer
    if (commandBuffers.compute.empty()) {
      throw std::runtime_error("failed to allocate compute command buffers");
    }

    if (Args::verbose() > 0)
      std::cerr << std::format("{}:{}: Command buffers allocated\n", __FILE__,
                               __LINE__);
  }

  void createSyncObjects() {
    auto image_available_semaphore_info = vk::SemaphoreCreateInfo();
    imageAvailableSemaphore =
        device.createSemaphore(image_available_semaphore_info);
    if (!imageAvailableSemaphore) {
      throw std::runtime_error("failed to create image available semaphore");
    }

    auto render_finished_semaphore_info = vk::SemaphoreCreateInfo();
    renderFinishedSemaphore =
        device.createSemaphore(render_finished_semaphore_info);
    if (!renderFinishedSemaphore) {
      throw std::runtime_error("failed to create render finished semaphore");
    }

    auto in_flight_fence_info = vk::FenceCreateInfo();
    in_flight_fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
    inFlightFence = device.createFence(in_flight_fence_info);
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
                         platform_vk_impl = this->platformVkImpl]() {
      if (capabilities.currentExtent.width !=
          std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
      }

      auto const [geo_width, geo_height] = platform_vk_impl->getGeometry();

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
    auto const present_modes =
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
      for (auto const &pmode : present_modes) {
        std::cerr << "Present mode " << vk::to_string(pmode) << " supported\n";
      }
    }

    auto const present_mode =
        std::ranges::find_if(present_modes, [](auto const &presentMode) {
          return presentMode == vk::PresentModeKHR::eFifo; // vsync
        });

    if (present_mode == present_modes.end()) {
      throw std::runtime_error("failed to find suitable present mode");
    }

    // we need at least one more image than the minimum
    auto const image_count = std::clamp(
        capabilities.minImageCount + 1, capabilities.minImageCount,
        capabilities.maxImageCount == 0 ? 3 : capabilities.maxImageCount);

    auto create_info = vk::SwapchainCreateInfoKHR{};
    create_info.surface = *surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = format->format;
    create_info.imageColorSpace = format->colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    if (!queueFamilyIndices.isComplete()) {
      throw std::runtime_error("queue family indices are not complete");
    }

    // we need to specify the queue families that will access the images
    if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily) {
      create_info.imageSharingMode = vk::SharingMode::eConcurrent;
      create_info.queueFamilyIndexCount = 2;
      create_info.pQueueFamilyIndices =
          new uint32_t[2]{*queueFamilyIndices.graphicsFamily,
                          *queueFamilyIndices.presentFamily};
    } else {
      // if the queue families are the same, we can use exclusive sharing mode
      create_info.imageSharingMode = vk::SharingMode::eExclusive;
    }

    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    create_info.presentMode = *present_mode;
    create_info.clipped = true;

    swapchain = device.createSwapchainKHR(create_info);
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

    auto const queue_family_properties =
        physicalDevice.getQueueFamilyProperties();

    for (auto i = 0U; i < queue_family_properties.size(); ++i) {
      if (queue_family_properties[i].queueFlags &
          vk::QueueFlagBits::eGraphics) {
        queueFamilyIndices.graphicsFamily = i;
      }

      auto const present_support =
          physicalDevice.getSurfaceSupportKHR(i, *surface);
      if (present_support != vk::Bool32{false}) {
        queueFamilyIndices.presentFamily = i;
      }

      // check for transfer and compute queues
      if (queue_family_properties[i].queueFlags &
          vk::QueueFlagBits::eTransfer) {
        queueFamilyIndices.transferFamily = i;
      }

      if (queue_family_properties[i].queueFlags & vk::QueueFlagBits::eCompute) {
        queueFamilyIndices.computeFamily = i;
      }

      if (queueFamilyIndices.isComplete()) {
        break;
      }
    }

    if (Args::verbose() > 0) {
      std::cerr << "Queue family properties:\n";
      for (auto const &properties : queue_family_properties) {
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

    float const queue_priority = 1.0f;
    auto queue_create_infos = std::vector<vk::DeviceQueueCreateInfo>();

    if (queueFamilyIndices.graphicsFamily.has_value()) {
      auto queue_create_info = vk::DeviceQueueCreateInfo();
      queue_create_info.queueFamilyIndex = *queueFamilyIndices.graphicsFamily;
      queue_create_info.queueCount = 1;
      queue_create_info.pQueuePriorities = &queue_priority;
      queue_create_infos.push_back(queue_create_info);
    }

    if (queueFamilyIndices.presentFamily.has_value() &&
        queueFamilyIndices.presentFamily != queueFamilyIndices.graphicsFamily) {
      auto queue_create_info = vk::DeviceQueueCreateInfo();
      queue_create_info.queueFamilyIndex = *queueFamilyIndices.presentFamily;
      queue_create_info.queueCount = 1;
      queue_create_info.pQueuePriorities = &queue_priority;
      queue_create_infos.push_back(queue_create_info);
    }

    if (queueFamilyIndices.transferFamily.has_value() &&
        queueFamilyIndices.transferFamily !=
            queueFamilyIndices.graphicsFamily &&
        queueFamilyIndices.transferFamily != queueFamilyIndices.presentFamily) {
      auto queue_create_info = vk::DeviceQueueCreateInfo();
      queue_create_info.queueFamilyIndex = *queueFamilyIndices.transferFamily;
      queue_create_info.queueCount = 1;
      queue_create_info.pQueuePriorities = &queue_priority;
      queue_create_infos.push_back(queue_create_info);
    }

    if (queueFamilyIndices.computeFamily.has_value() &&
        queueFamilyIndices.computeFamily != queueFamilyIndices.graphicsFamily &&
        queueFamilyIndices.computeFamily != queueFamilyIndices.presentFamily &&
        queueFamilyIndices.computeFamily != queueFamilyIndices.transferFamily) {
      auto queue_create_info = vk::DeviceQueueCreateInfo();
      queue_create_info.queueFamilyIndex = *queueFamilyIndices.computeFamily;
      queue_create_info.queueCount = 1;
      queue_create_info.pQueuePriorities = &queue_priority;
      queue_create_infos.push_back(queue_create_info);
    }

    auto const exts =
        std::vector{"VK_KHR_swapchain"}; // required extension
                                         // createSwapchain requires it

    auto device_create_info = vk::DeviceCreateInfo{};
    device_create_info.queueCreateInfoCount = queue_create_infos.size();
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.setEnabledExtensionCount(exts.size());
    device_create_info.setPpEnabledExtensionNames(exts.data());

    device = physicalDevice.createDevice(device_create_info);
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