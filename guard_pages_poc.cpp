

#include <sys/mman.h>
#include <unistd.h>
#include <cassert>
#include <csetjmp>
#include <csignal>
#include <iostream>

constexpr size_t stack_size = 32 * 1024;

const size_t page_size = []() noexcept { return static_cast<size_t>(sysconf(_SC_PAGESIZE)); }();

thread_local jmp_buf fault_jmp_buf;


static auto allocate_stack()
{
    assert(stack_size % page_size == 0);
    const auto num_pages = stack_size / page_size + 2;
    const auto alloc_size = num_pages * page_size;

    // The mem is zeroed what is undesired for the EVM stack.
    auto* mem = static_cast<char*>(
        mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    mprotect(mem, page_size, PROT_NONE);                           // Stack underflow guard page.
    mprotect(mem + stack_size + page_size, page_size, PROT_NONE);  // Stack overflow guard page.

    return mem;
}

static void handler(int sig, siginfo_t* siginfo, void*)
{
    const auto* fault_address = siginfo->si_addr;
    assert(sig == SIGSEGV);
    printf("SIGSEGV @ %p\n", fault_address);

    // TODO: Check if the fault address is from stack space. If not fallback to default
    //       signal handler with signal(SIGSEGV, SIG_DFL).

    // TODO: jmp may be not initialized yet.
    siglongjmp(fault_jmp_buf, 1);
}

/// Registers EVM stack overflow/underflow signal handler for SIGSEGV.
/// The SIGSEGV is delivered to the same thread where the fault happens.
static void register_page_fault_handler()
{
    struct sigaction sa = {};
    sa.sa_sigaction = &handler;
    sa.sa_flags = SA_SIGINFO;

    struct sigaction old = {};
    sigaction(SIGSEGV, &sa, &old);  // Register handler for SIGSEGV.

    assert(old.sa_flags == 0 && old.sa_handler == nullptr);
}

static bool execute()
{
    // TODO: To allow recursive execute(), the old fault_jmp_buf should be saved and restored later.

    static constexpr int save_proc_signal_mask = 1;  // Not sure this is wanted.
    if (sigsetjmp(fault_jmp_buf, save_proc_signal_mask) != 0)
    {
        std::cout << "STACK OVERFLOW/UNDERFLOW!\n";
        return false;
    }

    auto* b = allocate_stack();


    b[page_size] = 1;                   // Bottom of the stack space.
    b[stack_size + page_size - 1] = 1;  // Top of the stack space.

    // Stack underflow:
    // b[7] = 1;

    // Stack overflow:
    b[stack_size + page_size] = 1;

    return true;
}

int main()
{
    std::cout << "PAGESIZE: " << page_size << "\n";

    register_page_fault_handler();

    const auto status = execute();
    return status ? 0 : 1;
}
