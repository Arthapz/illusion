////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                //
//   _)  |  |            _)                 This software may be modified and distributed         //
//    |  |  |  |  | (_-<  |   _ \    \      under the terms of the MIT license.                   //
//   _| _| _| \_,_| ___/ _| \___/ _| _|     See the LICENSE file for details.                     //
//                                                                                                //
//  Authors: Simon Schneegans (code@simonschneegans.de)                                           //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ILLUSION_COLOR_HPP
#define ILLUSION_COLOR_HPP

#include <glm/glm.hpp>
#include <string>

namespace Illusion::Core {

////////////////////////////////////////////////////////////////////////////////////////////////////
// This class stores color values in RGBA manner, but provides an HSV interface as well.          //
////////////////////////////////////////////////////////////////////////////////////////////////////

class Color {

 public:
  // Constructs a Color with all values set to 0 (black).
  Color() = default;

  // Parses an HTML-style presentation of a color. For example white would be
  // rgba(255, 255, 255, 1) while a semi-transparent red is rgba(255, 0, 0, 0.5).
  // Colors without alpha such as rgb(123, 234, 123) are supported as well.
  Color(std::string const& htmlRgba);

  // Constructs a Color from given RGB values.
  Color(float red, float green, float blue, float alpha = 1.f);

  // Returns a single Color component.
  float r() const;
  float g() const;
  float b() const;
  float h() const;
  float s() const;
  float v() const;
  float a() const;

  // Sets a single Color component.
  void r(float red);
  void g(float green);
  void b(float blue);
  void h(float hue);
  void s(float saturation);
  void v(float value);
  void a(float alpha);

  // Returns an inverted copy of the Color.
  Color inverted() const;

  // Returns an inverted copy of the Color.
  Color brightened() const;

  // Returns only red, green and blue as a glm::vec3.
  glm::vec3 vec3() const;

  // Returns red, green, blue and alpha as a glm::vec4.
  glm::vec4 const& vec4() const;

  std::string htmlRgba() const;
  void        htmlRgba(std::string const& val);

  float  operator[](unsigned rhs) const;
  float& operator[](unsigned rhs);

 private:
  void setHsv(float hue, float saturation, float value);

  glm::vec4 mVal = glm::vec4(0, 0, 0, 1);
};

//  operators --------------------------------------------------------------------------------------
bool operator==(Color const& lhs, Color const& rhs);
bool operator!=(Color const& lhs, Color const& rhs);

// Multiplication of a color with a float.
Color operator*(float const& lhs, Color rhs);
Color operator*(Color const& lhs, float rhs);

// Addition of two colors. Clamped.
Color operator+(Color const& lhs, Color const& rhs);

// Subtraction of two colors. Clamped.
Color operator-(Color const& lhs, Color const& rhs);

// Division of a color by a float.
Color operator/(Color const& lhs, float rhs);

std::ostream& operator<<(std::ostream& os, Color const& color);
} // namespace Illusion::Core

#endif // ILLUSION_COLOR_HPP