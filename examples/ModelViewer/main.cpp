////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//   _)  |  |            _)                 This software may be modified and distributed         //
//    |  |  |  |  | (_-<  |   _ \    \      under the terms of the MIT license.                   //
//   _| _| _| \_,_| ___/ _| \___/ _| _|     See the LICENSE file for details.                     //
//                                                                                                //
//  Authors: Simon Schneegans (code@simonschneegans.de)                                           //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Illusion/Core/FPSCounter.hpp>
#include <Illusion/Core/Logger.hpp>
#include <Illusion/Graphics/CommandBuffer.hpp>
#include <Illusion/Graphics/Context.hpp>
#include <Illusion/Graphics/DescriptorSet.hpp>
#include <Illusion/Graphics/DisplayPass.hpp>
#include <Illusion/Graphics/Engine.hpp>
#include <Illusion/Graphics/GraphicsState.hpp>
#include <Illusion/Graphics/PhysicalDevice.hpp>
#include <Illusion/Graphics/ShaderProgram.hpp>
#include <Illusion/Graphics/ShaderReflection.hpp>
#include <Illusion/Graphics/Texture.hpp>
#include <Illusion/Graphics/Window.hpp>

#include <iostream>
#include <sstream>
#include <thread>

std::string appName = "SimpleWindow";

int main(int argc, char* argv[]) {

#ifdef NDEBUG
  Illusion::Core::Logger::enableDebug = false;
  Illusion::Core::Logger::enableTrace = false;
  auto engine = std::make_shared<Illusion::Graphics::Engine>(appName, false);
#else
  Illusion::Core::Logger::enableDebug = true;
  Illusion::Core::Logger::enableTrace = true;
  auto engine                         = std::make_shared<Illusion::Graphics::Engine>(appName, true);
#endif

  auto physicalDevice = engine->getPhysicalDevice();
  physicalDevice->printInfo();

  auto context = std::make_shared<Illusion::Graphics::Context>(physicalDevice);
  auto window  = std::make_shared<Illusion::Graphics::Window>(engine, context);
  window->open();

  auto shader = Illusion::Graphics::ShaderProgram::createFromGlslFiles(
    context,
    {{vk::ShaderStageFlagBits::eVertex, "data/shaders/TexturedQuad.vert"},
     {vk::ShaderStageFlagBits::eFragment, "data/shaders/TexturedQuad.frag"}});
  shader->getReflection()->printInfo();

  auto renderPass = window->getDisplayPass();
  renderPass->init();

  auto texture = Illusion::Graphics::Texture::createFromFile(context, "data/textures/box.dds");

  auto set = shader->allocateDescriptorSet();
  set->bindCombinedImageSampler(texture, 0);

  Illusion::Graphics::GraphicsState state;
  state.setShaderProgram(shader);

  Illusion::Graphics::GraphicsState::DepthStencilState depthStencilState;
  depthStencilState.mDepthTestEnable  = false;
  depthStencilState.mDepthWriteEnable = false;
  state.setDepthStencilState(depthStencilState);

  Illusion::Graphics::GraphicsState::ColorBlendState colorBlendState;
  colorBlendState.mAttachments.resize(1);
  state.setColorBlendState(colorBlendState);

  Illusion::Graphics::GraphicsState::ViewportState viewportState;
  viewportState.mViewports.push_back({glm::vec2(0), glm::vec2(window->pSize.get()), 0.f, 1.f});
  viewportState.mScissors.push_back({glm::ivec2(0), window->pSize.get()});
  state.setViewportState(viewportState);

  window->pSize.onChange().connect([&state](glm::uvec2 const& size) {
    auto viewportState                  = state.getViewportState();
    viewportState.mViewports[0].mExtend = size;
    viewportState.mScissors[0].mExtend  = size;
    state.setViewportState(viewportState);
    return true;
  });

  struct PushConstants {
    glm::vec2 pos  = glm::vec2(0.2, 0.0);
    float     time = 0;
  } pushConstants;

  renderPass->drawFunc = [&state, &pushConstants, &set](
                           std::shared_ptr<Illusion::Graphics::CommandBuffer> cmd,
                           Illusion::Graphics::RenderPass const&              pass,
                           uint32_t                                           subPass) {
    auto pipeline = pass.getPipelineHandle(state, subPass);
    pushConstants.time += 0.001;
    cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    cmd->bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      *state.getShaderProgram()->getPipelineLayout(),
      set->getSet(),
      *set,
      nullptr);
    cmd->pushConstants(
      *state.getShaderProgram()->getPipelineLayout(),
      vk::ShaderStageFlagBits::eVertex,
      pushConstants);
    cmd->draw(4, 1, 0, 0);
  };

  Illusion::Core::FPSCounter fpsCounter;
  fpsCounter.pFPS.onChange().connect([window](float fps) {
    std::stringstream title;
    title << appName << " (" << std::floor(fps) << " fps)";
    window->pTitle = title.str();
    return true;
  });

  while (!window->shouldClose()) {
    window->processInput();
    renderPass->render();
    fpsCounter.step();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return 0;
}
