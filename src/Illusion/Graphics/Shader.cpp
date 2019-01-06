////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//   _)  |  |            _)                 This software may be modified and distributed         //
//    |  |  |  |  | (_-<  |   _ \    \      under the terms of the MIT license.                   //
//   _| _| _| \_,_| ___/ _| \___/ _| _|     See the LICENSE file for details.                     //
//                                                                                                //
//  Authors: Simon Schneegans (code@simonschneegans.de)                                           //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Shader.hpp"

#include "../Core/File.hpp"
#include "../Core/Logger.hpp"
#include "Device.hpp"
#include "PipelineReflection.hpp"
#include "ShaderModule.hpp"
#include "Window.hpp"

#include <spirv_glsl.hpp>

namespace Illusion::Graphics {

namespace {
const std::unordered_map<std::string, vk::ShaderStageFlagBits> extensionMapping = {
    {"frag", vk::ShaderStageFlagBits::eFragment}, {"vert", vk::ShaderStageFlagBits::eVertex},
    {"geom", vk::ShaderStageFlagBits::eGeometry}, {"comp", vk::ShaderStageFlagBits::eCompute},
    {"tesc", vk::ShaderStageFlagBits::eTessellationControl},
    {"tese", vk::ShaderStageFlagBits::eTessellationEvaluation}};
}

////////////////////////////////////////////////////////////////////////////////////////////////////

ShaderPtr Shader::createFromGlslFiles(DevicePtr const& device,
    std::vector<std::string> const& fileNames, bool reloadOnChanges,
    std::set<std::string> dynamicBuffers) {

  auto shader = Shader::create(device);

  for (auto const& fileName : fileNames) {
    auto extension = fileName.substr(fileName.size() - 4);

    auto stage = extensionMapping.find(extension);
    if (stage == extensionMapping.end()) {
      throw std::runtime_error(
          "Failed to add shader stage: File " + fileName + " has an unknown extension!");
    }

    shader->addModule(
        stage->second, ShaderModule::GlslFile{fileName, reloadOnChanges}, dynamicBuffers);
  }

  return shader;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Shader::Shader(DevicePtr const& device)
    : mDevice(device) {
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Shader::~Shader() {
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Shader::addModule(vk::ShaderStageFlagBits stage, ShaderModule::Source const& source,
    std::set<std::string> const& dynamicBuffers) {
  mDirty                 = true;
  mSources[stage]        = source;
  mDynamicBuffers[stage] = dynamicBuffers;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<ShaderModulePtr> const& Shader::getModules() {
  reload();
  return mModules;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

PipelineReflectionPtr const& Shader::getReflection() {
  reload();
  return mReflection;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<DescriptorSetReflectionPtr> const& Shader::getDescriptorSetReflections() {
  reload();
  return mReflection->getDescriptorSetReflections();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Shader::reload() {
  for (auto const& m : mModules) {
    if (m->requiresReload()) {
      try {
        m->reload();
      } catch (std::runtime_error const& e) {
        ILLUSION_ERROR << "Failed to compile shader: " << e.what() << std::endl;
      }
    }
  }

  if (mDirty) {
    std::vector<ShaderModulePtr> modules;
    auto                         reflection = std::make_shared<PipelineReflection>(mDevice);

    try {
      for (auto const& s : mSources) {
        modules.emplace_back(
            ShaderModule::create(mDevice, s.second, s.first, mDynamicBuffers[s.first]));
      }

      for (auto const& module : modules) {
        for (auto const& resource : module->getResources()) {
          reflection->addResource(resource);
        }
      }
    } catch (std::runtime_error const& e) {
      ILLUSION_ERROR << "Failed to compile shader: " << e.what() << std::endl;
      mDirty = false;
      return;
    }

    mReflection = reflection;
    mModules    = modules;
    mDirty      = false;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Illusion::Graphics
