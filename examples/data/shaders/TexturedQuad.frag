////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//    _)  |  |            _)                This code may be used and modified under the terms    //
//     |  |  |  |  | (_-<  |   _ \    \     of the MIT license. See the LICENSE file for details. //
//    _| _| _| \_,_| ___/ _| \___/ _| _|    Copyright (c) 2018-2019 Simon Schneegans              //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#version 450

// inputs
layout(location = 0) in vec2 vTexcoords;

// uniforms
layout(binding = 0) uniform sampler2D texSampler;

// outputs
layout(location = 0) out vec4 outColor;

// Very simple shader which just samples a texture. It is used in the TexturedQuad example, which
// can also load HLSL code. See TexturedQuad.ps for the HLSL version of this shader.
void main() {
  outColor = texture(texSampler, vTexcoords);
}
