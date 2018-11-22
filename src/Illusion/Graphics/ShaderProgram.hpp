////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//   _)  |  |            _)                 This software may be modified and distributed         //
//    |  |  |  |  | (_-<  |   _ \    \      under the terms of the MIT license.                   //
//   _| _| _| \_,_| ___/ _| \___/ _| _|     See the LICENSE file for details.                     //
//                                                                                                //
//  Authors: Simon Schneegans (code@simonschneegans.de)                                           //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ILLUSION_GRAPHICS_SHADER_PROGRAM_HPP
#define ILLUSION_GRAPHICS_SHADER_PROGRAM_HPP

// ---------------------------------------------------------------------------------------- includes
#include "DescriptorPool.hpp"

#include <map>

namespace Illusion::Graphics {

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// -------------------------------------------------------------------------------------------------
class ShaderProgram {

 public:
  // -------------------------------------------------------------- helper methods for easy creation
  static std::shared_ptr<ShaderProgram> createFromGlslFiles(
    std::shared_ptr<Context> const&                                 context,
    std::shared_ptr<DescriptorSetCache> const&                      descriptorSetCache,
    std::unordered_map<vk::ShaderStageFlagBits, std::string> const& files);

  // -------------------------------------------------------------------------------- public methods
  ShaderProgram(
    std::shared_ptr<Context> const&                   context,
    std::shared_ptr<DescriptorSetCache> const&        descriptorSetCache,
    std::vector<std::shared_ptr<ShaderModule>> const& modules);
  virtual ~ShaderProgram();

  std::vector<std::shared_ptr<ShaderModule>> const&                   getModules() const;
  std::shared_ptr<ShaderReflection> const&                            getReflection() const;
  std::map<uint32_t, std::shared_ptr<vk::DescriptorSetLayout>> const& getDescriptorSetLayouts()
    const;
  std::shared_ptr<vk::PipelineLayout> const& getPipelineLayout() const;

 private:
  // ------------------------------------------------------------------------------- private methods
  void createReflection();
  void createDescriptorSetLayouts(std::shared_ptr<DescriptorSetCache> const& descriptorSetCache);
  void createPipelineLayout();

  // ------------------------------------------------------------------------------- private members
  std::shared_ptr<Context>                                     mContext;
  std::vector<std::shared_ptr<ShaderModule>>                   mModules;
  std::shared_ptr<ShaderReflection>                            mReflection;
  std::map<uint32_t, std::shared_ptr<vk::DescriptorSetLayout>> mDescriptorSetLayouts;
  std::shared_ptr<vk::PipelineLayout>                          mPipelineLayout;
};
} // namespace Illusion::Graphics

#endif // ILLUSION_GRAPHICS_SHADER_PROGRAM_HPP
