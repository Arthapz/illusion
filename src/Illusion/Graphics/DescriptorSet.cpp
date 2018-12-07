////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//   _)  |  |            _)                 This software may be modified and distributed         //
//    |  |  |  |  | (_-<  |   _ \    \      under the terms of the MIT license.                   //
//   _| _| _| \_,_| ___/ _| \___/ _| _|     See the LICENSE file for details.                     //
//                                                                                                //
//  Authors: Simon Schneegans (code@simonschneegans.de)                                           //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DescriptorSet.hpp"

#include "../Core/Logger.hpp"
#include "Device.hpp"
#include "Texture.hpp"

#include <iostream>

namespace Illusion::Graphics {

////////////////////////////////////////////////////////////////////////////////////////////////////

DescriptorSet::DescriptorSet(std::shared_ptr<Device> const& device, vk::DescriptorSet const& base)
  : vk::DescriptorSet(base)
  , mDevice(device) {}

////////////////////////////////////////////////////////////////////////////////////////////////////

void DescriptorSet::bindCombinedImageSampler(
  std::shared_ptr<Texture> const& texture, uint32_t binding) {
  vk::DescriptorImageInfo imageInfo;
  imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  imageInfo.imageView   = *texture->getImageView();
  imageInfo.sampler     = *texture->getSampler();

  vk::WriteDescriptorSet info;
  info.dstSet          = *this;
  info.dstBinding      = binding;
  info.dstArrayElement = 0;
  info.descriptorType  = vk::DescriptorType::eCombinedImageSampler;
  info.descriptorCount = 1;
  info.pImageInfo      = &imageInfo;

  mDevice->getHandle()->updateDescriptorSets(info, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void DescriptorSet::bindStorageImage(std::shared_ptr<Texture> const& texture, uint32_t binding) {
  vk::DescriptorImageInfo imageInfo;
  imageInfo.imageLayout = vk::ImageLayout::eGeneral;
  imageInfo.imageView   = *texture->getImageView();
  imageInfo.sampler     = *texture->getSampler();

  vk::WriteDescriptorSet info;
  info.dstSet          = *this;
  info.dstBinding      = binding;
  info.dstArrayElement = 0;
  info.descriptorType  = vk::DescriptorType::eStorageImage;
  info.descriptorCount = 1;
  info.pImageInfo      = &imageInfo;

  mDevice->getHandle()->updateDescriptorSets(info, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void DescriptorSet::bindUniformBuffer(
  std::shared_ptr<BackedBuffer> const& buffer,
  uint32_t                             binding,
  vk::DeviceSize                       size,
  vk::DeviceSize                       offset) {

  vk::DescriptorBufferInfo bufferInfo;
  bufferInfo.buffer = *buffer->mBuffer;
  bufferInfo.offset = offset;
  bufferInfo.range  = size == 0 ? buffer->mSize : size;

  vk::WriteDescriptorSet info;
  info.dstSet          = *this;
  info.dstBinding      = binding;
  info.dstArrayElement = 0;
  info.descriptorType  = vk::DescriptorType::eUniformBuffer;
  info.descriptorCount = 1;
  info.pBufferInfo     = &bufferInfo;

  mDevice->getHandle()->updateDescriptorSets(info, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Illusion::Graphics
