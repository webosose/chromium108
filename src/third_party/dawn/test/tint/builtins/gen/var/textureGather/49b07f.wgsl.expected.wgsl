@group(1) @binding(1) var arg_1 : texture_2d<u32>;

@group(1) @binding(2) var arg_2 : sampler;

fn textureGather_49b07f() {
  const arg_0 = 1;
  var arg_3 = vec2<f32>();
  const arg_4 = vec2<i32>();
  var res : vec4<u32> = textureGather(arg_0, arg_1, arg_2, arg_3, arg_4);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureGather_49b07f();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureGather_49b07f();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureGather_49b07f();
}
