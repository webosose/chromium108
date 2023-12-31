#version 310 es
precision mediump float;

layout(location = 0) in vec4 vcolor_S0_param_1;
layout(location = 0) out vec4 sk_FragColor_1_1;
layout(binding = 0, std140) uniform UniformBuffer_ubo {
  uint pad;
  uint pad_1;
  uint pad_2;
  uint pad_3;
  float unknownInput_S1_c0;
  uint pad_4;
  uint pad_5;
  uint pad_6;
  vec4 ucolorRed_S1_c0;
  vec4 ucolorGreen_S1_c0;
  mat3 umatrix_S1;
} x_4;

vec4 sk_FragColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
bool sk_Clockwise = false;
vec4 vcolor_S0 = vec4(0.0f, 0.0f, 0.0f, 0.0f);
bool test_int_S1_c0_b() {
  int unknown = 0;
  bool ok = false;
  ivec4 val = ivec4(0, 0, 0, 0);
  bool x_40 = false;
  bool x_41 = false;
  bool x_54 = false;
  bool x_55 = false;
  bool x_65 = false;
  bool x_66 = false;
  float x_26 = x_4.unknownInput_S1_c0;
  int x_27 = int(x_26);
  unknown = x_27;
  ok = true;
  x_41 = false;
  if (true) {
    x_40 = all(equal((ivec4(0) / ivec4(x_27, x_27, x_27, x_27)), ivec4(0)));
    x_41 = x_40;
  }
  ok = x_41;
  ivec4 x_44 = ivec4(x_27, x_27, x_27, x_27);
  val = x_44;
  ivec4 x_47 = (x_44 + ivec4(1));
  val = x_47;
  ivec4 x_48 = (x_47 - ivec4(1));
  val = x_48;
  ivec4 x_49 = (x_48 + ivec4(1));
  val = x_49;
  ivec4 x_50 = (x_49 - ivec4(1));
  val = x_50;
  x_55 = false;
  if (x_41) {
    x_54 = all(equal(x_50, x_44));
    x_55 = x_54;
  }
  ok = x_55;
  ivec4 x_58 = (x_50 * ivec4(2));
  val = x_58;
  ivec4 x_59 = (x_58 / ivec4(2));
  val = x_59;
  ivec4 x_60 = (x_59 * ivec4(2));
  val = x_60;
  ivec4 x_61 = (x_60 / ivec4(2));
  val = x_61;
  x_66 = false;
  if (x_55) {
    x_65 = all(equal(x_61, x_44));
    x_66 = x_65;
  }
  ok = x_66;
  return x_66;
}

void main_1() {
  vec4 outputColor_S0 = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  vec4 output_S1 = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  float x_8_unknown = 0.0f;
  bool x_9_ok = false;
  vec4 x_10_val = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  vec4 x_116 = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  bool x_86 = false;
  bool x_87 = false;
  bool x_99 = false;
  bool x_100 = false;
  bool x_110 = false;
  bool x_111 = false;
  bool x_114 = false;
  bool x_115 = false;
  vec4 x_72 = vcolor_S0;
  outputColor_S0 = x_72;
  float x_77 = x_4.unknownInput_S1_c0;
  x_8_unknown = x_77;
  x_9_ok = true;
  x_87 = false;
  if (true) {
    x_86 = all(equal((vec4(0.0f) / vec4(x_77, x_77, x_77, x_77)), vec4(0.0f)));
    x_87 = x_86;
  }
  x_9_ok = x_87;
  vec4 x_89 = vec4(x_77, x_77, x_77, x_77);
  x_10_val = x_89;
  vec4 x_92 = (x_89 + vec4(1.0f));
  x_10_val = x_92;
  vec4 x_93 = (x_92 - vec4(1.0f));
  x_10_val = x_93;
  vec4 x_94 = (x_93 + vec4(1.0f));
  x_10_val = x_94;
  vec4 x_95 = (x_94 - vec4(1.0f));
  x_10_val = x_95;
  x_100 = false;
  if (x_87) {
    x_99 = all(equal(x_95, x_89));
    x_100 = x_99;
  }
  x_9_ok = x_100;
  vec4 x_103 = (x_95 * vec4(2.0f));
  x_10_val = x_103;
  vec4 x_104 = (x_103 / vec4(2.0f));
  x_10_val = x_104;
  vec4 x_105 = (x_104 * vec4(2.0f));
  x_10_val = x_105;
  vec4 x_106 = (x_105 / vec4(2.0f));
  x_10_val = x_106;
  x_111 = false;
  if (x_100) {
    x_110 = all(equal(x_106, x_89));
    x_111 = x_110;
  }
  x_9_ok = x_111;
  x_115 = false;
  if (x_111) {
    x_114 = test_int_S1_c0_b();
    x_115 = x_114;
  }
  if (x_115) {
    vec4 x_122 = x_4.ucolorGreen_S1_c0;
    x_116 = x_122;
  } else {
    vec4 x_124 = x_4.ucolorRed_S1_c0;
    x_116 = x_124;
  }
  vec4 x_125 = x_116;
  output_S1 = x_125;
  sk_FragColor = x_125;
  return;
}

struct main_out {
  vec4 sk_FragColor_1;
};

main_out tint_symbol(bool sk_Clockwise_param, vec4 vcolor_S0_param) {
  sk_Clockwise = sk_Clockwise_param;
  vcolor_S0 = vcolor_S0_param;
  main_1();
  main_out tint_symbol_1 = main_out(sk_FragColor);
  return tint_symbol_1;
}

void main() {
  main_out inner_result = tint_symbol(gl_FrontFacing, vcolor_S0_param_1);
  sk_FragColor_1_1 = inner_result.sk_FragColor_1;
  return;
}
