@group(1) @binding(0) var arg_0 : texture_depth_2d_array;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSample_60bf45() {
  var arg_2 = vec2<f32>();
  var arg_3 = 1;
  const arg_4 = vec2<i32>();
  var res : f32 = textureSample(arg_0, arg_1, arg_2, arg_3, arg_4);
}

@fragment
fn fragment_main() {
  textureSample_60bf45();
}
