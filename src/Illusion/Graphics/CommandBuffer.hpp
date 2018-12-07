////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//   _)  |  |            _)                 This software may be modified and distributed         //
//    |  |  |  |  | (_-<  |   _ \    \      under the terms of the MIT license.                   //
//   _| _| _| \_,_| ___/ _| \___/ _| _|     See the LICENSE file for details.                     //
//                                                                                                //
//  Authors: Simon Schneegans (code@simonschneegans.de)                                           //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ILLUSION_GRAPHICS_COMMAND_BUFFER_HPP
#define ILLUSION_GRAPHICS_COMMAND_BUFFER_HPP

#include "BindingState.hpp"
#include "DescriptorSetCache.hpp"
#include "GraphicsState.hpp"
#include "fwd.hpp"

#include <glm/glm.hpp>

namespace Illusion::Graphics {

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CommandBuffer {
 public:
  CommandBuffer(
    std::shared_ptr<Device> const& device,
    QueueType                      type  = QueueType::eGeneric,
    vk::CommandBufferLevel         level = vk::CommandBufferLevel::ePrimary);

  void reset() const;
  void begin(
    vk::CommandBufferUsageFlagBits usage = vk::CommandBufferUsageFlagBits::eSimultaneousUse) const;
  void end() const;
  void submit(
    std::vector<vk::Semaphore> const&          waitSemaphores   = {},
    std::vector<vk::PipelineStageFlags> const& waitStages       = {},
    std::vector<vk::Semaphore> const&          signalSemaphores = {},
    vk::Fence const&                           fence            = nullptr) const;

  void waitIdle() const;

  void beginRenderPass(std::shared_ptr<RenderPass> const& renderPass);
  void endRenderPass();

  void bindIndexBuffer(vk::Buffer buffer, vk::DeviceSize offset, vk::IndexType indexType) const;
  void bindVertexBuffers(
    uint32_t                             firstBinding,
    vk::ArrayProxy<const vk::Buffer>     buffers,
    vk::ArrayProxy<const vk::DeviceSize> offsets) const;

  void bindCombinedImageSampler(
    std::shared_ptr<Texture> const& texture, uint32_t set, uint32_t binding) const;

  void draw(
    uint32_t vertexCount,
    uint32_t instanceCount = 1,
    uint32_t firstVertex   = 0,
    uint32_t firstInstance = 0);

  void drawIndexed(
    uint32_t indexCount,
    uint32_t instanceCount = 1,
    uint32_t firstIndex    = 0,
    int32_t  vertexOffset  = 0,
    uint32_t firstInstance = 0);

  GraphicsState& graphicsState();
  BindingState&  bindingState();

  template <typename T>
  void pushConstants(
    vk::PipelineLayout const& layout,
    vk::ShaderStageFlags      stages,
    T const&                  data,
    uint32_t                  offset = 0) const {

    mVkCmd->pushConstants(layout, stages, offset, sizeof(T), &data);
  }

  void transitionImageLayout(
    vk::Image                 image,
    vk::ImageLayout           oldLayout,
    vk::ImageLayout           newLayout,
    vk::PipelineStageFlagBits stage = vk::PipelineStageFlagBits::eTopOfPipe,
    vk::ImageSubresourceRange range = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}) const;

  void copyImage(vk::Image src, vk::Image dst, glm::uvec2 const& size) const;

  void blitImage(
    vk::Image         src,
    vk::Image         dst,
    glm::uvec2 const& srcSize,
    glm::uvec2 const& dstSize,
    vk::Filter        filter) const;

  void resolveImage(
    vk::Image        src,
    vk::ImageLayout  srcLayout,
    vk::Image        dst,
    vk::ImageLayout  dstLayout,
    vk::ImageResolve region) const;

  void copyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) const;
  void copyBufferToImage(
    vk::Buffer                              src,
    vk::Image                               dst,
    vk::ImageLayout                         dstLayout,
    std::vector<vk::BufferImageCopy> const& infos) const;

 private:
  void flush();

  std::shared_ptr<Device>            mDevice;
  std::shared_ptr<vk::CommandBuffer> mVkCmd;
  QueueType                          mType;
  vk::CommandBufferLevel             mLevel;

  std::shared_ptr<RenderPass> mCurrentRenderPass;

  DescriptorSetCache mDescriptorSetCache;
  GraphicsState      mGraphicsState;
  BindingState       mBindingState;
};

} // namespace Illusion::Graphics

#endif // ILLUSION_GRAPHICS_COMMAND_BUFFER_HPP
