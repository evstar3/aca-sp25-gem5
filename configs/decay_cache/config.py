from m5.objects import *
from m5.objects import DecayCache

l1d = DecayCache(size="32kB", assoc=1, block_size=32)

# Create the system
system = System()
system.clk_domain = SrcClockDomain(clock="2GHz", voltage_domain=VoltageDomain())
system.mem_mode = 'timing'
system.mem_ranges = [AddrRange("512MB")]

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

# L1 instruction cache (standard)
system.cpu.icache = Cache(size='32kB', assoc=1, block_size=32, is_read_only=True, writeback_clean=True)

# L1 data cache (custom DecayCache)
system.cpu.dcache = DecayCache(size='32kB', assoc=1, block_size=32, writeback_clean=True)

# Connect caches to CPU ports
system.cpu.icache_port = system.cpu.icache.cpu_side
system.cpu.dcache_port = system.cpu.dcache.cpu_side

# Bus to connect L1 and L2
system.l2bus = L2XBar()

# Connect caches to L2 bus
system.cpu.icache.mem_side = system.l2bus.slave
system.cpu.dcache.mem_side = system.l2bus.slave

# L2 cache
system.l2cache = Cache(size='1MB', assoc=8, block_size=64, tag_latency=6, data_latency=6, response_latency=6)
system.l2cache.cpu_side = system.l2bus.master

# Main memory
system.membus = SystemXBar()
system.l2cache.mem_side = system.membus.slave

# Memory controller
system.mem_ctrl = DDR3_1600_8x8()
system.mem_ctrl.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.master

# TLB
system.cpu.itb = X86TLB(size=128)
system.cpu.dtb = X86TLB(size=128)

# System port
system.system_port = system.membus.slave

# Create a process (barebones for now)
process = Process()
process.cmd = ['tests/test-progs/hello/bin/x86/linux/hello'] # can be replaced with a different benchmark
system.cpu.workload = process
system.cpu.createThreads()

# Instantiate and simulate
root = Root(full_system=False, system=system)
m5.instantiate()
print("Beginning simulation...")
exit_event = m5.simulate()
print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
