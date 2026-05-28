// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "tracing.hpp"
#include "execution_state.hpp"
#include "instructions_traits.hpp"
#include <evmc/hex.hpp>
#include <fstream>
#include <stack>

namespace evmone
{
namespace
{
std::string get_name(uint8_t opcode)
{
    // TODO: Create constexpr tables of names (maybe even per revision).
    const auto name = instr::traits[opcode].name;
    return (name != nullptr) ? name : "0x" + evmc::hex(opcode);
}

/// @see create_histogram_tracer()
class HistogramTracer : public Tracer
{
    struct Context
    {
        const int32_t depth;
        const uint8_t* const code;
        uint32_t counts[256]{};

        Context(int32_t _depth, const uint8_t* _code) noexcept : depth{_depth}, code{_code} {}
    };

    std::stack<Context> m_contexts;
    std::ostream& m_out;

    void on_execution_start(
        evmc_revision /*rev*/, const evmc_message& msg, bytes_view code) noexcept override
    {
        m_contexts.emplace(msg.depth, code.data());
    }

    void on_instruction_start(uint32_t pc, const intx::uint256* /*stack_top*/, int /*stack_height*/,
        int64_t /*gas*/, const ExecutionState& /*state*/) noexcept override
    {
        auto& ctx = m_contexts.top();
        ++ctx.counts[ctx.code[pc]];
    }

    void on_execution_end(const evmc_result& /*result*/) noexcept override
    {
        const auto& ctx = m_contexts.top();

        m_out << "--- # HISTOGRAM depth=" << ctx.depth << "\nopcode,count\n";
        for (size_t i = 0; i < std::size(ctx.counts); ++i)
        {
            if (ctx.counts[i] != 0)
                m_out << get_name(static_cast<uint8_t>(i)) << ',' << ctx.counts[i] << '\n';
        }

        m_contexts.pop();
    }

public:
    explicit HistogramTracer(std::ostream& out) noexcept : m_out{out} {}
};

class InstructionCounter : public Tracer
{
    std::stack<bytes_view> m_codes;
    std::array<unsigned, 256> m_counters{};
    std::string m_out_file_path;

    void on_execution_start(
        evmc_revision /*rev*/, const evmc_message& /*msg*/, bytes_view code) noexcept override
    {
        m_codes.emplace(code);
    }

    void on_instruction_start(uint32_t pc, const intx::uint256* /*stack_top*/, int /*stack_height*/,
        int64_t /*gas*/, const ExecutionState& /*state*/) noexcept override
    {
        const auto& code = m_codes.top();
        m_counters[code[pc]]++;
    }

    void on_execution_end(const evmc_result& /*result*/) noexcept override { m_codes.pop(); }

public:
    explicit InstructionCounter(std::string_view out_file_path) noexcept
      : m_out_file_path{out_file_path}
    {}

    ~InstructionCounter() noexcept override
    {
        std::ofstream out{m_out_file_path};
        out << "{\n";
        bool first = true;
        for (size_t i = 0; i < std::size(m_counters); ++i)
        {
            if (m_counters[i] == 0)
                continue;
            if (first)
                first = false;
            else
                out << ",\n";
            out << "  \"" << get_name(static_cast<uint8_t>(i)) << "\": " << m_counters[i];
        }
        out << "\n}\n";
    }
};


class InstructionTracer : public Tracer
{
    struct Context
    {
        const int32_t depth;
        const uint8_t* const code;  ///< Reference to the code being executed.
        const int64_t start_gas;

        Context(int32_t d, const uint8_t* c, int64_t g) noexcept : depth{d}, code{c}, start_gas{g}
        {}
    };

    std::stack<Context> m_contexts;
    std::ostream& m_out;  ///< Output stream.

    void output_stack(const intx::uint256* stack_top, int stack_height)
    {
        m_out << R"(,"stack":[)";
        const auto stack_end = stack_top + 1;
        const auto stack_begin = stack_end - stack_height;
        for (auto it = stack_begin; it != stack_end; ++it)
        {
            if (it != stack_begin)
                m_out << ',';
            m_out << R"("0x)" << to_string(*it, 16) << '"';
        }
        m_out << ']';
    }

    void on_execution_start(
        evmc_revision /*rev*/, const evmc_message& msg, bytes_view code) noexcept override
    {
        m_contexts.emplace(msg.depth, code.data(), msg.gas);
    }

    void on_instruction_start(uint32_t pc, const intx::uint256* stack_top, int stack_height,
        int64_t gas, const ExecutionState& state) noexcept override
    {
        const auto& ctx = m_contexts.top();

        const auto opcode = ctx.code[pc];
        m_out << "{";
        m_out << R"("pc":)" << std::dec << pc;
        m_out << R"(,"op":)" << std::dec << int{opcode};
        m_out << R"(,"gas":"0x)" << std::hex << gas << '"';
        m_out << R"(,"gasCost":"0x)" << std::hex << instr::gas_costs[state.rev][opcode] << '"';

        // Full memory can be dumped as evmc::hex({state.memory.data(), state.memory.size()}),
        // but this should not be done by default. Adding --tracing=+memory option would be nice.
        m_out << R"(,"memSize":)" << std::dec << state.memory.size();

        output_stack(stack_top, stack_height);
        if (!state.return_data.empty())
            m_out << R"(,"returnData":"0x)" << evmc::hex(state.return_data) << '"';
        m_out << R"(,"depth":)" << std::dec << (ctx.depth + 1);
        m_out << R"(,"refund":)" << std::dec << state.gas_refund;
        m_out << R"(,"opName":")" << get_name(opcode) << '"';

        m_out << "}\n";
    }

    void on_execution_end(const evmc_result& /*result*/) noexcept override { m_contexts.pop(); }

public:
    explicit InstructionTracer(std::ostream& out) noexcept : m_out{out}
    {
        m_out << std::dec;  // Set number formatting to dec, JSON does not support other forms.
    }
};
}  // namespace

std::unique_ptr<Tracer> create_histogram_tracer(std::ostream& out)
{
    return std::make_unique<HistogramTracer>(out);
}

std::unique_ptr<Tracer> create_instruction_counter(std::string_view out_file_path)
{
    return std::make_unique<InstructionCounter>(out_file_path);
}

std::unique_ptr<Tracer> create_instruction_tracer(std::ostream& out)
{
    return std::make_unique<InstructionTracer>(out);
}
}  // namespace evmone
