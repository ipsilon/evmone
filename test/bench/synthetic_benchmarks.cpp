// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "synthetic_benchmarks.hpp"
#include "helpers.hpp"
#include "test/utils/bytecode.hpp"
#include <evmone/instructions_traits.hpp>

using namespace benchmark;

namespace evmone::test
{
namespace
{
/// Stack limit inside the EVM benchmark loop (one stack item is used for the loop counter).
constexpr auto stack_limit = 1023;

enum class Mode
{
    min_stack = 0,   ///< The code uses as minimal stack as possible.
    full_stack = 1,  ///< The code fills the stack up to its limit.
};

/// The instruction grouping by EVM stack requirements.
enum class InstructionCategory : char
{
    nop = 'n',     ///< No-op instruction.
    nullop = 'a',  ///< Nullary operator - produces a result without any stack input.
    unop = 'u',    ///< Unary operator.
    binop = 'b',   ///< Binary operator.
    push = 'p',    ///< PUSH instruction.
    dup = 'd',     ///< DUP instruction.
    swap = 's',    ///< SWAP instruction.
    other = 'X',   ///< Not any of the categories above.
};

constexpr InstructionCategory get_instruction_category(Opcode opcode) noexcept
{
    const auto trait = instr::traits[opcode];
    if (opcode >= OP_PUSH1 && opcode <= OP_PUSH32)
        return InstructionCategory::push;
    else if (opcode >= OP_SWAP1 && opcode <= OP_SWAP16)
        return InstructionCategory::swap;
    else if (opcode >= OP_DUP1 && opcode <= OP_DUP16)
        return InstructionCategory::dup;
    else if (trait.stack_height_required == 0 && trait.stack_height_change == 0)
        return InstructionCategory::nop;
    else if (trait.stack_height_required == 0 && trait.stack_height_change == 1)
        return InstructionCategory::nullop;
    else if (trait.stack_height_required == 1 && trait.stack_height_change == 0)
        return InstructionCategory::unop;
    else if (trait.stack_height_required == 2 && trait.stack_height_change == -1)
        return InstructionCategory::binop;
    else
        return InstructionCategory::other;
}

struct CodeParams
{
    Opcode opcode;
    Mode mode;

    friend auto operator<=>(const CodeParams&, const CodeParams&) = default;
};

std::string to_string(const CodeParams& params)
{
    return std::string{instr::traits[params.opcode].name} + '/' +
           static_cast<char>(get_instruction_category(params.opcode)) +
           std::to_string(static_cast<int>(params.mode));
}

/// Generates the EVM benchmark loop inner code for the given opcode and "mode".
bytecode generate_loop_inner_code(CodeParams params)
{
    const auto [opcode, mode] = params;
    const auto category = get_instruction_category(opcode);
    switch (mode)
    {
    case Mode::min_stack:
        switch (category)
        {
        case InstructionCategory::nop:
            // JUMPDEST JUMPDEST ...
            return stack_limit * 2 * bytecode{opcode};

        case InstructionCategory::nullop:
            // CALLER POP CALLER POP ...
            return stack_limit * (bytecode{opcode} + OP_POP);

        case InstructionCategory::unop:
            // DUP1 NOT NOT ... POP
            return OP_DUP1 + stack_limit * 2 * bytecode{opcode} + OP_POP;

        case InstructionCategory::binop:
            // DUP1 DUP1 ADD DUP1 ADD DUP1 ADD ... POP
            return OP_DUP1 + (stack_limit - 1) * (OP_DUP1 + bytecode{opcode}) + OP_POP;

        case InstructionCategory::push:
            // PUSH1 POP PUSH1 POP ...
            return stack_limit * (push(opcode, {}) + OP_POP);

        case InstructionCategory::dup:
        {
            // The required n stack height for DUPn is provided by
            // duplicating the loop counter n-1 times with DUP1.
            const auto n = opcode - OP_DUP1 + 1;
            // DUP1 ...  DUPn POP DUPn POP ...  POP ...
            // \ n-1  /                         \ n-1 /
            return (n - 1) * OP_DUP1 +                // Required n stack height.
                   (stack_limit - (n - 1)) *          //
                       (bytecode{opcode} + OP_POP) +  // Multiple DUPn POP pairs.
                   (n - 1) * OP_POP;                  // Pop initially duplicated values.
        }

        case InstructionCategory::swap:
        {
            // The required n+1 stack height for SWAPn is provided by duplicating the loop counter
            // n times with DUP1. This also guarantees the loop counter remains unchanged because
            // it is always going to be swapped to the same value.
            const auto n = opcode - OP_SWAP1 + 1;
            // DUP1 ...  SWAPn SWAPn ...  POP ...
            // \  n   /                   \  n  /
            return n * OP_DUP1 +                         // Required n+1 stack height.
                   stack_limit * 2 * bytecode{opcode} +  // Multiple SWAPns.
                   n * OP_POP;                           // Pop initially duplicated values.
        }

        default:
            break;
        }
        break;

    case Mode::full_stack:
        switch (category)
        {
        case InstructionCategory::nullop:
            // CALLER CALLER ... POP POP ...
            return stack_limit * opcode + stack_limit * OP_POP;

        case InstructionCategory::binop:
            // DUP1 DUP1 DUP1 ... ADD ADD ADD ... POP
            return stack_limit * OP_DUP1 + (stack_limit - 1) * opcode + OP_POP;

        case InstructionCategory::push:
            // PUSH1 PUSH1 PUSH1 ... POP POP POP ...
            return stack_limit * push(opcode, {}) + stack_limit * OP_POP;

        case InstructionCategory::dup:
        {
            // The required initial n stack height for DUPn is provided by
            // duplicating the loop counter n-1 times with DUP1.
            const auto n = opcode - OP_DUP1 + 1;
            // DUP1 ...  DUPn DUPn ...  POP POP ...
            // \ n-1  /  \  S-(n-1)  /  \    S    /
            return (n - 1) * OP_DUP1 +                           // Required n stack height.
                   (stack_limit - (n - 1)) * bytecode{opcode} +  // Fill the stack with DUPn.
                   stack_limit * OP_POP;                         // Clear whole stack.
        }

        default:
            break;
        }
        break;
    }

    return {};
}

/// Generates a benchmark loop with given inner code.
///
/// This generates do-while loop with 255 iterations and it starts with PUSH1 of 255 as the loop
/// counter. The while check is done as `(counter += -1) != 0`. The SUB is avoided because it
/// consumes arguments in unnatural order and additional SWAP would be required.
///
/// The loop counter stays on the stack top. The inner code is allowed to duplicate it, but must not
/// modify it.
bytecode generate_loop_v1(const bytecode& inner_code)
{
    const auto counter = push(255);
    const auto jumpdest_offset = counter.size();
    return counter + OP_JUMPDEST + inner_code +  // loop label + inner code
           push("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") +  // -1
           OP_ADD + OP_DUP1 +                 // counter += (-1)
           push(jumpdest_offset) + OP_JUMPI;  // jump to jumpdest_offset if counter != 0
}

/// Generates a benchmark loop with given inner code.
///
/// This is improved variant of v1. It has exactly the same instructions and consumes the same
/// amount of gas, but according to performed benchmarks (see "loop_v1" and "loop_v2") it runs
/// faster. And we want the lowest possible loop overhead.
/// The change is to set the loop counter to -255 and check `(counter += 1) != 0`.
bytecode generate_loop_v2(const bytecode& inner_code)
{
    const auto counter =
        push("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff01");  // -255
    const auto jumpdest_offset = counter.size();
    return counter + OP_JUMPDEST + inner_code +  // loop label + inner code
           push(1) + OP_ADD + OP_DUP1 +          // counter += 1
           push(jumpdest_offset) + OP_JUMPI;     // jump to jumpdest_offset if counter != 0
}

bytes_view generate_code(CodeParams params)
{
    static std::map<CodeParams, bytecode> cache;

    auto& code = cache[params];
    if (!code.empty())
        return code;

    code = generate_loop_v2(generate_loop_inner_code(params));  // Cache it.
    return code;
}
}  // namespace

void register_synthetic_benchmarks()
{
    std::vector<CodeParams> params_list;

    // Nops & unops.
    for (const auto opcode : {OP_JUMPDEST, OP_ISZERO, OP_NOT})
        params_list.push_back({opcode, Mode::min_stack});

    // Binops.
    for (const auto opcode : {OP_ADD, OP_MUL, OP_SUB, OP_SIGNEXTEND, OP_LT, OP_GT, OP_SLT, OP_SGT,
             OP_EQ, OP_AND, OP_OR, OP_XOR, OP_BYTE, OP_SHL, OP_SHR, OP_SAR})
        params_list.insert(
            params_list.end(), {{opcode, Mode::min_stack}, {opcode, Mode::full_stack}});

    // Nullops.
    for (const auto opcode : {OP_ADDRESS, OP_CALLER, OP_CALLVALUE, OP_CALLDATASIZE, OP_CODESIZE,
             OP_RETURNDATASIZE, OP_PC, OP_MSIZE, OP_GAS})
        params_list.insert(
            params_list.end(), {{opcode, Mode::min_stack}, {opcode, Mode::full_stack}});

    // PUSH.
    for (auto opcode = OP_PUSH1; opcode <= OP_PUSH32; opcode = static_cast<Opcode>(opcode + 1))
        params_list.insert(
            params_list.end(), {{opcode, Mode::min_stack}, {opcode, Mode::full_stack}});

    // SWAP.
    for (auto opcode = OP_SWAP1; opcode <= OP_SWAP16; opcode = static_cast<Opcode>(opcode + 1))
        params_list.insert(params_list.end(), {{opcode, Mode::min_stack}});

    // DUP.
    for (auto opcode = OP_DUP1; opcode <= OP_DUP16; opcode = static_cast<Opcode>(opcode + 1))
        params_list.insert(
            params_list.end(), {{opcode, Mode::min_stack}, {opcode, Mode::full_stack}});


    for (auto& [vm_name, vm] : registered_vms)
    {
        RegisterBenchmark(bench_name(std::string{vm_name} + "/total/synth/loop_v1"),
            [&vm](State& state) { bench_evmc_execute(state, vm, generate_loop_v1({})); });
        RegisterBenchmark(bench_name(std::string{vm_name} + "/total/synth/loop_v2"),
            [&vm](State& state) { bench_evmc_execute(state, vm, generate_loop_v2({})); });
    }

    for (const auto params : params_list)
    {
        for (auto& [vm_name, vm] : registered_vms)
        {
            RegisterBenchmark(
                bench_name(std::string{vm_name} + "/total/synth/" + to_string(params)),
                [&vm, params](
                    State& state) { bench_evmc_execute(state, vm, generate_code(params)); })
                ->Unit(kMicrosecond);
        }
    }
}
}  // namespace evmone::test
