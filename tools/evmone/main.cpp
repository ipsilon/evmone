// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <CLI/CLI.hpp>
#include <evmc/hex.hpp>
#include <evmone/evmone.h>
#include <test/utils/run.hpp>
#include <test/utils/t8n.hpp>
#include <test/utils/utils.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>

namespace fs = std::filesystem;

namespace
{
/// If the argument starts with @ returns the hex-decoded contents of the file
/// at the path following the @. Otherwise, returns the argument.
/// @todo The file content is expected to be a hex string but not validated.
evmc::bytes load_from_hex(const std::string& str)
{
    if (str.starts_with('@'))  // The argument is file path.
    {
        const auto path = str.substr(1);
        std::ifstream file{path};
        auto out = evmc::from_spaced_hex(
            std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
        if (!out)
            throw std::invalid_argument{"invalid hex in " + path};
        return out.value();
    }

    return evmc::from_hex(str).value();  // Should be validated already.
}

struct HexOrFileValidator : CLI::Validator
{
    HexOrFileValidator() : Validator{"HEX|@FILE"}
    {
        func_ = [](const std::string& str) -> std::string {
            if (str.starts_with('@'))
                return CLI::ExistingFile(str.substr(1));
            if (!evmc::validate_hex(str))
                return "invalid hex";
            return {};
        };
    }
};

/// CLI bindings for the `t8n` subcommand.
struct T8nOptions
{
    std::string state_fork;
    uint64_t state_chainid = 0;
    std::optional<int64_t> state_reward;
    fs::path alloc_file;
    fs::path env_file;
    fs::path txs_file;
    fs::path blob_params_file;
    fs::path output_dir;
    fs::path output_result_file;
    fs::path output_alloc_file;
    fs::path output_body_file;
    fs::path opcode_count_filename;
    bool trace = false;
};

const CLI::App& setup_t8n_cmd(CLI::App& app, T8nOptions& opts)
{
    auto& cmd = *app.add_subcommand("t8n", "Run Ethereum state transition (EELS t8n protocol)");
    cmd.add_option("--state.fork", opts.state_fork, "Active EVM revision")->required();
    cmd.add_option("--state.chainid", opts.state_chainid, "Chain ID (decimal or 0x-prefixed hex)");
    cmd.add_option(
           "--state.reward", opts.state_reward, "Block reward in wei (-1 to output pre-state only)")
        ->check(CLI::Range(int64_t{-1}, std::numeric_limits<int64_t>::max()));
    cmd.add_option("--input.alloc", opts.alloc_file, "Pre-state alloc JSON")
        ->check(CLI::ExistingFile);
    cmd.add_option("--input.env", opts.env_file, "Block env JSON")->check(CLI::ExistingFile);
    cmd.add_option("--input.txs", opts.txs_file, "Transactions JSON")->check(CLI::ExistingFile);
    cmd.add_option("--input.blobParams", opts.blob_params_file, "Blob schedule JSON")
        ->check(CLI::ExistingFile);
    cmd.add_option("--output.basedir", opts.output_dir, "Output base directory");
    cmd.add_option("--output.result", opts.output_result_file, "Result JSON filename");
    cmd.add_option("--output.alloc", opts.output_alloc_file, "Post-state alloc JSON filename");
    cmd.add_option("--output.body", opts.output_body_file, "RLP-encoded transactions filename");
    cmd.add_flag("--trace", opts.trace, "Write per-tx execution traces under --output.basedir");
    cmd.add_option(
        "--opcode.count", opts.opcode_count_filename, "Per-opcode count output filename");
    return cmd;
}

int exec_t8n_cmd(evmc::VM& vm, const T8nOptions& opts)
{
    evmone::tooling::T8NArgs args;
    args.rev = evmone::test::to_rev(opts.state_fork);
    args.chain_id = opts.state_chainid;
    if (opts.state_reward)
    {
        if (*opts.state_reward == -1)
            args.pre_state_only = true;
        else
            args.block_reward = static_cast<uint64_t>(*opts.state_reward);
    }

    if (!opts.output_dir.empty())
        fs::create_directories(opts.output_dir);
    if (!opts.opcode_count_filename.empty())
        args.opcode_count_file = (opts.output_dir / opts.opcode_count_filename).string();

    std::ifstream in_alloc;
    std::ifstream in_env;
    std::ifstream in_txs;
    std::ifstream in_blob_params;
    std::ofstream out_result;
    std::ofstream out_alloc;
    std::ofstream out_body;
    std::ofstream trace_file;

    const auto bind_stream = [](auto& s, const fs::path& p) -> decltype(&s) {
        if (p.empty())
            return nullptr;
        s.open(p);
        return &s;
    };
    const auto output_path = [&](const fs::path& name) -> fs::path {
        return name.empty() ? fs::path{} : opts.output_dir / name;
    };

    args.alloc = bind_stream(in_alloc, opts.alloc_file);
    args.env = bind_stream(in_env, opts.env_file);
    args.txs = bind_stream(in_txs, opts.txs_file);
    args.blob_params = bind_stream(in_blob_params, opts.blob_params_file);
    args.out_result = bind_stream(out_result, output_path(opts.output_result_file));
    args.out_alloc = bind_stream(out_alloc, output_path(opts.output_alloc_file));
    args.out_body = bind_stream(out_body, output_path(opts.output_body_file));

    if (opts.trace)
    {
        args.open_trace = [&](size_t tx_index, const evmc::bytes32& tx_hash) -> std::ostream& {
            trace_file =
                std::ofstream{opts.output_dir / ("trace-" + std::to_string(tx_index) + "-0x" +
                                                    evmc::hex(tx_hash) + ".jsonl")};
            return trace_file;
        };
    }

    evmone::tooling::t8n(vm, args);
    return 0;
}
}  // namespace

int main(int argc, const char* const* argv) noexcept
{
    using namespace evmc;

    try
    {
        const HexOrFileValidator HexOrFile;

        std::string code_arg;
        int64_t gas = 1000000;
        auto rev = EVMC_LATEST_STABLE_REVISION;
        std::string input_arg;
        auto create = false;
        auto bench = false;
        auto trace = false;
        auto histogram = false;

        VM vm{evmc_create_evmone()};

        CLI::App app{"evmone EVM tool"};
        app.set_version_flag(
            "--version", [&vm] { return std::string{vm.name()} + " " + vm.version(); });
        app.add_flag("--trace", trace, "Enable execution trace");
        app.add_flag("--histogram", histogram, "Enable opcode histogram");

        auto& run_cmd = *app.add_subcommand("run", "Execute EVM bytecode")->fallthrough();
        run_cmd.add_option("code", code_arg, "Bytecode")->required()->check(HexOrFile);
        run_cmd.add_option("--gas", gas, "Execution gas limit")
            ->capture_default_str()
            ->check(CLI::Range(0, 1000000000));
        run_cmd
            .add_option_function<std::string>(
                "--rev", [&rev](const std::string& name) { rev = evmone::test::to_rev(name); },
                "EVM revision name")
            ->default_str(evmc::to_string(rev));
        run_cmd.add_option("--input", input_arg, "Input bytes")->check(HexOrFile);
        run_cmd.add_flag("--create", create,
            "Create new contract out of the code and then execute this contract with the input");
        run_cmd.add_flag("--bench", bench,
            "Benchmark execution time (state modification may result in unexpected behaviour)");

        T8nOptions t8n_opts;
        const auto& t8n_cmd = setup_t8n_cmd(app, t8n_opts);

        try
        {
            app.parse(argc, argv);

            if (trace)
                vm.set_option("trace", "");
            if (histogram)
                vm.set_option("histogram", "");

            if (run_cmd)
            {
                // If code_arg or input_arg contains invalid hex string, an exception is thrown.
                const auto code = load_from_hex(code_arg);
                const auto input = load_from_hex(input_arg);
                return evmone::tooling::run(vm, rev, gas, code, input, create, bench, std::cout);
            }

            if (t8n_cmd)
                return exec_t8n_cmd(vm, t8n_opts);

            return 0;
        }
        catch (const CLI::ParseError& e)
        {
            return app.exit(e);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return -1;
    }
    catch (...)
    {
        return -2;
    }
}
