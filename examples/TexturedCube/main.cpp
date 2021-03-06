////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//    _)  |  |            _)                This code may be used and modified under the terms    //
//     |  |  |  |  | (_-<  |   _ \    \     of the MIT license. See the LICENSE file for details. //
//    _| _| _| \_,_| ___/ _| \___/ _| _|    Copyright (c) 2018-2019 Simon Schneegans              //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Illusion/Core/Logger.hpp>
#include <Illusion/Core/RingBuffer.hpp>
#include <Illusion/Core/Timer.hpp>
#include <Illusion/Graphics/CoherentUniformBuffer.hpp>
#include <Illusion/Graphics/CommandBuffer.hpp>
#include <Illusion/Graphics/Instance.hpp>
#include <Illusion/Graphics/RenderPass.hpp>
#include <Illusion/Graphics/Shader.hpp>
#include <Illusion/Graphics/Texture.hpp>
#include <Illusion/Graphics/Window.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <thread>

////////////////////////////////////////////////////////////////////////////////////////////////////
// When compared to the ShaderSandbox example, this example is a bit more involved in several     //
// ways. On the one hand, we use actual vertex and index buffers, on the other, we use a set of   //
// per-frame resources so that we can start recording the next frame while the last one is still  //
// being processed.                                                                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// clang-format off
const std::array<glm::vec3, 26> POSITIONS = {
  glm::vec3( 1, -1, 1),  glm::vec3(-1, -1, -1), glm::vec3( 1, -1, -1), glm::vec3(-1,  1, -1),
  glm::vec3( 1,  1,  1), glm::vec3( 1,  1, -1), glm::vec3( 1,  1, -1), glm::vec3( 1, -1,  1),
  glm::vec3( 1, -1, -1), glm::vec3( 1,  1,  1), glm::vec3(-1, -1,  1), glm::vec3( 1, -1,  1),
  glm::vec3(-1, -1,  1), glm::vec3(-1,  1, -1), glm::vec3(-1, -1, -1), glm::vec3( 1, -1, -1),
  glm::vec3(-1,  1, -1), glm::vec3( 1,  1, -1), glm::vec3(-1, -1,  1), glm::vec3(-1,  1,  1),
  glm::vec3( 1,  1, -1), glm::vec3( 1,  1,  1), glm::vec3( 1, -1,  1), glm::vec3(-1,  1,  1),
  glm::vec3(-1,  1,  1), glm::vec3(-1, -1, -1)};

const std::array<glm::vec3, 26> NORMALS = {
  glm::vec3( 0, -1,  0), glm::vec3( 0, -1,  0), glm::vec3( 0, -1,  0), glm::vec3( 0,  1,  0),
  glm::vec3( 0,  1,  0), glm::vec3( 0,  1,  0), glm::vec3( 1,  0,  0), glm::vec3( 1,  0,  0),
  glm::vec3( 1,  0,  0), glm::vec3( 0,  0,  1), glm::vec3( 0,  0,  1), glm::vec3( 0,  0,  1),
  glm::vec3(-1,  0,  0), glm::vec3(-1,  0,  0), glm::vec3(-1,  0,  0), glm::vec3( 0,  0, -1),
  glm::vec3( 0,  0, -1), glm::vec3( 0,  0, -1), glm::vec3( 0, -1,  0), glm::vec3( 0,  1,  0),
  glm::vec3( 1,  0,  0), glm::vec3( 1,  0,  0), glm::vec3( 1,  0,  0), glm::vec3( 0,  0,  1),
  glm::vec3(-1,  0,  0), glm::vec3( 0,  0, -1)};

const std::array<glm::vec2, 26> TEXCOORDS = {
  glm::vec2(1, 0), glm::vec2(0, 1), glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(0, 1),
  glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(0, 1), glm::vec2(0, 0), glm::vec2(1, 0), 
  glm::vec2(0, 1), glm::vec2(0, 0), glm::vec2(0, 0), glm::vec2(1, 1), glm::vec2(0, 1), 
  glm::vec2(1, 0), glm::vec2(0, 1), glm::vec2(0, 0), glm::vec2(1, 1), glm::vec2(1, 1), 
  glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1), glm::vec2(1, 1), glm::vec2(1, 0), 
  glm::vec2(1, 1)};

const std::array<uint32_t, 36> INDICES = {
  0, 1,  2, 3, 4,  5, 6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,
  0, 18, 1, 3, 19, 4, 20, 21, 22, 9, 23, 10, 12, 24, 13, 15, 25, 16,
};
// clang-format on

////////////////////////////////////////////////////////////////////////////////////////////////////
// This struct contains all resources we will need for one frame. While one frame is processed by //
// the GPU, we will acquire a instance of FrameResources and work with that one. We will store    //
// the FrameResources in a ring-buffer and re-use older FrameResources after some frames when the //
// GPU is likely to be finished processing it anyways.                                            //
// The FrameResources contain a command buffer, a render pass, a uniform buffer (for the          //
// projection matrix), a semaphore indicating when rendering has finished and the frame buffer is //
// ready for presentation and a fence telling us when the FrameResources are ready to be re-used. //
////////////////////////////////////////////////////////////////////////////////////////////////////

struct FrameResources {
  FrameResources(Illusion::Graphics::DevicePtr const& device)
      : mCmd(Illusion::Graphics::CommandBuffer::create(device))
      , mRenderPass(Illusion::Graphics::RenderPass::create(device))
      , mUniformBuffer(Illusion::Graphics::CoherentUniformBuffer::create(device, sizeof(glm::mat4)))
      , mFrameFinishedFence(device->createFence())
      , mRenderFinishedSemaphore(device->createSemaphore()) {

    // In addition to a color buffer we will need a depth buffer for depth testing.
    mRenderPass->addAttachment(vk::Format::eR8G8B8A8Unorm);
    mRenderPass->addAttachment(vk::Format::eD32Sfloat);

    // The indices are provided as a triangle list
    mCmd->graphicsState().setTopology(vk::PrimitiveTopology::eTriangleList);

    // Here we define what kind of vertex buffers will be bound. The vertex data (positions, normals
    // and texture coordinates) actually comes from three different vertex buffer objects.
    mCmd->graphicsState().setVertexInputBindings(
        {{0, sizeof(glm::vec3), vk::VertexInputRate::eVertex},
            {1, sizeof(glm::vec3), vk::VertexInputRate::eVertex},
            {2, sizeof(glm::vec2), vk::VertexInputRate::eVertex}});

    // Here we define which vertex attribute comes from which vertex buffer.
    mCmd->graphicsState().setVertexInputAttributes({{0, 0, vk::Format::eR32G32B32Sfloat, 0},
        {1, 1, vk::Format::eR32G32B32Sfloat, 0}, {2, 2, vk::Format::eR32G32Sfloat, 0}});
  }

  Illusion::Graphics::CommandBufferPtr         mCmd;
  Illusion::Graphics::RenderPassPtr            mRenderPass;
  Illusion::Graphics::CoherentUniformBufferPtr mUniformBuffer;
  vk::FencePtr                                 mFrameFinishedFence;
  vk::SemaphorePtr                             mRenderFinishedSemaphore;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

int main() {

  // Then we start setting up our Vulkan resources.
  auto instance = Illusion::Graphics::Instance::create("Textured Cube Demo");
  auto device   = Illusion::Graphics::Device::create(instance->getPhysicalDevice());
  auto window   = Illusion::Graphics::Window::create(instance, device);

  // Here we load the texture. This supports many file formats (those supported by gli and stb).
  auto texture = Illusion::Graphics::Texture::createFromFile(device, "data/textures/box.dds");

  // Load the shader. You can have a look at the files for some more comments on how they work.
  auto shader = Illusion::Graphics::Shader::createFromFiles(
      device, {"data/shaders/TexturedCube.vert", "data/shaders/TexturedCube.frag"});

  // Here we create our three vertex buffers and one index buffer.
  auto positionBuffer = device->createVertexBuffer(POSITIONS);
  auto normalBuffer   = device->createVertexBuffer(NORMALS);
  auto texcoordBuffer = device->createVertexBuffer(TEXCOORDS);
  auto indexBuffer    = device->createIndexBuffer(INDICES);

  // Now we create a RingBuffer of FrameResources. We use only two entries in this example but you
  // could change this number to whatever you like. Three or four could improve the performance in
  // some cases, but this will use more memory and could also lead to some input lag. Usually two or
  // three will be a could choice.
  Illusion::Core::RingBuffer<FrameResources, 2> frameResources{
      FrameResources(device), FrameResources(device)};

  // Use a timer to get the current system time at each frame.
  Illusion::Core::Timer timer;

  // Then we open our window.
  window->open();

  // And start the application loop.
  while (!window->shouldClose()) {

    // This will trigger re-creations of the swapchain and make sure that window->shouldClose()
    // actually returns true when the user closed the window.
    window->update();

    // First, we acquire the next FrameResources instance from our RingBuffer.
    auto& res = frameResources.next();

    // Then we have to wait until the GPU has finished processing the corresponding frame. Usually
    // this should return instantly because, thanks to the RingBuffer there was at least one frame
    // in between.
    device->waitForFences(*res.mFrameFinishedFence);
    device->resetFences(*res.mFrameFinishedFence);

    // Get the current time for animations.
    float time = timer.getElapsed();

    // As we are re-recording our command buffer, we have to reset it before starting to record new
    // commands.
    res.mCmd->reset();
    res.mCmd->begin();

    // Set the shader to be used.
    res.mCmd->setShader(shader);

    // Adapt the render pass and viewport sizes.
    res.mRenderPass->setExtent(window->pExtent.get());
    res.mCmd->graphicsState().setViewports({{glm::vec2(window->pExtent.get())}});

    // Compute a projection matrix and write the data to our uniform buffer.
    glm::mat4 projection = glm::perspectiveZO(glm::radians(60.f),
        static_cast<float>(window->pExtent.get().x) / static_cast<float>(window->pExtent.get().y),
        0.1f, 100.0f);
    projection[1][1] *= -1; // flip for y-down
    res.mUniformBuffer->updateData(projection);

    // Bind the uniform buffer to descriptor set 0
    res.mCmd->bindingState().setUniformBuffer(
        res.mUniformBuffer->getBuffer(), sizeof(glm::mat4), 0, 0, 0);

    // Bind the texture to descriptor set 1
    res.mCmd->bindingState().setTexture(texture, 1, 0);

    // Begin our render pass.
    res.mCmd->beginRenderPass(res.mRenderPass);

    // Compute a modelView matrix based on the simulation time (this makes th cube spin). Then
    // upload this matrix via push constants.
    glm::mat4 modelView(1.f);
    modelView = glm::translate(modelView, glm::vec3(0, 0, -3));
    modelView = glm::rotate(modelView, -time * 0.5f, glm::vec3(0, 1, 0));
    modelView = glm::rotate(modelView, time * 0.3f, glm::vec3(1, 0, 0));
    res.mCmd->pushConstants(modelView);

    // Bind the three vertex buffers and the index buffer.
    res.mCmd->bindVertexBuffers(0, {positionBuffer, normalBuffer, texcoordBuffer});
    res.mCmd->bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);

    // Do the actual drawing.
    res.mCmd->drawIndexed(static_cast<uint32_t>(INDICES.size()), 1, 0, 0, 0);

    // End the render pass and finish recording of the command buffer.
    res.mCmd->endRenderPass();
    res.mCmd->end();

    // Now we can just submit the command buffer. Once it has been processed, the
    // renderFinishedSemaphore will be signaled.
    res.mCmd->submit({}, {}, {*res.mRenderFinishedSemaphore});

    // Present the color attachment of the render pass on the window. This operation will wait for
    // the renderFinishedSemaphore and signal the frameFinishedFence so that we know when to start
    // the next frame.
    window->present(res.mRenderPass->getFramebuffer()->getImages()[0], res.mRenderFinishedSemaphore,
        res.mFrameFinishedFence);

    // Prevent the GPU from over-heating :)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  // The window has been closed. We wait for all pending operations and then all objects will be
  // deleted automatically in the correct order.
  device->waitIdle();

  return 0;
}
