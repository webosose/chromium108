@group(1) @binding(1) var arg_1 : texture_2d_array<i32>;

@group(1) @binding(2) var arg_2 : sampler;

fn textureGather_8b754c() {
  const arg_0 = 1;
  var arg_3 = vec2<f32>();
  var arg_4 = 1;
  var res : vec4<i32> = textureGather(arg_0, arg_1, arg_2, arg_3, arg_4);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureGather_8b754c();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureGather_8b754c();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureGather_8b754c();
}
