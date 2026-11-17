#ifndef PARTICLE_HLSLI
#define PARTICLE_HLSLI

struct Particle
{
    float3 pos;
    float  life;
    float3 vel;
    float  lifeMax;
    float4 color;
    float  size;
    float  sizeBirth;
    float2 pad;
};

#endif
