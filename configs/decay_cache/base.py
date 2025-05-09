#!/usr/bin/env python3

from m5.objects import *

import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('binary', type=Path)
args = parser.parse_args()

# Create the system
system = System()
system.clk_domain = SrcClockDomain(clock='2GHz', voltage_domain=VoltageDomain())
system.mem_mode = 'timing'
system.mem_ranges = [AddrRange('512MiB')]
system.cache_line_size = 64

# Create a simple CPU (e.g., O3CPU for Alpha-like spec)
system.cpu = O3CPU()
system.cpu.numIQEntries = 80
system.cpu.numROBEntries = 80
system.cpu.LQEntries = 40
system.cpu.SQEntries = 40
system.cpu.issueWidth = 4
system.cpu.fetchWidth = 4
system.cpu.decodeWidth = 4
system.cpu.renameWidth = 4
system.cpu.dispatchWidth = 4
system.cpu.commitWidth = 4

class L1ICache(Cache):
    size = '32KiB'
    assoc = 1
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20
    is_read_only=True
    writeback_clean=True

class L1DCache(Cache):
    size = '32KiB'
    assoc = 1
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20
    writeback_clean=True

class L2Cache(Cache):
    size = '1MiB'
    assoc = 8
    tag_latency = 6
    data_latency = 6
    response_latency = 6
    mshrs = 20
    tgts_per_mshr = 12

# L1 instruction cache (standard)
system.cpu.icache = L1ICache()

# L1 data cache (standard)
system.cpu.dcache = L1DCache()

# Connect caches to CPU ports
system.cpu.icache_port = system.cpu.icache.cpu_side
system.cpu.dcache_port = system.cpu.dcache.cpu_side

# Bus to connect L1 and L2
system.l2bus = L2XBar()

# Connect caches to L2 bus
system.cpu.icache.mem_side = system.l2bus.cpu_side_ports
system.cpu.dcache.mem_side = system.l2bus.cpu_side_ports

# L2 cache
system.l2cache = L2Cache()
system.l2cache.cpu_side = system.l2bus.mem_side_ports

# Main memory
system.membus = SystemXBar()
system.l2cache.mem_side = system.membus.cpu_side_ports

# create the interrupt controller for the CPU and connect to the membus
system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.mem_side_ports
system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

# Memory controller
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports

# TLB
system.cpu.itb = X86TLB(entry_type='instruction', size=128)
system.cpu.dtb = X86TLB(entry_type='data', size=128)

# System port
system.system_port = system.membus.cpu_side_ports

# Create a process (barebones for now)
binary_str = str(args.binary)
system.workload = SEWorkload.init_compatible(binary_str)

process = Process()
process.cmd = [binary_str]
system.cpu.workload = process
system.cpu.createThreads()

# Instantiate and simulate
root = Root(full_system=False, system=system)
m5.instantiate()
print('Beginning simulation...')
exit_event = m5.simulate()
print(f'Exiting @ tick {m5.curTick()} because {exit_event.getCause()}')
