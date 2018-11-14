////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//   _)  |  |            _)                 This software may be modified and distributed         //
//    |  |  |  |  | (_-<  |   _ \    \      under the terms of the MIT license.                   //
//   _| _| _| \_,_| ___/ _| \___/ _| _|     See the LICENSE file for details.                     //
//                                                                                                //
//  Authors: Simon Schneegans (code@simonschneegans.de)                                           //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ILLUSION_GRAPHICS_SHADER_REFLECTION_HPP
#define ILLUSION_GRAPHICS_SHADER_REFLECTION_HPP

// ---------------------------------------------------------------------------------------- includes
#include "PipelineResource.hpp"

#include <map>
#include <set>
#include <unordered_map>

namespace Illusion::Graphics {

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class ShaderReflection {
 public:
  ShaderReflection();
  virtual ~ShaderReflection();

  void addResource(PipelineResource const& resource);
  void addResources(std::vector<PipelineResource> const& resources);

  std::map<std::string, PipelineResource> const& getResources() const;

  std::vector<PipelineResource> getResources(PipelineResource::ResourceType type) const;
  std::vector<PipelineResource> getResources(uint32_t set) const;
  std::set<uint32_t> const&     getActiveSets() const;

  void printInfo() const;

 private:
  std::map<std::string, PipelineResource> mResources;
  std::set<uint32_t>                      mActiveSets;
};

} // namespace Illusion::Graphics

#endif // ILLUSION_GRAPHICS_SHADER_REFLECTION_HPP