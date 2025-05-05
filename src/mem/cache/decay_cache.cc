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
    : NoncoherentCache(p)
{
}

bool
DecayCache::access(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
                         PacketList &writebacks)
{
    bool success = NoncoherentCache::access(pkt, blk, lat, writebacks);

    lastAccessStore[pkt->getBlockAddr(blkSize)] = curCycle();

    schedule(
        new EventFunctionWrapper(
            [this, pkt, blk] { decayTimeout(pkt, blk); },
            name() + ".decayTimeout"
        ),
        cyclesToTicks(curCycle() + Cycles(10000))
    );

    return success;
}

PacketPtr
DecayCache::decayTimeout(PacketPtr pkt, CacheBlk *blk)
{
    if (curCycle() - lastAccessStore[pkt->getBlockAddr(blkSize)] == 10000)
        return NoncoherentCache::evictBlock(blk);

    return nullptr;
}

} // namespace gem5
