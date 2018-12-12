////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//   _)  |  |            _)                 This software may be modified and distributed         //
//    |  |  |  |  | (_-<  |   _ \    \      under the terms of the MIT license.                   //
//   _| _| _| \_,_| ___/ _| \___/ _| _|     See the LICENSE file for details.                     //
//                                                                                                //
//  Authors: Simon Schneegans (code@simonschneegans.de)                                           //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BindingTypes.hpp"

namespace Illusion::Graphics {

////////////////////////////////////////////////////////////////////////////////////////////////////

bool StorageImageBinding::operator==(StorageImageBinding const& other) const {
  return mImage == other.mImage;
}

bool StorageImageBinding::operator!=(StorageImageBinding const& other) const {
  return !(*this == other);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool CombinedImageSamplerBinding::operator==(CombinedImageSamplerBinding const& other) const {
  return mTexture == other.mTexture;
}

bool CombinedImageSamplerBinding::operator!=(CombinedImageSamplerBinding const& other) const {
  return !(*this == other);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool DynamicUniformBufferBinding::operator==(DynamicUniformBufferBinding const& other) const {
  return mBuffer == other.mBuffer && mSize == other.mSize;
}

bool DynamicUniformBufferBinding::operator!=(DynamicUniformBufferBinding const& other) const {
  return !(*this == other);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool UniformBufferBinding::operator==(UniformBufferBinding const& other) const {
  return mBuffer == other.mBuffer && mSize == other.mSize && mOffset == other.mOffset;
}

bool UniformBufferBinding::operator!=(UniformBufferBinding const& other) const {
  return !(*this == other);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Illusion::Graphics