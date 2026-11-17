#include "common/registers_compute.hlsli"
#include "common/particle.hlsli"

cbuffer ParticleCB : ACTION_DATA_CB
{
    float  act_Dt;
    float  act_Time;
    float  act_EmitRatio;
    float  act_Shape;

    float3 act_EmitPos;
    float  act_ConeAngle;

    float3 act_EmitDir;
    float  act_Drag;

    float3 act_ShapeSize;
    float  act_Pad0;

    float3 act_Accel;
    float  act_Pad1;

    float  act_SpeedMin;
    float  act_SpeedMax;
    float  act_LifeMin;
    float  act_LifeMax;

    float  act_SizeMin;
    float  act_SizeMax;
    float  act_SizeEnd;
    float  act_Pad2;

    float3 act_ColorStart;
    float  act_Pad3;

    float3 act_ColorEnd;
};


RWStructuredBuffer<Particle> g_Particles : CS_WRITE_BUFFER(0);


uint Hash(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

float Rand(inout uint seed)
{
    seed = Hash(seed);
    return (float)(seed >> 8) * (1.0f / 16777216.0f);
}

float3 RandDirection(inout uint seed)
{
    const float z = 1.0f - 2.0f * Rand(seed);
    const float r = sqrt(max(1.0f - z * z, 0.0f));
    const float angle = Rand(seed) * 6.2831853f;

    return float3(cos(angle) * r, sin(angle) * r, z);
}

float3 RandCone(float3 axis, float halfAngle, inout uint seed)
{
    const float3 helper = abs(axis.y) < 0.99f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    const float3 side0 = normalize(cross(helper, axis));
    const float3 side1 = cross(axis, side0);

    const float tilt = Rand(seed) * halfAngle;
    const float spin = Rand(seed) * 6.2831853f;

    return axis * cos(tilt) + (side0 * cos(spin) + side1 * sin(spin)) * sin(tilt);
}


[numthreads(64, 1, 1)]
void CSMain(uint3 threadId : SV_DispatchThreadID)
{
    Particle p = g_Particles[threadId.x];

    p.life -= act_Dt;

    if (p.life > 0.0f)
    {
        p.vel += act_Accel * act_Dt;
        p.vel *= saturate(1.0f - act_Drag * act_Dt);
        p.pos += p.vel * act_Dt;

        const float age = 1.0f - p.life / p.lifeMax;
        p.color = float4(lerp(act_ColorStart, act_ColorEnd, age), 1.0f);
        p.size = p.sizeBirth * lerp(1.0f, act_SizeEnd, age);

        g_Particles[threadId.x] = p;
        return;
    }

    uint slotSeed = threadId.x;

    if (Rand(slotSeed) >= act_EmitRatio)
    {
        p.size = 0.0f;

        g_Particles[threadId.x] = p;
        return;
    }

    uint seed = Hash(threadId.x) ^ Hash(asuint(act_Time));

    if (p.lifeMax == 0.0f)
    {
        p.lifeMax = act_LifeMax * Rand(seed);
        p.life = p.lifeMax;

        g_Particles[threadId.x] = p;
        return;
    }

    const float3 axis = dot(act_EmitDir, act_EmitDir) > 0.0f ? normalize(act_EmitDir) : float3(0.0f, 1.0f, 0.0f);

    float3 pos = act_EmitPos;
    float3 dir = axis;

    if (act_Shape < 0.5f)
    {
        dir = RandCone(axis, act_ConeAngle, seed);
    }
    else if (act_Shape < 1.5f)
    {
        dir = RandDirection(seed);
        pos += dir * act_ShapeSize.x * pow(Rand(seed), 1.0f / 3.0f);
    }
    else
    {
        const float ox = Rand(seed) * 2.0f - 1.0f;
        const float oy = Rand(seed) * 2.0f - 1.0f;
        const float oz = Rand(seed) * 2.0f - 1.0f;
        pos += float3(ox, oy, oz) * act_ShapeSize;
    }

    p.pos = pos;
    p.vel = dir * lerp(act_SpeedMin, act_SpeedMax, Rand(seed));
    p.lifeMax = lerp(act_LifeMin, act_LifeMax, Rand(seed));
    p.life = p.lifeMax;
    p.sizeBirth = lerp(act_SizeMin, act_SizeMax, Rand(seed));
    p.size = p.sizeBirth;
    p.color = float4(act_ColorStart, 1.0f);

    g_Particles[threadId.x] = p;
}
