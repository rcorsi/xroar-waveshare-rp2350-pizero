/** \file
 *
 *  \brief Where the time per emulated 6809 cycle goes (PIZERO-209).
 *
 *  Counted with the Cortex-M33's own cycle counter (DWT CYCCNT), which
 *  resolves single host clocks; micros() cannot see a 185-clock cycle.
 *  Only in the CPU_PROF builds (envs pizero_cpuprof, pizero_cpuprof_hook);
 *  every macro is empty otherwise, so the product core is unchanged.
 *
 *  CPU_PROF=1 times the bus path (interrupt latch stores, the call and
 *  coco_mem_cycle) as one window and counts cycles, dummy cycles and
 *  instructions. CPU_PROF=2 also splits the hook into its sections.
 */

#ifndef XROAR_CPU_PROF_H_
#define XROAR_CPU_PROF_H_

#include <stdint.h>

#ifdef CPU_PROF

// The ARMv8-M architectural addresses, not the SDK's m33_hw: its headers
// bring in stdbool.h, which breaks the core's token-pasted delegate names.
#define CPU_PROF_DWT_CTRL   (*(volatile uint32_t *)0xE0001000u)
#define CPU_PROF_DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)
#define CPU_PROF_DEMCR      (*(volatile uint32_t *)0xE000EDFCu)
#define CPU_PROF_DEMCR_TRCENA     (1u << 24)
#define CPU_PROF_DWT_CYCCNTENA    (1u << 0)

#ifdef __cplusplus
extern "C" {
#endif

struct cpu_prof {
	uint32_t run_clk;     // whole of the CPU run, per frame
	uint32_t bus_clk;     // latch stores + call + coco_mem_cycle
	uint32_t bus_n;       // bus cycles: one per emulated 6809 cycle
	uint32_t nvma_n;      // of which dummy cycles (address $FFFF)
	uint32_t insn_n;      // opcode fetches (a page prefix counts again)
	uint32_t sec_clk[4];  // CPU_PROF=2: decode, data, events+audio, irq+budget
};

extern struct cpu_prof g_cpu_prof;

static inline uint32_t cpu_prof_now(void) {
	return CPU_PROF_DWT_CYCCNT;
}

#ifdef __cplusplus
}
#endif

#define CPU_PROF_BUS_BEGIN() uint32_t cpu_prof_t0 = cpu_prof_now()
#define CPU_PROF_BUS_END(a) do { \
		g_cpu_prof.bus_clk += cpu_prof_now() - cpu_prof_t0; \
		g_cpu_prof.bus_n++; \
		if ((a) == 0xffff) g_cpu_prof.nvma_n++; \
	} while (0)
#define CPU_PROF_INSN() (g_cpu_prof.insn_n++)
// The timing makes the bus helpers too big for the compiler to inline, and
// out of line they land in flash: force them back so the layout matches the
// product build, where they are inlined into the RAM-resident run loop.
#define CPU_PROF_INLINE __attribute__((always_inline)) inline

#else

#define CPU_PROF_BUS_BEGIN() do {} while (0)
#define CPU_PROF_BUS_END(a) do {} while (0)
#define CPU_PROF_INSN() do {} while (0)
// PIZERO-131: the bus helpers now hold the RAM/ROM fast path, which made
// them too big for the compiler to inline, and out of line they landed in
// flash. Forced inline in every build.
#define CPU_PROF_INLINE __attribute__((always_inline)) inline

#endif

#endif
