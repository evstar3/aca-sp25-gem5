/*
 * Copyright (c) 2010-2019, 2024 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
 * Copyright (c) 2010,2015 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Cache definitions.
 */

#include "mem/cache/decay_cache.hh"

namespace gem5
{

DecayCache::DecayCache(const DecayCacheParams &p)
    : Cache(p),
      tickPeriods(p.tick_periods)
{
}

void
DecayCache::startup()
{
    // schedule each tick
    for (uint8_t index = 0; index < tickPeriods.size(); ++index)
    {
        Cycles period = tickPeriods[index];
        DPRINTF(DecayCache, "Sechduling tick_period=%u\n", period);
        schedule(
            new EventFunctionWrapper([this, index] { processGlobalTick(index); }, name() + ".deadGlobalTick"),
            cyclesToTicks(Cycles(period))
        );
    }
}

bool
DecayCache::access(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
                         PacketList &writebacks)
{
    bool success = Cache::access(pkt, blk, lat, writebacks);
    if (!success)
        return false;

    assert(blk);

    auto search = blkStates.find(blk); 
    if (search == blkStates.end())
    {
        // first access
        DPRINTF(DecayCache, "init BlockState for %x\n", pkt->getBlockAddr(blkSize));
        blkStates[blk] = BlockState();
        return true;
    }

    // block already exists
    BlockState *p = &search->second;

    if (!p->alive)
    {
        DPRINTF(DecayCache, "Accessing dead block at %x\n", pkt->getBlockAddr(blkSize));
        if (p->counter == 3)
        {
            DPRINTF(DecayCache, "Counter == 3, increasing tickIndex\n");
            p->tickIndex = std::min<std::size_t>(tickPeriods.size() - 1, p->tickIndex + 1);
        }
        else if (p->counter == 0)
        {
            DPRINTF(DecayCache, "Counter == 0, decreasing tickIndex\n");
            p->tickIndex = std::max(0, p->tickIndex - 1);
        }

        p->alive = true;
    }

    p->counter = 0;

    return true;
}

void
DecayCache::processGlobalTick(uint8_t index)
{
    DPRINTF(DecayCache, "Proccessing Global Tick for %u\n", tickPeriods[index]);
    unsigned i = 1;

    // schedule cascading counter updates
    tags->forEachBlk(
        [this, index, &i](CacheBlk &blk) {
            BlockState state = blkStates[&blk];

            // only update counter if this global tick is at the rate of the block
            if (state.tickIndex != index)
                return;

            schedule(
                new EventFunctionWrapper([this, &blk] { updateCounter(&blk); }, name() + ".updateCounter"),
                clockEdge(Cycles(3 * i))
            );
            ++i;
        }
    );

    // schedule the next tick
    schedule(
        new EventFunctionWrapper(
            [this, index] { processGlobalTick(index); },
            name() + ".globalTick"
        ),
        cyclesToTicks(curCycle() + Cycles(tickPeriods[index]))
    );
}

void
DecayCache::updateCounter(CacheBlk *blk)
{
    BlockState *p = &blkStates[blk];

    if (p->alive && p->counter == 3)
    {
        p->counter = 0;
        p->alive = false;

        if (blk->isValid())
        {
            PacketPtr pkt = Cache::evictBlock(blk);
            if (pkt)
            {
                PacketList writebacks;
                DPRINTF(DecayCache, "Tick %llu: Decaying alive block at %x\n", curTick(), pkt->getBlockAddr(blkSize));
                decays++;
                writebacks.push_back(pkt);
                doWritebacks(writebacks, clockEdge(Cycles(0)));
            }
        }
    }

    DPRINTF(DecayCache, "Increasing counter\n");
    p->counter = std::min(3, p->counter + 1);
}

void
DecayCache::regStats()
{
    Cache::regStats();

    decays.name(name() + ".decays")
        .desc("Number of decays")
        ;
}

} // namespace gem5
