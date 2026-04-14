// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <CLI/CLI.hpp>
#include <evmc/hex.hpp>
#include <evmc/tooling.hpp>
#include <evmone/evmone.h>
#include <fstream>

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

        CLI::App app{"evmone EVM tool"};
        const auto& version_flag = *app.add_flag("--version", "Print version information and exit");
        app.add_flag("--trace", trace, "Enable execution trace");
        app.add_flag("--histogram", histogram, "Enable opcode histogram");

        auto& run_cmd = *app.add_subcommand("run", "Execute EVM bytecode")->fallthrough();
        run_cmd.add_option("code", code_arg, "Bytecode")->required()->check(HexOrFile);
        run_cmd.add_option("--gas", gas, "Execution gas limit")
            ->capture_default_str()
            ->check(CLI::Range(0, 1000000000));
        run_cmd.add_option("--rev", rev, "EVM revision")->capture_default_str();
        run_cmd.add_option("--input", input_arg, "Input bytes")->check(HexOrFile);
        run_cmd.add_flag("--create", create,
            "Create new contract out of the code and then execute this contract with the input");
        run_cmd.add_flag("--bench", bench,
            "Benchmark execution time (state modification may result in unexpected behaviour)");

        try
        {
            app.parse(argc, argv);

            VM vm{evmc_create_evmone()};

            if (trace)
                vm.set_option("trace", "");
            if (histogram)
                vm.set_option("histogram", "");

            // Handle the --version flag first and exit when present.
            if (version_flag)
            {
                std::cout << vm.name() << " " << vm.version() << "\n";
                return 0;
            }

            if (run_cmd)
            {
                // If code_arg or input_arg contains invalid hex string, an exception is thrown.
                const auto code = load_from_hex(code_arg);
                const auto input = load_from_hex(input_arg);
                return tooling::run(vm, rev, gas, code, input, create, bench, std::cout);
            }

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
