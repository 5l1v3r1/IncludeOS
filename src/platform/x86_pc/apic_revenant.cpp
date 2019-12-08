#include "apic_revenant.hpp"
#include "apic.hpp"
#include "apic_timer.hpp"
#include "clocks.hpp"
#include "idt.hpp"
#include "init_libc.hpp"
#include <kernel/events.hpp>
#include <kernel/rng.hpp>
#include <kernel/threads.hpp>
#include <os.hpp>

namespace x86 {
  extern void initialize_cpu_tables_for_cpu(int);
  smp_stuff smp_main;
  std::vector<smp_system_stuff> smp_system;
}
#define INFO(FROM, TEXT, ...) printf("%13s ] " TEXT "\n", "[ " FROM, ##__VA_ARGS__)

using namespace x86;

static bool revenant_task_doer(smp_system_stuff& system)
{
  // grab hold on task list
  system.tlock.lock();

  if (system.tasks.empty()) {
    system.tlock.unlock();
    // try again
    return false;
  }

  // create local vector which holds tasks
  std::vector<smp_task> tasks;
  system.tasks.swap(tasks);

  system.tlock.unlock();

  for (auto& task : tasks)
  {
    // execute actual task
    task.func();

    // add done function to completed list (only if its callable)
    if (task.done)
    {
      // NOTE: specifically pushing to 'smp' here, and not 'system'
      PER_CPU(smp_system).flock.lock();
      PER_CPU(smp_system).completed.push_back(std::move(task.done));
      PER_CPU(smp_system).flock.unlock();
      // signal home
      PER_CPU(smp_system).work_done = true;
    }
  }
  return true;
}
static void revenant_task_handler()
{
  auto& system = PER_CPU(smp_system);
  system.work_done = false;
  // cpu-specific tasks
  while(revenant_task_doer(PER_CPU(smp_system)));
  // global tasks (by taking from index 0)
  while (revenant_task_doer(smp_system[0]));
  // if we did any work with done functions, signal back
  if (system.work_done) {
    // set bit for this CPU
    smp_main.bitmap.atomic_set(SMP::cpu_id());
    // signal main CPU
    x86::APIC::get().send_bsp_intr();
  }
}

void revenant_thread_main(int cpu)
{
	sched_yield();
	uintptr_t this_stack = smp_main.stack_base + cpu * smp_main.stack_size;

    // show we are online, and verify CPU ID is correct
    SMP::global_lock();
    INFO2("AP %d started at %p", SMP::cpu_id(), (void*) this_stack);
    SMP::global_unlock();
    Expects(cpu == SMP::cpu_id());

	auto& ev = Events::get(cpu);
	ev.init_local();
	// subscribe to task and timer interrupts
	ev.subscribe(0, revenant_task_handler);
	ev.subscribe(1, APIC_Timer::start_timers);
	// enable interrupts
	asm volatile("sti");
	// init timer system
	APIC_Timer::init();
	// initialize clocks
	Clocks::init();
	// seed RNG
	RNG::get().init();

	// allow programmers to do stuff on each core at init
    SMP::init_task();

    // signal that the revenant has started
    smp_main.boot_barrier.increment();

    SMP::global_lock();
    x86::smp_main.initialized_cpus.push_back(cpu);
    SMP::global_unlock();
    while (true)
    {
      Events::get().process_events();
      os::halt();
    }
    __builtin_unreachable();
}

void revenant_main(int cpu)
{
  // enable Local APIC
  x86::APIC::get().smp_enable();
  // setup GDT & per-cpu feature
  x86::initialize_cpu_tables_for_cpu(cpu);
  // initialize exceptions before asserts
  x86::idt_initialize_for_cpu(cpu);

#ifdef ARCH_x86_64
  // interrupt stack tables
  uintptr_t this_stack = smp_main.stack_base + cpu * smp_main.stack_size;
  ist_initialize_for_cpu(cpu, this_stack);

  const uint64_t star_kernel_cs = 8ull << 32;
  const uint64_t star_user_cs   = 8ull << 48;
  const uint64_t star = star_kernel_cs | star_user_cs;
  x86::CPU::write_msr(IA32_STAR, star);
  x86::CPU::write_msr(IA32_LSTAR, (uintptr_t)&__syscall_entry);
#endif

  auto& system = PER_CPU(smp_system);
  // setup main thread
  auto* kthread = kernel::setup_main_thread(system.main_thread_id);
  // resume APs main thread
  kthread->resume();
  __builtin_unreachable();
}
