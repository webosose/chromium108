#version 310 es

struct Particle {
  vec3 position[8];
  float lifetime;
  uint pad_3;
  uint pad_4;
  uint pad_5;
  vec4 color;
  vec3 velocity;
  uint pad_6;
};

layout(binding = 3, std430) buffer Particles_ssbo {
  Particle p[];
} particles;

layout(binding = 4, std140) uniform Simulation_ubo {
  uint i;
  uint pad;
  uint pad_1;
  uint pad_2;
} sim;

void tint_symbol() {
  Particle particle = particles.p[0];
  particle.position[sim.i] = particle.position[sim.i];
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
