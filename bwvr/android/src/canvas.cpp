#include "canvas.h"

#include <android/bitmap.h>
#include <string.h>

#include "app_log.h"

namespace {

// A global reference of a local one, and the local one goes.
jobject Keep(JNIEnv* env, jobject local) {
  if (local == nullptr) return nullptr;
  jobject global = env->NewGlobalRef(local);
  env->DeleteLocalRef(local);
  return global;
}

jobject StaticField(JNIEnv* env, const char* klass, const char* name,
                    const char* type) {
  jclass c = env->FindClass(klass);
  if (c == nullptr) return nullptr;
  jfieldID f = env->GetStaticFieldID(c, name, type);
  jobject value = f != nullptr ? env->GetStaticObjectField(c, f) : nullptr;
  env->DeleteLocalRef(c);
  return Keep(env, value);
}

bool Failed(JNIEnv* env, const char* what) {
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    AppLog("canvas       : %s failed with an exception of Java", what);
    return true;
  }
  return false;
}

}  // namespace

bool Canvas2D::Start(JNIEnv* env, int width, int height) {
  env_ = env;
  width_ = width;
  height_ = height;

  jclass bitmap_class = env->FindClass("android/graphics/Bitmap");
  jclass canvas_class = env->FindClass("android/graphics/Canvas");
  jclass paint_class = env->FindClass("android/graphics/Paint");
  jclass typeface_class = env->FindClass("android/graphics/Typeface");
  if (Failed(env, "FindClass") || bitmap_class == nullptr ||
      canvas_class == nullptr || paint_class == nullptr ||
      typeface_class == nullptr) {
    return false;
  }

  jobject argb8888 = StaticField(env, "android/graphics/Bitmap$Config",
                                 "ARGB_8888",
                                 "Landroid/graphics/Bitmap$Config;");
  jmethodID create_bitmap = env->GetStaticMethodID(
      bitmap_class, "createBitmap",
      "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
  bitmap_ = Keep(env, env->CallStaticObjectMethod(bitmap_class, create_bitmap,
                                                  width, height, argb8888));
  env->DeleteGlobalRef(argb8888);
  if (Failed(env, "Bitmap.createBitmap") || bitmap_ == nullptr) return false;

  jmethodID canvas_new =
      env->GetMethodID(canvas_class, "<init>", "(Landroid/graphics/Bitmap;)V");
  canvas_ = Keep(env, env->NewObject(canvas_class, canvas_new, bitmap_));
  jmethodID paint_new = env->GetMethodID(paint_class, "<init>", "(I)V");
  // ANTI_ALIAS_FLAG | FILTER_BITMAP_FLAG | SUBPIXEL_TEXT_FLAG.
  paint_ = Keep(env, env->NewObject(paint_class, paint_new, 0x01 | 0x02 | 0x80));
  if (Failed(env, "new Canvas, new Paint")) return false;

  jobject sans = StaticField(env, "android/graphics/Typeface", "SANS_SERIF",
                              "Landroid/graphics/Typeface;");
  jmethodID typeface_create = env->GetStaticMethodID(
      typeface_class, "create",
      "(Landroid/graphics/Typeface;I)Landroid/graphics/Typeface;");
  font_ = Keep(env, env->CallStaticObjectMethod(typeface_class,
                                                 typeface_create, sans, 0));
  font_bold_ = Keep(env, env->CallStaticObjectMethod(
                              typeface_class, typeface_create, sans, 1));
  env->DeleteGlobalRef(sans);

  style_fill_ = StaticField(env, "android/graphics/Paint$Style", "FILL",
                            "Landroid/graphics/Paint$Style;");
  style_stroke_ = StaticField(env, "android/graphics/Paint$Style", "STROKE",
                              "Landroid/graphics/Paint$Style;");
  align_[kLeftAlign] = StaticField(env, "android/graphics/Paint$Align", "LEFT",
                                   "Landroid/graphics/Paint$Align;");
  align_[kCenterAlign] = StaticField(env, "android/graphics/Paint$Align",
                                     "CENTER", "Landroid/graphics/Paint$Align;");
  align_[kRightAlign] = StaticField(env, "android/graphics/Paint$Align",
                                    "RIGHT", "Landroid/graphics/Paint$Align;");
  tile_clamp_ = StaticField(env, "android/graphics/Shader$TileMode", "CLAMP",
                            "Landroid/graphics/Shader$TileMode;");
  mode_clear_ = StaticField(env, "android/graphics/PorterDuff$Mode", "CLEAR",
                            "Landroid/graphics/PorterDuff$Mode;");

  jclass gradient = env->FindClass("android/graphics/LinearGradient");
  gradient_class_ = static_cast<jclass>(env->NewGlobalRef(gradient));
  env->DeleteLocalRef(gradient);
  gradient_new_ = env->GetMethodID(gradient_class_, "<init>",
                                   "(FFFFIILandroid/graphics/Shader$TileMode;)V");

  set_color_ = env->GetMethodID(paint_class, "setColor", "(I)V");
  set_style_ = env->GetMethodID(paint_class, "setStyle",
                                "(Landroid/graphics/Paint$Style;)V");
  set_stroke_width_ = env->GetMethodID(paint_class, "setStrokeWidth", "(F)V");
  set_text_size_ = env->GetMethodID(paint_class, "setTextSize", "(F)V");
  set_typeface_ = env->GetMethodID(
      paint_class, "setTypeface",
      "(Landroid/graphics/Typeface;)Landroid/graphics/Typeface;");
  set_text_align_ = env->GetMethodID(paint_class, "setTextAlign",
                                     "(Landroid/graphics/Paint$Align;)V");
  set_shader_ = env->GetMethodID(paint_class, "setShader",
                                 "(Landroid/graphics/Shader;)Landroid/graphics/Shader;");
  set_shadow_ = env->GetMethodID(paint_class, "setShadowLayer", "(FFFI)V");
  clear_shadow_ = env->GetMethodID(paint_class, "clearShadowLayer", "()V");
  measure_text_ = env->GetMethodID(paint_class, "measureText",
                                   "(Ljava/lang/String;)F");
  draw_color_mode_ = env->GetMethodID(canvas_class, "drawColor",
                                      "(ILandroid/graphics/PorterDuff$Mode;)V");
  draw_rect_ = env->GetMethodID(canvas_class, "drawRect",
                                "(FFFFLandroid/graphics/Paint;)V");
  draw_round_rect_ = env->GetMethodID(canvas_class, "drawRoundRect",
                                      "(FFFFFFLandroid/graphics/Paint;)V");
  draw_circle_ = env->GetMethodID(canvas_class, "drawCircle",
                                  "(FFFLandroid/graphics/Paint;)V");
  draw_line_ = env->GetMethodID(canvas_class, "drawLine",
                                "(FFFFLandroid/graphics/Paint;)V");
  draw_text_ = env->GetMethodID(canvas_class, "drawText",
                                "(Ljava/lang/String;FFLandroid/graphics/Paint;)V");

  env->DeleteLocalRef(bitmap_class);
  env->DeleteLocalRef(canvas_class);
  env->DeleteLocalRef(paint_class);
  env->DeleteLocalRef(typeface_class);
  if (Failed(env, "the methods of Paint and Canvas") || draw_text_ == nullptr ||
      font_bold_ == nullptr || mode_clear_ == nullptr) {
    Stop();
    return false;
  }
  AppLog("canvas       : a picture of %d by %d with the sans serif font "
         "of the system", width, height);
  return true;
}

void Canvas2D::Stop() {
  if (env_ == nullptr) return;
  jobject* refs[] = {&bitmap_,     &canvas_,       &paint_,     &font_,
                     &font_bold_, &style_fill_,   &style_stroke_,
                     &align_[0],   &align_[1],     &align_[2],  &tile_clamp_,
                     &mode_clear_};
  for (jobject* r : refs) {
    if (*r != nullptr) env_->DeleteGlobalRef(*r);
    *r = nullptr;
  }
  if (gradient_class_ != nullptr) env_->DeleteGlobalRef(gradient_class_);
  gradient_class_ = nullptr;
}

void Canvas2D::UsePaint(uint32_t argb, bool fill, float stroke) {
  // setShader gives the shader before. CheckJNI wants the call of its type.
  jobject old = env_->CallObjectMethod(paint_, set_shader_, nullptr);
  if (old != nullptr) env_->DeleteLocalRef(old);
  env_->CallVoidMethod(paint_, set_color_, static_cast<jint>(argb));
  env_->CallVoidMethod(paint_, set_style_, fill ? style_fill_ : style_stroke_);
  env_->CallVoidMethod(paint_, set_stroke_width_, stroke);
}

jstring Canvas2D::Str(const std::string& text) {
  return env_->NewStringUTF(text.c_str());
}

void Canvas2D::Clear() {
  env_->CallVoidMethod(canvas_, draw_color_mode_, 0, mode_clear_);
}

void Canvas2D::FillRect(float l, float t, float r, float b, uint32_t argb) {
  UsePaint(argb, true, 0.0f);
  env_->CallVoidMethod(canvas_, draw_rect_, l, t, r, b, paint_);
}

void Canvas2D::FillRoundRect(float l, float t, float r, float b, float radius,
                             uint32_t argb) {
  UsePaint(argb, true, 0.0f);
  env_->CallVoidMethod(canvas_, draw_round_rect_, l, t, r, b, radius, radius,
                       paint_);
}

void Canvas2D::StrokeRect(float l, float t, float r, float b, float width,
                          uint32_t argb) {
  UsePaint(argb, false, width);
  env_->CallVoidMethod(canvas_, draw_rect_, l, t, r, b, paint_);
}

void Canvas2D::StrokeRoundRect(float l, float t, float r, float b,
                               float radius, float width, uint32_t argb) {
  UsePaint(argb, false, width);
  env_->CallVoidMethod(canvas_, draw_round_rect_, l, t, r, b, radius, radius,
                       paint_);
}

void Canvas2D::GradientRect(float l, float t, float r, float b, float x0,
                            float y0, float x1, float y1, uint32_t c0,
                            uint32_t c1, float radius) {
  UsePaint(0xFFFFFFFFu, true, 0.0f);
  jobject shader = env_->NewObject(gradient_class_, gradient_new_, x0, y0, x1,
                                   y1, static_cast<jint>(c0),
                                   static_cast<jint>(c1), tile_clamp_);
  jobject old = env_->CallObjectMethod(paint_, set_shader_, shader);
  if (old != nullptr) env_->DeleteLocalRef(old);
  if (radius > 0.0f) {
    env_->CallVoidMethod(canvas_, draw_round_rect_, l, t, r, b, radius, radius,
                         paint_);
  } else {
    env_->CallVoidMethod(canvas_, draw_rect_, l, t, r, b, paint_);
  }
  old = env_->CallObjectMethod(paint_, set_shader_, nullptr);
  if (old != nullptr) env_->DeleteLocalRef(old);
  env_->DeleteLocalRef(shader);
}

void Canvas2D::FillCircle(float cx, float cy, float radius, uint32_t argb) {
  UsePaint(argb, true, 0.0f);
  env_->CallVoidMethod(canvas_, draw_circle_, cx, cy, radius, paint_);
}

void Canvas2D::Line(float x0, float y0, float x1, float y1, float width,
                    uint32_t argb) {
  UsePaint(argb, false, width);
  env_->CallVoidMethod(canvas_, draw_line_, x0, y0, x1, y1, paint_);
}

void Canvas2D::Text(const std::string& text, float x, float y, float size,
                    uint32_t argb, bool bold, Align align, bool shadow) {
  UsePaint(argb, true, 0.0f);
  jobject old = env_->CallObjectMethod(paint_, set_typeface_,
                                       bold ? font_bold_ : font_);
  if (old != nullptr) env_->DeleteLocalRef(old);
  env_->CallVoidMethod(paint_, set_text_size_, size);
  env_->CallVoidMethod(paint_, set_text_align_, align_[align]);
  if (shadow) {
    env_->CallVoidMethod(paint_, set_shadow_, size * 0.12f, size * 0.05f,
                         size * 0.06f, static_cast<jint>(0xE0000000u));
  }
  jstring s = Str(text);
  env_->CallVoidMethod(canvas_, draw_text_, s, x, y, paint_);
  env_->DeleteLocalRef(s);
  if (shadow) env_->CallVoidMethod(paint_, clear_shadow_);
}

float Canvas2D::Measure(const std::string& text, float size, bool bold) {
  jobject old = env_->CallObjectMethod(paint_, set_typeface_,
                                       bold ? font_bold_ : font_);
  if (old != nullptr) env_->DeleteLocalRef(old);
  env_->CallVoidMethod(paint_, set_text_size_, size);
  jstring s = Str(text);
  const float w = env_->CallFloatMethod(paint_, measure_text_, s);
  env_->DeleteLocalRef(s);
  return w;
}

bool Canvas2D::ReadPixels(std::vector<uint8_t>* out) {
  AndroidBitmapInfo info;
  if (AndroidBitmap_getInfo(env_, bitmap_, &info) !=
      ANDROID_BITMAP_RESULT_SUCCESS) {
    return false;
  }
  void* pixels = nullptr;
  if (AndroidBitmap_lockPixels(env_, bitmap_, &pixels) !=
      ANDROID_BITMAP_RESULT_SUCCESS) {
    return false;
  }
  out->resize(static_cast<size_t>(width_) * height_ * 4);
  for (int y = 0; y < height_; ++y) {
    memcpy(out->data() + static_cast<size_t>(y) * width_ * 4,
           static_cast<const uint8_t*>(pixels) + static_cast<size_t>(y) * info.stride,
           static_cast<size_t>(width_) * 4);
  }
  AndroidBitmap_unlockPixels(env_, bitmap_);
  return true;
}
