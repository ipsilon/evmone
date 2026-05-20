// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/evmc.hpp>
#include <evmc/hex.hpp>
#include <evmone/version.h>
#include <intx/intx.hpp>
#include <test/utils/t8n.hpp>
#include <test/utils/utils.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace fs = std::filesystem;
using namespace evmone;
using namespace std::literals;

int main(int argc, const char* argv[])
{
    tooling::T8NArgs args;
    fs::path alloc_file;
    fs::path env_file;
    fs::path txs_file;
    fs::path blob_params_file;
    fs::path output_dir;
    fs::path output_result_file;
    fs::path output_alloc_file;
    fs::path output_body_file;
    fs::path opcode_count_filename;
    std::ofstream trace_file;

    try
    {
        for (int i = 0; i < argc; ++i)
        {
            const std::string_view arg{argv[i]};

            if (arg == "-v" || arg == "--version")
            {
                std::cout << "evmone-t8n " EVMONE_VERSION "\n";
                return 0;
            }
            if (arg == "--state.fork" && ++i < argc)
                args.rev = test::to_rev(argv[i]);
            else if (arg == "--input.alloc" && ++i < argc)
                alloc_file = argv[i];
            else if (arg == "--input.env" && ++i < argc)
                env_file = argv[i];
            else if (arg == "--input.txs" && ++i < argc)
                txs_file = argv[i];
            else if (arg == "--input.blobParams" && ++i < argc)
                blob_params_file = argv[i];
            else if (arg == "--output.basedir" && ++i < argc)
            {
                output_dir = argv[i];
                fs::create_directories(output_dir);
            }
            else if (arg == "--output.result" && ++i < argc)
                output_result_file = argv[i];
            else if (arg == "--output.alloc" && ++i < argc)
                output_alloc_file = argv[i];
            else if (arg == "--state.chainid" && ++i < argc)
                args.chain_id = intx::from_string<uint64_t>(argv[i]);
            else if (arg == "--output.body" && ++i < argc)
                output_body_file = argv[i];
            else if (arg == "--trace")
            {
                args.open_trace = [&](size_t tx_index,
                                      const evmc::bytes32& tx_hash) -> std::ostream& {
                    trace_file =
                        std::ofstream{output_dir / ("trace-" + std::to_string(tx_index) + "-0x" +
                                                       evmc::hex(tx_hash) + ".jsonl")};
                    return trace_file;
                };
            }
            else if (arg == "--opcode.count" && ++i < argc)
                opcode_count_filename = argv[i];
            else if (arg == "--state.reward" && ++i < argc)
            {
                if (argv[i] == "-1"sv)  // Hack to compute the root hash of the pre-state.
                    args.pre_state_only = true;
                else
                    args.block_reward = intx::from_string<uint64_t>(argv[i]);
            }
        }

        if (!opcode_count_filename.empty())
            args.opcode_count_file = (output_dir / opcode_count_filename).string();

        auto bind_input = [](std::ifstream& s, const fs::path& p) -> std::istream* {
            if (p.empty())
                return nullptr;
            s.open(p);
            return &s;
        };
        std::ifstream in_alloc;
        std::ifstream in_env;
        std::ifstream in_txs;
        std::ifstream in_blob_params;
        args.alloc = bind_input(in_alloc, alloc_file);
        args.env = bind_input(in_env, env_file);
        args.txs = bind_input(in_txs, txs_file);
        args.blob_params = bind_input(in_blob_params, blob_params_file);

        std::ofstream out_result{output_dir / output_result_file};
        std::ofstream out_alloc{output_dir / output_alloc_file};
        args.out_result = &out_result;
        args.out_alloc = &out_alloc;
        std::ofstream out_body;
        if (!output_body_file.empty())
        {
            out_body.open(output_dir / output_body_file);
            args.out_body = &out_body;
        }

        tooling::t8n(args);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
