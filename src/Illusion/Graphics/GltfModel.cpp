////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//   _)  |  |            _)                 This software may be modified and distributed         //
//    |  |  |  |  | (_-<  |   _ \    \      under the terms of the MIT license.                   //
//   _| _| _| \_,_| ___/ _| \___/ _| _|     See the LICENSE file for details.                     //
//                                                                                                //
//  Authors: Simon Schneegans (code@simonschneegans.de)                                           //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GltfModel.hpp"

#include "../Core/Logger.hpp"
#include "CommandBuffer.hpp"
#include "Device.hpp"
#include "TextureUtils.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

#include <functional>
#include <unordered_set>

namespace Illusion::Graphics {

////////////////////////////////////////////////////////////////////////////////////////////////////
// Some parts of this code are inspired by Sasha Willem's GLTF loading example:                   //
// https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/base/VulkanglTFModel.hpp          //
////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////

vk::Filter convertFilter(int value) {
  switch (value) {
  case TINYGLTF_TEXTURE_FILTER_NEAREST:
  case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
  case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
    return vk::Filter::eNearest;
  case TINYGLTF_TEXTURE_FILTER_LINEAR:
  case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
  case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
    return vk::Filter::eLinear;
  }

  throw std::runtime_error("Invalid filter mode " + std::to_string(value));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

vk::SamplerMipmapMode convertSamplerMipmapMode(int value) {
  switch (value) {
  case TINYGLTF_TEXTURE_FILTER_NEAREST:
  case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
  case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
    return vk::SamplerMipmapMode::eNearest;
  case TINYGLTF_TEXTURE_FILTER_LINEAR:
  case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
  case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
    return vk::SamplerMipmapMode::eLinear;
  }

  throw std::runtime_error("Invalid sampler mipmap mode " + std::to_string(value));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

vk::SamplerAddressMode convertSamplerAddressMode(int value) {
  switch (value) {
  case TINYGLTF_TEXTURE_WRAP_REPEAT:
    return vk::SamplerAddressMode::eRepeat;
  case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
    return vk::SamplerAddressMode::eClampToEdge;
  case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
    return vk::SamplerAddressMode::eMirroredRepeat;
  }

  throw std::runtime_error("Invalid sampler address mode " + std::to_string(value));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

vk::PrimitiveTopology convertPrimitiveTopology(int value) {
  switch (value) {
  case TINYGLTF_MODE_POINTS:
    return vk::PrimitiveTopology::ePointList;
  case TINYGLTF_MODE_LINE:
    return vk::PrimitiveTopology::eLineStrip;
  case TINYGLTF_MODE_TRIANGLES:
    return vk::PrimitiveTopology::eTriangleList;
  case TINYGLTF_MODE_TRIANGLE_STRIP:
    return vk::PrimitiveTopology::eTriangleStrip;
  case TINYGLTF_MODE_TRIANGLE_FAN:
    return vk::PrimitiveTopology::eTriangleFan;
  }

  throw std::runtime_error("Invalid primitive topology " + std::to_string(value));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////

GltfModel::GltfModel(
  DevicePtr const& device, std::string const& file, TextureChannelMapping const& textureChannels)
  : mDevice(device) {

  // load the file ---------------------------------------------------------------------------------
  tinygltf::Model model;
  {
    std::string        extension{file.substr(file.find_last_of('.'))};
    std::string        error, warn;
    bool               success = false;
    tinygltf::TinyGLTF loader;

    if (extension == ".glb") {
      ILLUSION_TRACE << "Loading binary file " << file << "..." << std::endl;
      success = loader.LoadBinaryFromFile(&model, &error, &warn, file);
    } else if (extension == ".gltf") {
      ILLUSION_TRACE << "Loading ascii file " << file << "..." << std::endl;
      success = loader.LoadASCIIFromFile(&model, &error, &warn, file);
    } else {
      throw std::runtime_error(
        "Error loading GLTF file " + file + ": Unknown extension " + extension);
    }

    if (!error.empty()) {
      throw std::runtime_error("Error loading GLTF file " + file + ": " + error);
    }
    if (!success) {
      throw std::runtime_error("Error loading GLTF file " + file);
    }
  }

  // create textures -------------------------------------------------------------------------------
  {
    for (size_t i{0}; i < model.textures.size(); ++i) {

      tinygltf::Sampler sampler;

      if (model.textures[i].sampler >= 0) {
        sampler = model.samplers[model.textures[i].sampler];
      } else {
        sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
        sampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
        sampler.wrapS     = TINYGLTF_TEXTURE_WRAP_REPEAT;
        sampler.wrapT     = TINYGLTF_TEXTURE_WRAP_REPEAT;
      }

      tinygltf::Image image;

      if (model.textures[i].source >= 0) {
        image = model.images[model.textures[i].source];
      } else {
        throw std::runtime_error("Error loading GLTF file " + file + ": No image source given");
      }

      vk::SamplerCreateInfo samplerInfo;
      samplerInfo.magFilter               = convertFilter(sampler.magFilter);
      samplerInfo.minFilter               = convertFilter(sampler.minFilter);
      samplerInfo.addressModeU            = convertSamplerAddressMode(sampler.wrapS);
      samplerInfo.addressModeV            = convertSamplerAddressMode(sampler.wrapT);
      samplerInfo.addressModeW            = vk::SamplerAddressMode::eRepeat;
      samplerInfo.anisotropyEnable        = true;
      samplerInfo.maxAnisotropy           = 16;
      samplerInfo.borderColor             = vk::BorderColor::eIntOpaqueBlack;
      samplerInfo.unnormalizedCoordinates = false;
      samplerInfo.compareEnable           = false;
      samplerInfo.compareOp               = vk::CompareOp::eAlways;
      samplerInfo.mipmapMode              = convertSamplerMipmapMode(sampler.minFilter);
      samplerInfo.mipLodBias              = 0.f;
      samplerInfo.minLod                  = 0.f;
      samplerInfo.maxLod = TextureUtils::getMaxMipmapLevels(image.width, image.height);

      // TODO: if no image data has been loaded, try loading it on our own
      if (image.image.empty()) {
        throw std::runtime_error(
          "Failed to load GLTF model: Non-tinygltf texture loading is not implemented yet!");
      } else {
        // if there is image data, create an appropriate texture object for it
        vk::ImageCreateInfo imageInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.format =
          image.component == 3 ? vk::Format::eR8G8B8Unorm : vk::Format::eR8G8B8A8Unorm;
        imageInfo.extent.width  = image.width;
        imageInfo.extent.height = image.height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = samplerInfo.maxLod;
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = vk::SampleCountFlagBits::e1;
        imageInfo.tiling        = vk::ImageTiling::eOptimal;
        imageInfo.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc |
                          vk::ImageUsageFlagBits::eTransferDst;
        imageInfo.sharingMode   = vk::SharingMode::eExclusive;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;

        // Check if this texture is used as occlusion or metallicRoughness texture, if so we need to
        // adapt the vk::ComponentMapping accordingly. Occlusion should always map to the the red
        // channel, roughness to green and metallic to blue.
        vk::ComponentMapping componentMapping;
        static const std::unordered_map<TextureChannelMapping::Channel, vk::ComponentSwizzle>
          convert = {{TextureChannelMapping::Channel::eRed, vk::ComponentSwizzle::eR},
            {TextureChannelMapping::Channel::eGreen, vk::ComponentSwizzle::eG},
            {TextureChannelMapping::Channel::eBlue, vk::ComponentSwizzle::eB}};

        for (auto const& material : model.materials) {
          for (auto const& p : material.values) {
            if (p.first == "metallicRoughnessTexture" && p.second.TextureIndex() == i) {
              componentMapping.g = convert.at(textureChannels.mRoughness);
              componentMapping.b = convert.at(textureChannels.mMetallic);
              break;
            }
          }
          for (auto const& p : material.additionalValues) {
            if (p.first == "occlusionTexture" && p.second.TextureIndex() == i) {
              componentMapping.r = convert.at(textureChannels.mOcclusion);
              break;
            }
          }
        }

        // create the texture
        auto texture = mDevice->createTexture(imageInfo, samplerInfo, vk::ImageViewType::e2D,
          vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eShaderReadOnlyOptimal,
          componentMapping, image.image.size(), (void*)image.image.data());

        TextureUtils::updateMipmaps(mDevice, texture);

        mTextures.push_back(texture);
      }
    }
  }

  // create materials ------------------------------------------------------------------------------
  {
    for (auto const& material : model.materials) {

      auto m = std::make_shared<Material>();

      m->mAlbedoTexture            = mDevice->getSinglePixelTexture({255, 255, 255, 255});
      m->mMetallicRoughnessTexture = mDevice->getSinglePixelTexture({255, 255, 255, 255});
      m->mNormalTexture            = mDevice->getSinglePixelTexture({127, 127, 255, 255});
      m->mOcclusionTexture         = mDevice->getSinglePixelTexture({255, 255, 255, 255});
      m->mEmissiveTexture          = mDevice->getSinglePixelTexture({0, 0, 0, 255});

      m->mName = material.name;

      for (auto const& p : material.values) {
        if (p.first == "baseColorTexture") {
          m->mAlbedoTexture = mTextures[p.second.TextureIndex()];
        } else if (p.first == "metallicRoughnessTexture") {
          m->mMetallicRoughnessTexture = mTextures[p.second.TextureIndex()];
        } else if (p.first == "metallicFactor") {
          m->mPushConstants.mMetallicFactor = p.second.Factor();
        } else if (p.first == "roughnessFactor") {
          m->mPushConstants.mRoughnessFactor = p.second.Factor();
        } else if (p.first == "baseColorFactor") {
          auto fac                        = p.second.ColorFactor();
          m->mPushConstants.mAlbedoFactor = glm::vec4(fac[0], fac[1], fac[2], fac[3]);
        } else {
          ILLUSION_WARNING << "Ignoring GLTF property \"" << p.first << "\" of material "
                           << m->mName << "\"!" << std::endl;
        }
      }

      bool ignoreCutoff = false;
      bool hasBlendMode = false;

      for (auto const& p : material.additionalValues) {
        if (p.first == "normalTexture") {
          m->mNormalTexture = mTextures[p.second.TextureIndex()];
        } else if (p.first == "occlusionTexture") {
          m->mOcclusionTexture = mTextures[p.second.TextureIndex()];
        } else if (p.first == "emissiveTexture") {
          m->mEmissiveTexture = mTextures[p.second.TextureIndex()];
        } else if (p.first == "normalScale") {
          m->mPushConstants.mNormalScale = p.second.Factor();
        } else if (p.first == "alphaCutoff") {
          if (!ignoreCutoff) {
            m->mPushConstants.mAlphaCutoff = p.second.Factor();
          }
        } else if (p.first == "occlusionStrength") {
          m->mPushConstants.mOcclusionStrength = p.second.Factor();
        } else if (p.first == "emissiveFactor") {
          auto fac                          = p.second.ColorFactor();
          m->mPushConstants.mEmissiveFactor = glm::vec3(fac[0], fac[1], fac[2]);
        } else if (p.first == "alphaMode") {
          hasBlendMode = true;
          if (p.second.string_value == "BLEND") {
            m->mDoAlphaBlending            = true;
            m->mPushConstants.mAlphaCutoff = 0.f;
            ignoreCutoff                   = true;
          } else if (p.second.string_value == "MASK") {
            m->mDoAlphaBlending = false;
          } else {
            m->mDoAlphaBlending            = false;
            m->mPushConstants.mAlphaCutoff = 1.f;
            ignoreCutoff                   = true;
          }
        } else if (p.first == "doubleSided") {
          m->mDoubleSided = p.second.bool_value;
        } else if (p.first == "name") {
          // tinygltf already loaded the name
        } else {
          ILLUSION_WARNING << "Ignoring GLTF property \"" << p.first << "\" of material \""
                           << m->mName << "\"!" << std::endl;
        }
      }

      if (!hasBlendMode) {
        m->mPushConstants.mAlphaCutoff = 0.f;
      }

      mMaterials.emplace_back(m);
    }
  }

  // create meshes & primitives --------------------------------------------------------------------
  {
    std::vector<uint32_t> indexBuffer;
    std::vector<Vertex>   vertexBuffer;

    for (auto const& m : model.meshes) {

      auto mesh   = std::make_shared<Mesh>();
      mesh->mName = m.name;

      for (auto const& p : m.primitives) {
        Primitive primitve;

        primitve.mMaterial = mMaterials[p.material];
        primitve.mTopology = convertPrimitiveTopology(p.mode);

        uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());

        // append all vertices to our vertex buffer
        const float*    vertexPositions = nullptr;
        const float*    vertexNormals   = nullptr;
        const float*    vertexTexcoords = nullptr;
        const uint16_t* vertexJoints    = nullptr;
        const float*    vertexWeights   = nullptr;
        uint32_t        vertexCount     = 0;

        auto it = p.attributes.find("POSITION");
        if (it != p.attributes.end()) {
          auto const& a   = model.accessors[it->second];
          auto const& v   = model.bufferViews[a.bufferView];
          vertexPositions = reinterpret_cast<const float*>(
            &(model.buffers[v.buffer].data[a.byteOffset + v.byteOffset]));
          vertexCount = a.count;
        } else {
          throw std::runtime_error("Failed to load GLTF model: Primitve has no vertex data!");
        }

        if ((it = p.attributes.find("NORMAL")) != p.attributes.end()) {
          auto const& a = model.accessors[it->second];
          auto const& v = model.bufferViews[a.bufferView];
          vertexNormals = reinterpret_cast<const float*>(
            &(model.buffers[v.buffer].data[a.byteOffset + v.byteOffset]));
        }

        if ((it = p.attributes.find("TEXCOORD_0")) != p.attributes.end()) {
          auto const& a   = model.accessors[it->second];
          auto const& v   = model.bufferViews[a.bufferView];
          vertexTexcoords = reinterpret_cast<const float*>(
            &(model.buffers[v.buffer].data[a.byteOffset + v.byteOffset]));
        }

        if ((it = p.attributes.find("JOINTS_0")) != p.attributes.end()) {
          auto const& a = model.accessors[it->second];
          auto const& v = model.bufferViews[a.bufferView];
          vertexJoints  = reinterpret_cast<const uint16_t*>(
            &(model.buffers[v.buffer].data[a.byteOffset + v.byteOffset]));
        }

        if ((it = p.attributes.find("WEIGHTS_0")) != p.attributes.end()) {
          auto const& a = model.accessors[it->second];
          auto const& v = model.bufferViews[a.bufferView];
          vertexWeights = reinterpret_cast<const float*>(
            &(model.buffers[v.buffer].data[a.byteOffset + v.byteOffset]));
        }

        for (uint32_t v = 0; v < vertexCount; ++v) {
          Vertex vertex;
          vertex.mPosition = glm::make_vec3(&vertexPositions[v * 3]);

          primitve.mBoundingBox.add(vertex.mPosition);

          if (vertexNormals) {
            vertex.mNormal = glm::normalize(glm::make_vec3(&vertexNormals[v * 3]));
          }
          if (vertexTexcoords) {
            vertex.mTexcoords = glm::make_vec2(&vertexTexcoords[v * 2]);
          }
          if (vertexJoints && vertexWeights) {
            vertex.mJoint0  = glm::vec4(glm::make_vec4(&vertexJoints[v * 4]));
            vertex.mWeight0 = glm::make_vec4(&vertexWeights[v * 4]);
          }

          vertexBuffer.emplace_back(vertex);
        }

        // append all indices to our index buffer
        auto const& a = model.accessors[p.indices];
        auto const& v = model.bufferViews[a.bufferView];

        primitve.mIndexOffset = static_cast<uint32_t>(indexBuffer.size());
        primitve.mIndexCount  = static_cast<uint32_t>(a.count);

        switch (a.componentType) {
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
          auto data = reinterpret_cast<const uint32_t*>(
            &model.buffers[v.buffer].data[a.byteOffset + v.byteOffset]);
          for (uint32_t i = 0; i < primitve.mIndexCount; ++i) {
            indexBuffer.push_back(data[i] + vertexStart);
          }
          break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
          auto data = reinterpret_cast<const uint16_t*>(
            &model.buffers[v.buffer].data[a.byteOffset + v.byteOffset]);
          for (uint32_t i = 0; i < primitve.mIndexCount; ++i) {
            indexBuffer.push_back(data[i] + vertexStart);
          }
          break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
          auto data = reinterpret_cast<const uint8_t*>(
            &model.buffers[v.buffer].data[a.byteOffset + v.byteOffset]);
          for (uint32_t i = 0; i < primitve.mIndexCount; ++i) {
            indexBuffer.push_back(data[i] + vertexStart);
          }
          break;
        }
        default:
          throw std::runtime_error("Failed to load GLTF model: Unsupported index type!");
        }

        mesh->mPrimitives.emplace_back(primitve);
        mesh->mBoundingBox.add(primitve.mBoundingBox);
      }
      mMeshes.emplace_back(mesh);
    }

    mVertexBuffer = mDevice->createVertexBuffer(vertexBuffer);
    mIndexBuffer  = mDevice->createIndexBuffer(indexBuffer);
  }

  // create nodes ----------------------------------------------------------------------------------
  std::function<void(uint32_t, Node&)> loadNodes = [this, &loadNodes, &model](
                                                     uint32_t childIndex, Node& parent) {
    auto const& child = model.nodes[childIndex];

    Node node;
    node.mName = child.name;

    if (child.matrix.size() > 0) {
      node.mModelMatrix *= glm::make_mat4(child.matrix.data());
    } else {
      if (child.translation.size() > 0) {
        node.mModelMatrix =
          glm::translate(node.mModelMatrix, glm::make_vec3(child.translation.data()));
      }
      if (child.rotation.size() > 0) {
        glm::dquat quaternion(glm::make_quat(child.rotation.data()));
        node.mModelMatrix =
          glm::rotate(node.mModelMatrix, glm::angle(quaternion), glm::axis(quaternion));
      }
      if (child.scale.size() > 0) {
        node.mModelMatrix = glm::scale(node.mModelMatrix, glm::make_vec3(child.scale.data()));
      }
    }

    if (child.mesh >= 0) {
      node.mMesh = mMeshes[child.mesh];
    }

    for (auto const& c : child.children) {
      loadNodes(c, node);
    }

    parent.mChildren.emplace_back(node);
  };

  for (int i : model.scenes[model.defaultScene].nodes) {
    loadNodes(i, mRootNode);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<GltfModel::Node> const& GltfModel::getNodes() const { return mRootNode.mChildren; }

////////////////////////////////////////////////////////////////////////////////////////////////////

GltfModel::BoundingBox GltfModel::getBoundingBox() const { return mRootNode.getBoundingBox(); }

////////////////////////////////////////////////////////////////////////////////////////////////////

BackedBufferPtr const& GltfModel::getIndexBuffer() const { return mIndexBuffer; }

////////////////////////////////////////////////////////////////////////////////////////////////////

BackedBufferPtr const& GltfModel::getVertexBuffer() const { return mVertexBuffer; }

////////////////////////////////////////////////////////////////////////////////////////////////////

void GltfModel::printInfo() const {

  // clang-format off
  ILLUSION_MESSAGE << "Textures:" << std::endl;
  for (auto const& t : mTextures) {
    ILLUSION_MESSAGE << "  " << t << ": " << t->mBackedImage->mImageInfo.extent.width << "x"
                     << t->mBackedImage->mImageInfo.extent.width << ", "
                     << vk::to_string(t->mBackedImage->mImageInfo.format) << std::endl;
  }

  ILLUSION_MESSAGE << "Materials:" << std::endl;
  for (auto const& m : mMaterials) {
    ILLUSION_MESSAGE << "  " << m << ": " << m->mName << std::endl;
    ILLUSION_MESSAGE << "    AlbedoTexture:            " << m->mAlbedoTexture << std::endl;
    ILLUSION_MESSAGE << "    MetallicRoughnessTexture: " << m->mMetallicRoughnessTexture << std::endl;
    ILLUSION_MESSAGE << "    NormalTexture:            " << m->mNormalTexture << std::endl;
    ILLUSION_MESSAGE << "    OcclusionTexture:         " << m->mOcclusionTexture << std::endl;
    ILLUSION_MESSAGE << "    EmissiveTexture:          " << m->mEmissiveTexture << std::endl;
    ILLUSION_MESSAGE << "    DoAlphaBlending:          " << m->mDoAlphaBlending << std::endl;
    ILLUSION_MESSAGE << "    DoubleSided:              " << m->mDoubleSided << std::endl;
    ILLUSION_MESSAGE << "    AlbedoFactor:             " << m->mPushConstants.mAlbedoFactor << std::endl;
    ILLUSION_MESSAGE << "    EmissiveFactor:           " << m->mPushConstants.mEmissiveFactor << std::endl;
    ILLUSION_MESSAGE << "    MetallicFactor:           " << m->mPushConstants.mMetallicFactor << std::endl;
    ILLUSION_MESSAGE << "    RoughnessFactor:          " << m->mPushConstants.mRoughnessFactor << std::endl;
    ILLUSION_MESSAGE << "    NormalScale:              " << m->mPushConstants.mNormalScale << std::endl;
    ILLUSION_MESSAGE << "    OcclusionStrength:        " << m->mPushConstants.mOcclusionStrength << std::endl;
    ILLUSION_MESSAGE << "    AlphaCutoff:              " << m->mPushConstants.mAlphaCutoff << std::endl;
  }
  ILLUSION_MESSAGE << "Meshes:" << std::endl;
  for (auto const& m : mMeshes) {
    ILLUSION_MESSAGE << "  " << m << ": " << m->mName << std::endl;
    ILLUSION_MESSAGE << "    BoundingBox: " << m->mBoundingBox.mMin << " - " << m->mBoundingBox.mMax << std::endl;
    ILLUSION_MESSAGE << "    Primitives:" << std::endl;
    for (auto const& p : m->mPrimitives) {
      ILLUSION_MESSAGE << "      Material: " << p.mMaterial << " Topology: " << vk::to_string(p.mTopology) 
                       << " IndexCount: " << p.mIndexCount << " IndexOffset: " << p.mIndexOffset 
                       << " BoundingBox: " << p.mBoundingBox.mMin << " - " << p.mBoundingBox.mMax << std::endl;
    }
  }
  ILLUSION_MESSAGE << "Nodes:" << std::endl;
  std::function<void(Node const&, uint32_t)> printNode = [&printNode](Node const& n, uint32_t indent) {
    ILLUSION_MESSAGE << std::string(indent, ' ') << "  " << &n << ": " << n.mName << std::endl;

    if (n.mMesh) {
      ILLUSION_MESSAGE << std::string(indent, ' ') << "    Mesh:        " << n.mMesh << std::endl;
    }

    if (n.mChildren.size() > 0) {
      ILLUSION_MESSAGE << std::string(indent, ' ') << "    Children:" << std::endl;
      for (auto const& c : n.mChildren) {
        printNode(c, indent+2);
      }
    }
  };
  printNode(mRootNode, 0);
  // clang-format on
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<vk::VertexInputBindingDescription> GltfModel::getVertexInputBindings() {
  return {{0, sizeof(Vertex), vk::VertexInputRate::eVertex}};
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<vk::VertexInputAttributeDescription> GltfModel::getVertexInputAttributes() {
  return {{0, 0, vk::Format::eR32G32B32Sfloat, offsetof(struct Vertex, mPosition)},
    {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(struct Vertex, mNormal)},
    {2, 0, vk::Format::eR32G32Sfloat, offsetof(struct Vertex, mTexcoords)},
    {3, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(struct Vertex, mJoint0)},
    {4, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(struct Vertex, mWeight0)}};
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Illusion::Graphics
