//
// Created by Orgest on 1/22/2026.
//

#pragma once
#include <barrier>
#include <thread>

#include "Vector.h"
#include "../PrimTypes.h"
struct Entity
{
    u32 id;

    u32 idx() const { return id & 0xFFFFF; }
    u32 gen() const { return id >> 20; }
};

struct Range
{
    size_t start;
    size_t end;
};

struct LaneContext
{
    u32 idx;
    u32 count;
    std::barrier<>* syncBarrier;

    u32 GetCount() const { return count & 0xFFF; }
    u32 GetIndex() const { return idx & 0xFFF; }
    void Sync() const { syncBarrier->arrive_and_wait(); }

    Range Range(const size_t totalCount) const
    {
        const size_t per_lane = totalCount / count;
        const size_t leftovers = totalCount % count;

        const size_t start = (per_lane * idx) + std::min(static_cast<size_t>(idx), leftovers);
        const size_t end = start + per_lane + (idx < leftovers ? 1 : 0);

        return { start, end };
    }
};

inline thread_local LaneContext laneContext;

template<typename F>
void Bootstrap(F&& entryPoint) {
    const u32 core_count = std::thread::hardware_concurrency();

    // std::barrier can take a completion function (a lambda)
    // that runs ONCE on a single thread when everyone arrives.
    std::barrier sync{core_count};

    Vector<std::jthread> lanes;
    lanes.reserve(core_count);

    for (u32 i = 0; i < core_count; ++i) {
        lanes.emplace_back([&, i] {
            laneContext = { i, core_count, &sync };
            entryPoint();
        });
    }
}