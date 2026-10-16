// A PICTURE OF THE CPU drawn with android.graphics.Canvas through JNI
// (docs/android.md, "The menu"). The app has no Java code: the text gets the
// sans serif font of the system, as the menu of the game has, and the
// shapes get the anti-aliasing of
// Android. The pixels are RGBA, 8 bits each, with the color multiplied by
// the alpha.
#pragma once

#include <jni.h>
#include <stdint.h>

#include <string>
#include <vector>

class Canvas2D {
 public:
  enum Align { kLeftAlign = 0, kCenterAlign = 1, kRightAlign = 2 };

  // env must be of the thread that draws. False when a class or a method of
  // Android is not there.
  bool Start(JNIEnv* env, int width, int height);
  void Stop();
  bool Ready() const { return bitmap_ != nullptr; }
  int Width() const { return width_; }
  int Height() const { return height_; }

  // Colors are 0xAARRGGBB.
  void Clear();
  void FillRect(float l, float t, float r, float b, uint32_t argb);
  void FillRoundRect(float l, float t, float r, float b, float radius,
                     uint32_t argb);
  void StrokeRect(float l, float t, float r, float b, float width,
                  uint32_t argb);
  void StrokeRoundRect(float l, float t, float r, float b, float radius,
                       float width, uint32_t argb);
  // A fill with a linear gradient from (x0, y0) in c0 to (x1, y1) in c1.
  void GradientRect(float l, float t, float r, float b, float x0, float y0,
                    float x1, float y1, uint32_t c0, uint32_t c1,
                    float radius);
  void FillCircle(float cx, float cy, float radius, uint32_t argb);
  void Line(float x0, float y0, float x1, float y1, float width,
            uint32_t argb);
  // Text with its baseline at y. shadow: a dark shadow under the text.
  void Text(const std::string& text, float x, float y, float size,
            uint32_t argb, bool bold, Align align, bool shadow);
  float Measure(const std::string& text, float size, bool bold);

  // The pixels of the picture, width * height * 4 bytes.
  bool ReadPixels(std::vector<uint8_t>* out);

 private:
  void UsePaint(uint32_t argb, bool fill, float stroke);
  jstring Str(const std::string& text);

  JNIEnv* env_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  jobject bitmap_ = nullptr;
  jobject canvas_ = nullptr;
  jobject paint_ = nullptr;
  jobject font_ = nullptr;
  jobject font_bold_ = nullptr;
  jobject style_fill_ = nullptr;
  jobject style_stroke_ = nullptr;
  jobject align_[3] = {nullptr, nullptr, nullptr};
  jobject tile_clamp_ = nullptr;
  jobject mode_clear_ = nullptr;
  jclass gradient_class_ = nullptr;
  jmethodID gradient_new_ = nullptr;
  jmethodID set_color_ = nullptr;
  jmethodID set_style_ = nullptr;
  jmethodID set_stroke_width_ = nullptr;
  jmethodID set_text_size_ = nullptr;
  jmethodID set_typeface_ = nullptr;
  jmethodID set_text_align_ = nullptr;
  jmethodID set_shader_ = nullptr;
  jmethodID set_shadow_ = nullptr;
  jmethodID clear_shadow_ = nullptr;
  jmethodID measure_text_ = nullptr;
  jmethodID draw_color_mode_ = nullptr;
  jmethodID draw_rect_ = nullptr;
  jmethodID draw_round_rect_ = nullptr;
  jmethodID draw_circle_ = nullptr;
  jmethodID draw_line_ = nullptr;
  jmethodID draw_text_ = nullptr;
};
