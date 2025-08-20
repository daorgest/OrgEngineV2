[[vk::binding(0, 0)]]
cbuffer ParticleUBO
{
    float deltaTime;
};

struct Particle
{
    float2 position;
    float2 velocity;
    float4 color;
};

// Input and output buffers
[[vk::binding(1, 0)]]
StructuredBuffer<Particle> particleIn;

[[vk::binding(2, 0)]]
RWStructuredBuffer<Particle> particleOut;

[shader("compute")]
[numthreads(256, 1, 1)]
void compMain(uint3 threadId : SV_DispatchThreadID)
{
    uint index = threadId.x;

    // Read input particle
    Particle p = particleIn[index];

    // Update position
    p.position += p.velocity * deltaTime;

    // Flip velocity at bounds
    if (p.position.x <= -1.0 || p.position.x >= 1.0) {
        p.velocity.x = -p.velocity.x;
    }
    if (p.position.y <= -1.0 || p.position.y >= 1.0) {
        p.velocity.y = -p.velocity.y;
    }

    // Write updated particle
    particleOut[index] = p;
}
