////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//   _)  |  |            _)                 This software may be modified and distributed         //
//    |  |  |  |  | (_-<  |   _ \    \      under the terms of the MIT license.                   //
//   _| _| _| \_,_| ___/ _| \___/ _| _|     See the LICENSE file for details.                     //
//                                                                                                //
//  Authors: Simon Schneegans (code@simonschneegans.de)                                           //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

// ---------------------------------------------------------------------------------------- includes
#include "Device.hpp"

#include "../Core/Logger.hpp"
#include "CommandBuffer.hpp"
#include "PhysicalDevice.hpp"
#include "PipelineResource.hpp"
#include "Utils.hpp"

#include <iostream>
#include <set>

namespace Illusion::Graphics {

namespace {
const std::vector<const char*> DEVICE_EXTENSIONS{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Device::Device(std::shared_ptr<PhysicalDevice> const& physicalDevice)
  : mPhysicalDevice(physicalDevice)
  , mDevice(createDevice())
  , mGraphicsQueue(mDevice->getQueue(mPhysicalDevice->getGraphicsFamily(), 0))
  , mComputeQueue(mDevice->getQueue(mPhysicalDevice->getComputeFamily(), 0))
  , mPresentQueue(mDevice->getQueue(mPhysicalDevice->getPresentFamily(), 0)) {

  ILLUSION_TRACE << "Creating Device." << std::endl;
  {
    vk::CommandPoolCreateInfo info;
    info.queueFamilyIndex         = mPhysicalDevice->getGraphicsFamily();
    info.flags                    = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    mGraphicsCommandPool          = createCommandPool(info);
    mOneTimeGraphicsCommandBuffer = allocateGraphicsCommandBuffer();
  }
  {
    vk::CommandPoolCreateInfo info;
    info.queueFamilyIndex        = mPhysicalDevice->getComputeFamily();
    info.flags                   = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    mComputeCommandPool          = createCommandPool(info);
    mOneTimeComputeCommandBuffer = allocateComputeCommandBuffer();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Device::~Device() {
  // FIXME: Material::clearPipelineCache();
  ILLUSION_TRACE << "Deleting Device." << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<BackedImage> Device::createBackedImage(
  uint32_t                width,
  uint32_t                height,
  uint32_t                depth,
  uint32_t                levels,
  uint32_t                layers,
  vk::Format              format,
  vk::ImageTiling         tiling,
  vk::ImageUsageFlags     usage,
  vk::MemoryPropertyFlags properties,
  vk::SampleCountFlagBits samples,
  vk::ImageCreateFlags    flags) const {

  auto result = std::make_shared<BackedImage>();

  result->mInfo.imageType     = vk::ImageType::e2D;
  result->mInfo.extent.width  = width;
  result->mInfo.extent.height = height;
  result->mInfo.extent.depth  = depth;
  result->mInfo.mipLevels     = levels;
  result->mInfo.arrayLayers   = layers;
  result->mInfo.format        = format;
  result->mInfo.tiling        = tiling;
  result->mInfo.initialLayout = vk::ImageLayout::eUndefined;
  result->mInfo.usage         = usage;
  result->mInfo.sharingMode   = vk::SharingMode::eExclusive;
  result->mInfo.samples       = samples;
  result->mInfo.flags         = flags;

  result->mImage = createImage(result->mInfo);

  auto requirements = mDevice->getImageMemoryRequirements(*result->mImage);

  vk::MemoryAllocateInfo allocInfo;
  allocInfo.allocationSize = requirements.size;
  allocInfo.memoryTypeIndex =
    mPhysicalDevice->findMemoryType(requirements.memoryTypeBits, properties);

  result->mMemory = createMemory(allocInfo);
  result->mSize   = requirements.size;

  mDevice->bindImageMemory(*result->mImage, *result->mMemory, 0);

  return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<BackedBuffer> Device::createBackedBuffer(
  vk::DeviceSize          size,
  vk::BufferUsageFlags    usage,
  vk::MemoryPropertyFlags properties,
  const void*             data) const {

  auto result   = std::make_shared<BackedBuffer>();
  result->mSize = size;

  {
    vk::BufferCreateInfo info;
    info.size        = size;
    info.usage       = usage;
    info.sharingMode = vk::SharingMode::eExclusive;

    // if data upload will use a staging buffer, we need to make sure transferDst is set!
    if (
      data && (!(properties & vk::MemoryPropertyFlagBits::eHostVisible) ||
               !(properties & vk::MemoryPropertyFlagBits::eHostCoherent))) {
      info.usage |= vk::BufferUsageFlagBits::eTransferDst;
    }

    result->mBuffer = createBuffer(info);
  }

  {
    auto requirements = mDevice->getBufferMemoryRequirements(*result->mBuffer);

    vk::MemoryAllocateInfo info;
    info.allocationSize  = requirements.size;
    info.memoryTypeIndex = mPhysicalDevice->findMemoryType(requirements.memoryTypeBits, properties);

    result->mMemory = createMemory(info);
  }

  mDevice->bindBufferMemory(*result->mBuffer, *result->mMemory, 0);

  if (data) {
    // data was provided, we need to upload it!
    if (
      (properties & vk::MemoryPropertyFlagBits::eHostVisible) &&
      (properties & vk::MemoryPropertyFlagBits::eHostCoherent)) {

      // simple case - memory is host visible and coherent;
      // we can simply map it and upload the data
      void* dst = mDevice->mapMemory(*result->mMemory, 0, size);
      std::memcpy(dst, data, size);
      mDevice->unmapMemory(*result->mMemory);
    } else {

      // more difficult case, we need a staging buffer!
      auto stagingBuffer = createBackedBuffer(
        size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        data);

      auto cmd = beginSingleTimeGraphicsCommands();
      cmd->copyBuffer(*stagingBuffer->mBuffer, *result->mBuffer, size);
      endSingleTimeGraphicsCommands();
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<BackedBuffer> Device::createVertexBuffer(
  vk::DeviceSize size, const void* data) const {
  return createBackedBuffer(
    size, vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, data);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<BackedBuffer> Device::createIndexBuffer(
  vk::DeviceSize size, const void* data) const {
  return createBackedBuffer(
    size, vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, data);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<BackedBuffer> Device::createUniformBuffer(vk::DeviceSize size) const {
  return createBackedBuffer(
    size,
    vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
    vk::MemoryPropertyFlagBits::eDeviceLocal);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<CommandBuffer> Device::allocateGraphicsCommandBuffer() const {
  vk::CommandBufferAllocateInfo info;
  info.level              = vk::CommandBufferLevel::ePrimary;
  info.commandPool        = *mGraphicsCommandPool;
  info.commandBufferCount = 1;

  ILLUSION_TRACE << "Allocating Graphics CommandBuffer." << std::endl;

  auto device{mDevice};
  auto pool{mGraphicsCommandPool};
  auto commandBuffer = std::shared_ptr<CommandBuffer>(
    new CommandBuffer(mDevice->allocateCommandBuffers(info)[0]),
    [device, pool](CommandBuffer* obj) {
      ILLUSION_TRACE << "Freeing Graphics CommandBuffer." << std::endl;
      device->freeCommandBuffers(*pool, *obj);
      delete obj;
    });

  return commandBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<CommandBuffer> Device::allocateComputeCommandBuffer() const {
  vk::CommandBufferAllocateInfo info;
  info.level              = vk::CommandBufferLevel::ePrimary;
  info.commandPool        = *mComputeCommandPool;
  info.commandBufferCount = 1;

  ILLUSION_TRACE << "Allocating Compute CommandBuffer." << std::endl;

  auto device{mDevice};
  auto pool{mComputeCommandPool};
  auto commandBuffer = std::shared_ptr<CommandBuffer>(
    new CommandBuffer(mDevice->allocateCommandBuffers(info)[0]),
    [device, pool](CommandBuffer* obj) {
      ILLUSION_TRACE << "Freeing Compute CommandBuffer." << std::endl;
      device->freeCommandBuffers(*pool, *obj);
      delete obj;
    });

  return commandBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Device::submit(
  std::vector<CommandBuffer> const&          commandBuffers,
  std::vector<vk::Semaphore> const&          waitSemaphores,
  std::vector<vk::PipelineStageFlags> const& waitStages,
  std::vector<vk::Semaphore> const&          signalSemaphores,
  vk::Fence const&                           fence) const {

  vk::SubmitInfo info;
  info.pWaitDstStageMask    = waitStages.data();
  info.commandBufferCount   = commandBuffers.size();
  info.pCommandBuffers      = commandBuffers.data();
  info.signalSemaphoreCount = signalSemaphores.size();
  info.pSignalSemaphores    = signalSemaphores.data();
  info.waitSemaphoreCount   = waitSemaphores.size();
  info.pWaitSemaphores      = waitSemaphores.data();

  mGraphicsQueue.submit(info, fence);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::Buffer> Device::createBuffer(vk::BufferCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::Buffer." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->createBuffer(info), [device](vk::Buffer* obj) {
    ILLUSION_TRACE << "Deleting vk::Buffer." << std::endl;
    device->destroyBuffer(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::CommandPool> Device::createCommandPool(
  vk::CommandPoolCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::CommandPool." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->createCommandPool(info), [device](vk::CommandPool* obj) {
    ILLUSION_TRACE << "Deleting vk::CommandPool." << std::endl;
    device->destroyCommandPool(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::DescriptorPool> Device::createDescriptorPool(
  vk::DescriptorPoolCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::DescriptorPool." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(
    device->createDescriptorPool(info), [device](vk::DescriptorPool* obj) {
      ILLUSION_TRACE << "Deleting vk::DescriptorPool." << std::endl;
      device->destroyDescriptorPool(*obj);
      delete obj;
    });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::DescriptorSetLayout> Device::createDescriptorSetLayout(
  vk::DescriptorSetLayoutCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::DescriptorSetLayout." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(
    device->createDescriptorSetLayout(info), [device](vk::DescriptorSetLayout* obj) {
      ILLUSION_TRACE << "Deleting vk::DescriptorSetLayout." << std::endl;
      device->destroyDescriptorSetLayout(*obj);
      delete obj;
    });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::DeviceMemory> Device::createMemory(vk::MemoryAllocateInfo const& info) const {
  ILLUSION_TRACE << "Allocating vk::DeviceMemory." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->allocateMemory(info), [device](vk::DeviceMemory* obj) {
    ILLUSION_TRACE << "Freeing vk::DeviceMemory." << std::endl;
    device->freeMemory(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::Fence> Device::createFence(vk::FenceCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::Fence." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->createFence(info), [device](vk::Fence* obj) {
    ILLUSION_TRACE << "Deleting vk::Fence." << std::endl;
    device->destroyFence(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::Framebuffer> Device::createFramebuffer(
  vk::FramebufferCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::Framebuffer." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->createFramebuffer(info), [device](vk::Framebuffer* obj) {
    ILLUSION_TRACE << "Deleting vk::Framebuffer." << std::endl;
    device->destroyFramebuffer(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::Image> Device::createImage(vk::ImageCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::Image." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->createImage(info), [device](vk::Image* obj) {
    ILLUSION_TRACE << "Deleting vk::Image." << std::endl;
    device->destroyImage(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::ImageView> Device::createImageView(vk::ImageViewCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::ImageView." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->createImageView(info), [device](vk::ImageView* obj) {
    ILLUSION_TRACE << "Deleting vk::ImageView." << std::endl;
    device->destroyImageView(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::Pipeline> Device::createComputePipeline(
  vk::ComputePipelineCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::ComputePipeline." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(
    device->createComputePipeline(nullptr, info), [device](vk::Pipeline* obj) {
      ILLUSION_TRACE << "Deleting vk::ComputePipeline." << std::endl;
      device->destroyPipeline(*obj);
      delete obj;
    });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::Pipeline> Device::createPipeline(
  vk::GraphicsPipelineCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::Pipeline." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(
    device->createGraphicsPipeline(nullptr, info), [device](vk::Pipeline* obj) {
      ILLUSION_TRACE << "Deleting vk::Pipeline." << std::endl;
      device->destroyPipeline(*obj);
      delete obj;
    });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::PipelineLayout> Device::createPipelineLayout(
  vk::PipelineLayoutCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::PipelineLayout." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(
    device->createPipelineLayout(info), [device](vk::PipelineLayout* obj) {
      ILLUSION_TRACE << "Deleting vk::PipelineLayout." << std::endl;
      device->destroyPipelineLayout(*obj);
      delete obj;
    });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::RenderPass> Device::createRenderPass(
  vk::RenderPassCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::RenderPass." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->createRenderPass(info), [device](vk::RenderPass* obj) {
    ILLUSION_TRACE << "Deleting vk::RenderPass." << std::endl;
    device->destroyRenderPass(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::Sampler> Device::createSampler(vk::SamplerCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::Sampler." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->createSampler(info), [device](vk::Sampler* obj) {
    ILLUSION_TRACE << "Deleting vk::Sampler." << std::endl;
    device->destroySampler(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::Semaphore> Device::createSemaphore(vk::SemaphoreCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::Semaphore." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->createSemaphore(info), [device](vk::Semaphore* obj) {
    ILLUSION_TRACE << "Deleting vk::Semaphore." << std::endl;
    device->destroySemaphore(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::ShaderModule> Device::createShaderModule(
  vk::ShaderModuleCreateInfo const& info) const {
  ILLUSION_TRACE << "Creating vk::ShaderModule." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->createShaderModule(info), [device](vk::ShaderModule* obj) {
    ILLUSION_TRACE << "Deleting vk::ShaderModule." << std::endl;
    device->destroyShaderModule(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::SwapchainKHR> Device::createSwapChainKhr(
  vk::SwapchainCreateInfoKHR const& info) const {
  ILLUSION_TRACE << "Creating vk::SwapchainKHR." << std::endl;
  auto device{mDevice};
  return Utils::makeVulkanPtr(device->createSwapchainKHR(info), [device](vk::SwapchainKHR* obj) {
    ILLUSION_TRACE << "Deleting vk::SwapchainKHR." << std::endl;
    device->destroySwapchainKHR(*obj);
    delete obj;
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<CommandBuffer> Device::beginSingleTimeGraphicsCommands() const {
  mOneTimeGraphicsCommandBuffer->reset({});
  mOneTimeGraphicsCommandBuffer->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  return mOneTimeGraphicsCommandBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Device::endSingleTimeGraphicsCommands() const {
  mOneTimeGraphicsCommandBuffer->end();

  vk::SubmitInfo info;
  info.commandBufferCount = 1;
  info.pCommandBuffers    = mOneTimeGraphicsCommandBuffer.get();

  mGraphicsQueue.submit(info, nullptr);
  mGraphicsQueue.waitIdle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<CommandBuffer> Device::beginSingleTimeComputeCommands() const {
  mOneTimeComputeCommandBuffer->reset({});
  mOneTimeComputeCommandBuffer->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  return mOneTimeComputeCommandBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Device::endSingleTimeComputeCommands() const {
  mOneTimeComputeCommandBuffer->end();

  vk::SubmitInfo info;
  info.commandBufferCount = 1;
  info.pCommandBuffers    = mOneTimeComputeCommandBuffer.get();

  mComputeQueue.submit(info, nullptr);
  mComputeQueue.waitIdle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Device::waitForFences(
  vk::ArrayProxy<const vk::Fence> const& fences, bool waitAll, uint64_t timeout) {
  mDevice->waitForFences(fences, waitAll, timeout);
}

void Device::resetFences(vk::ArrayProxy<const vk::Fence> const& fences) {
  mDevice->resetFences(fences);
}

void Device::waitIdle() { mDevice->waitIdle(); }

////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<vk::Device> Device::createDevice() const {

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

  const float              queuePriority{1.0f};
  const std::set<uint32_t> uniqueQueueFamilies{(uint32_t)mPhysicalDevice->getGraphicsFamily(),
                                               (uint32_t)mPhysicalDevice->getComputeFamily(),
                                               (uint32_t)mPhysicalDevice->getPresentFamily()};

  for (uint32_t queueFamily : uniqueQueueFamilies) {
    vk::DeviceQueueCreateInfo queueCreateInfo;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount       = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  vk::PhysicalDeviceFeatures deviceFeatures;
  deviceFeatures.samplerAnisotropy = true;

  vk::DeviceCreateInfo createInfo;
  createInfo.pQueueCreateInfos       = queueCreateInfos.data();
  createInfo.queueCreateInfoCount    = (uint32_t)queueCreateInfos.size();
  createInfo.pEnabledFeatures        = &deviceFeatures;
  createInfo.enabledExtensionCount   = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
  createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

  ILLUSION_TRACE << "Creating vk::Device." << std::endl;
  return Utils::makeVulkanPtr(mPhysicalDevice->createDevice(createInfo), [](vk::Device* obj) {
    ILLUSION_TRACE << "Deleting vk::Device." << std::endl;
    obj->destroy();
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace Illusion::Graphics