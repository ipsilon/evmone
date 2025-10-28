// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "test/utils/bytecode.hpp"
#include <evmc/evmc.hpp>
#include <evmc/mocked_host.hpp>
#include <evmone/evmone.h>
#include <evmone/instructions_traits.hpp>
#include <evmone/tracing.hpp>
#include <evmone/vm.hpp>
#include <gmock/gmock.h>

using namespace testing;
using namespace evmone::test;

class tracing : public Test
{
private:
    evmc::VM m_baseline_vm;

protected:
    evmone::VM& vm;
    evmc::MockedHost host;

    std::ostringstream trace_stream;

    tracing()
      : m_baseline_vm{evmc_create_evmone()},
        vm{*static_cast<evmone::VM*>(m_baseline_vm.get_raw_pointer())}
    {}

    std::string trace(
        bytes_view code, int32_t depth = 0, uint32_t flags = 0, evmc_revision rev = EVMC_BERLIN)
    {
        evmc_message msg{};
        msg.depth = depth;
        msg.flags = flags;
        msg.gas = 1000000;
        m_baseline_vm.execute(host, rev, msg, code.data(), code.size());
        auto result = trace_stream.str();
        trace_stream.str({});
        return result;
    }

    class OpcodeTracer final : public evmone::Tracer
    {
        std::string m_name;
        std::ostringstream& m_trace;
        bytes_view m_code;

        void on_execution_start(
            evmc_revision /*rev*/, const evmc_message& /*msg*/, bytes_view code) noexcept override
        {
            m_code = code;
        }

        void on_execution_end(const evmc_result& /*result*/) noexcept override { m_code = {}; }

        void on_instruction_start(uint32_t pc, const intx::uint256* /*stack_top*/,
            int /*stack_height*/, int64_t /*gas*/,
            const evmone::ExecutionState& /*state*/) noexcept override
        {
            const auto opcode = m_code[pc];
            m_trace << m_name << pc << ":" << evmone::instr::traits[opcode].name << " ";
        }

    public:
        explicit OpcodeTracer(tracing& parent, std::string name) noexcept
          : m_name{std::move(name)}, m_trace{parent.trace_stream}
        {}
    };

    class Inspector final : public evmone::Tracer
    {
        bytes m_last_code;

        void on_execution_start(
            evmc_revision /*rev*/, const evmc_message& /*msg*/, bytes_view code) noexcept override
        {
            m_last_code = code;
        }

        void on_execution_end(const evmc_result& /*result*/) noexcept override {}

        void on_instruction_start(uint32_t /*pc*/, const intx::uint256* /*stack_top*/,
            int /*stack_height*/, int64_t /*gas*/,
            const evmone::ExecutionState& /*state*/) noexcept override
        {}

    public:
        explicit Inspector() noexcept = default;

        [[nodiscard]] const bytes& get_last_code() const noexcept { return m_last_code; }
    };
};


TEST_F(tracing, no_tracer)
{
    EXPECT_EQ(vm.get_tracer(), nullptr);
}

TEST_F(tracing, one_tracer)
{
    vm.add_tracer(std::make_unique<OpcodeTracer>(*this, ""));

    EXPECT_EQ(trace(add(1, 2)), "0:PUSH1 2:PUSH1 4:ADD ");
}

TEST_F(tracing, two_tracers)
{
    vm.add_tracer(std::make_unique<OpcodeTracer>(*this, "A"));
    vm.add_tracer(std::make_unique<OpcodeTracer>(*this, "B"));

    EXPECT_EQ(trace(add(1, 2)), "A0:PUSH1 B0:PUSH1 A2:PUSH1 B2:PUSH1 A4:ADD B4:ADD ");
}

TEST_F(tracing, three_tracers)
{
    vm.add_tracer(std::make_unique<OpcodeTracer>(*this, "A"));
    vm.add_tracer(std::make_unique<OpcodeTracer>(*this, "B"));
    vm.add_tracer(std::make_unique<OpcodeTracer>(*this, "C"));

    EXPECT_EQ(trace(dup1(0)), "A0:PUSH1 B0:PUSH1 C0:PUSH1 A2:DUP1 B2:DUP1 C2:DUP1 ");
}

TEST_F(tracing, tracer_removed)
{
    vm.add_tracer(std::make_unique<OpcodeTracer>(*this, ""));
    vm.remove_tracers();

    EXPECT_EQ(vm.get_tracer(), nullptr);
}

TEST_F(tracing, tracers_removed)
{
    vm.add_tracer(std::make_unique<OpcodeTracer>(*this, "A"));
    vm.add_tracer(std::make_unique<OpcodeTracer>(*this, "B"));
    vm.remove_tracers();

    EXPECT_EQ(vm.get_tracer(), nullptr);
}

TEST_F(tracing, histogram)
{
    vm.add_tracer(evmone::create_histogram_tracer(trace_stream));

    trace_stream << '\n';
    EXPECT_EQ(trace(add(0, 0)), R"(
--- # HISTOGRAM depth=0
opcode,count
ADD,1
PUSH1,2
)");
}

TEST_F(tracing, histogram_undefined_instruction)
{
    vm.add_tracer(evmone::create_histogram_tracer(trace_stream));

    trace_stream << '\n';
    EXPECT_EQ(trace(bytecode{"EF"}), R"(
--- # HISTOGRAM depth=0
opcode,count
0xef,1
)");
}

TEST_F(tracing, histogram_internal_call)
{
    vm.add_tracer(evmone::create_histogram_tracer(trace_stream));
    trace_stream << '\n';
    EXPECT_EQ(trace(push(0) + OP_DUP1 + OP_SWAP1 + OP_POP + OP_POP, 1), R"(
--- # HISTOGRAM depth=1
opcode,count
POP,2
PUSH1,1
DUP1,1
SWAP1,1
)");
}

TEST_F(tracing, trace)
{
    vm.add_tracer(evmone::create_instruction_tracer(trace_stream));

    trace_stream << '\n';
    EXPECT_EQ(trace(add(2, 3)), R"(
{"pc":0,"op":96,"gas":"0xf4240","gasCost":"0x3","memSize":0,"stack":[],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":2,"op":96,"gas":"0xf423d","gasCost":"0x3","memSize":0,"stack":["0x3"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":4,"op":1,"gas":"0xf423a","gasCost":"0x3","memSize":0,"stack":["0x3","0x2"],"depth":1,"refund":0,"opName":"ADD"}
)");
}

TEST_F(tracing, trace_stack)
{
    vm.add_tracer(evmone::create_instruction_tracer(trace_stream));

    const auto code = push(1) + push(2) + push(3) + push(4) + OP_ADD + OP_ADD + OP_ADD;
    trace_stream << '\n';
    EXPECT_EQ(trace(code), R"(
{"pc":0,"op":96,"gas":"0xf4240","gasCost":"0x3","memSize":0,"stack":[],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":2,"op":96,"gas":"0xf423d","gasCost":"0x3","memSize":0,"stack":["0x1"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":4,"op":96,"gas":"0xf423a","gasCost":"0x3","memSize":0,"stack":["0x1","0x2"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":6,"op":96,"gas":"0xf4237","gasCost":"0x3","memSize":0,"stack":["0x1","0x2","0x3"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":8,"op":1,"gas":"0xf4234","gasCost":"0x3","memSize":0,"stack":["0x1","0x2","0x3","0x4"],"depth":1,"refund":0,"opName":"ADD"}
{"pc":9,"op":1,"gas":"0xf4231","gasCost":"0x3","memSize":0,"stack":["0x1","0x2","0x7"],"depth":1,"refund":0,"opName":"ADD"}
{"pc":10,"op":1,"gas":"0xf422e","gasCost":"0x3","memSize":0,"stack":["0x1","0x9"],"depth":1,"refund":0,"opName":"ADD"}
)");
}

TEST_F(tracing, trace_error)
{
    vm.add_tracer(evmone::create_instruction_tracer(trace_stream));

    const auto code = bytecode{OP_POP};
    trace_stream << '\n';
    EXPECT_EQ(trace(code), R"(
{"pc":0,"op":80,"gas":"0xf4240","gasCost":"0x2","memSize":0,"stack":[],"depth":1,"refund":0,"opName":"POP"}
)");
}

TEST_F(tracing, trace_output)
{
    vm.add_tracer(evmone::create_instruction_tracer(trace_stream));

    const auto code = push(0xabcdef) + ret_top();
    trace_stream << '\n';
    EXPECT_EQ(trace(code), R"(
{"pc":0,"op":98,"gas":"0xf4240","gasCost":"0x3","memSize":0,"stack":[],"depth":1,"refund":0,"opName":"PUSH3"}
{"pc":4,"op":96,"gas":"0xf423d","gasCost":"0x3","memSize":0,"stack":["0xabcdef"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":6,"op":82,"gas":"0xf423a","gasCost":"0x3","memSize":0,"stack":["0xabcdef","0x0"],"depth":1,"refund":0,"opName":"MSTORE"}
{"pc":7,"op":96,"gas":"0xf4234","gasCost":"0x3","memSize":32,"stack":[],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":9,"op":96,"gas":"0xf4231","gasCost":"0x3","memSize":32,"stack":["0x20"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":11,"op":243,"gas":"0xf422e","gasCost":"0x0","memSize":32,"stack":["0x20","0x0"],"depth":1,"refund":0,"opName":"RETURN"}
)");
}

TEST_F(tracing, trace_revert)
{
    vm.add_tracer(evmone::create_instruction_tracer(trace_stream));

    const auto code = mstore(0, 0x0e4404) + push(3) + push(29) + OP_REVERT;
    trace_stream << '\n';
    EXPECT_EQ(trace(code), R"(
{"pc":0,"op":98,"gas":"0xf4240","gasCost":"0x3","memSize":0,"stack":[],"depth":1,"refund":0,"opName":"PUSH3"}
{"pc":4,"op":96,"gas":"0xf423d","gasCost":"0x3","memSize":0,"stack":["0xe4404"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":6,"op":82,"gas":"0xf423a","gasCost":"0x3","memSize":0,"stack":["0xe4404","0x0"],"depth":1,"refund":0,"opName":"MSTORE"}
{"pc":7,"op":96,"gas":"0xf4234","gasCost":"0x3","memSize":32,"stack":[],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":9,"op":96,"gas":"0xf4231","gasCost":"0x3","memSize":32,"stack":["0x3"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":11,"op":253,"gas":"0xf422e","gasCost":"0x0","memSize":32,"stack":["0x3","0x1d"],"depth":1,"refund":0,"opName":"REVERT"}
)");
}

// TEST_F(tracing, trace_create)
//{
//     vm.add_tracer(evmone::create_instruction_tracer(trace_stream));
//
//     trace_stream << '\n';
//     EXPECT_EQ(trace({}, 2), R"(
//{"depth":2,"rev":"Berlin","static":false}
//{"error":null,"gas":0xf4240,"gasUsed":0x0,"output":""}
//)");
// }
//
// TEST_F(tracing, trace_static)
//{
//     vm.add_tracer(evmone::create_instruction_tracer(trace_stream));
//
//     trace_stream << '\n';
//     EXPECT_EQ(trace({}, 2, EVMC_STATIC), R"(
//{"depth":2,"rev":"Berlin","static":true}
//{"error":null,"gas":0xf4240,"gasUsed":0x0,"output":""}
//)");
// }

TEST_F(tracing, trace_undefined_instruction)
{
    vm.add_tracer(evmone::create_instruction_tracer(trace_stream));

    const auto code = bytecode{} + OP_JUMPDEST + "EF";
    trace_stream << '\n';
    EXPECT_EQ(trace(code), R"(
{"pc":0,"op":91,"gas":"0xf4240","gasCost":"0x1","memSize":0,"stack":[],"depth":1,"refund":0,"opName":"JUMPDEST"}
{"pc":1,"op":239,"gas":"0xf423f","gasCost":"0xffff","memSize":0,"stack":[],"depth":1,"refund":0,"opName":"0xef"}
)");
}

TEST_F(tracing, trace_code_containing_zero)
{
    auto tracer_ptr = std::make_unique<Inspector>();
    const auto& tracer = *tracer_ptr;
    vm.add_tracer(std::move(tracer_ptr));

    const auto code = bytecode{} + "602a6000556101c960015560068060166000396000f3600035600055";

    trace(code);

    EXPECT_EQ(tracer.get_last_code().size(), code.size());
}

TEST_F(tracing, trace_create_instruction)
{
    using namespace evmc::literals;

    vm.add_tracer(evmone::create_instruction_tracer(trace_stream));

    trace_stream << '\n';

    const auto code = push(10) + push(0) + push(0) + OP_CREATE + ret_top();

    const auto result_data = "0x60016000526001601ff3"_hex;
    host.call_result.create_address = 0x1122334455667788991011223344556677889910_address;
    host.call_result.output_data = result_data.data();
    host.call_result.output_size = result_data.size();

    EXPECT_EQ(trace(code, 0, 0, EVMC_BERLIN), R"(
{"pc":0,"op":96,"gas":"0xf4240","gasCost":"0x3","memSize":0,"stack":[],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":2,"op":96,"gas":"0xf423d","gasCost":"0x3","memSize":0,"stack":["0xa"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":4,"op":96,"gas":"0xf423a","gasCost":"0x3","memSize":0,"stack":["0xa","0x0"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":6,"op":240,"gas":"0xf4237","gasCost":"0x7d00","memSize":0,"stack":["0xa","0x0","0x0"],"depth":1,"refund":0,"opName":"CREATE"}
{"pc":7,"op":96,"gas":"0x3b14","gasCost":"0x3","memSize":32,"stack":["0x1122334455667788991011223344556677889910"],"returnData":"0x60016000526001601ff3","depth":1,"refund":0,"opName":"PUSH1"}
{"pc":9,"op":82,"gas":"0x3b11","gasCost":"0x3","memSize":32,"stack":["0x1122334455667788991011223344556677889910","0x0"],"returnData":"0x60016000526001601ff3","depth":1,"refund":0,"opName":"MSTORE"}
{"pc":10,"op":96,"gas":"0x3b0e","gasCost":"0x3","memSize":32,"stack":[],"returnData":"0x60016000526001601ff3","depth":1,"refund":0,"opName":"PUSH1"}
{"pc":12,"op":96,"gas":"0x3b0b","gasCost":"0x3","memSize":32,"stack":["0x20"],"returnData":"0x60016000526001601ff3","depth":1,"refund":0,"opName":"PUSH1"}
{"pc":14,"op":243,"gas":"0x3b08","gasCost":"0x0","memSize":32,"stack":["0x20","0x0"],"returnData":"0x60016000526001601ff3","depth":1,"refund":0,"opName":"RETURN"}
)");
}
