// evmtime ROUNDS PROGRAMS_DIR REPS_FILE LIB...: loads every libevmone variant, then for each round, program and variant
// (rotating order) runs REPS executions and records the elapsed nanoseconds. Prints JSON {program: {lib: [ns...]}}.
#include <evmc/evmc.hpp>
#include <evmc/mocked_host.hpp>
#include <dlfcn.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static evmc::bytes read_hex(const std::string& path)
{
    std::ifstream f{path};
    const std::string s{std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
    evmc::bytes b;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
    {
        if (s[i] == '\n' || s[i] == ' ')
            break;
        b.push_back(static_cast<uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
    }
    return b;
}

int main(int argc, char** argv)
{
    if (argc < 5)
        return 1;
    const int rounds = std::atoi(argv[1]);
    const std::string dir = argv[2];
    struct Prog { std::string name; evmc::bytes code, input; int64_t gas; int reps; };
    std::vector<Prog> progs;
    std::ifstream rf{argv[3]};
    for (std::string name; rf >> name;)
    {
        int reps = 0;
        rf >> reps;
        std::ifstream g{dir + "/" + name + ".gas"};
        int64_t gas = 0;
        g >> gas;
        progs.push_back({name, read_hex(dir + "/" + name + ".code"), read_hex(dir + "/" + name + ".input"), gas, reps});
    }
    std::vector<std::pair<std::string, evmc::VM>> vms;
    for (int i = 4; i < argc; ++i)
    {
        void* lib = dlopen(argv[i], RTLD_NOW | RTLD_LOCAL);
        if (!lib)
        {
            std::fprintf(stderr, "dlopen %s: %s\n", argv[i], dlerror());
            return 1;
        }
        const auto create = reinterpret_cast<evmc_vm* (*)()>(dlsym(lib, "evmc_create_evmone"));
        vms.emplace_back(argv[i], evmc::VM{create()});
    }
    std::map<std::string, std::map<std::string, std::vector<long long>>> res;
    for (int r = 0; r < rounds; ++r)
        for (auto& p : progs)
            for (size_t k = 0; k < vms.size(); ++k)
            {
                auto& [name, vm] = vms[(k + static_cast<size_t>(r)) % vms.size()];
                evmc::MockedHost host;
                evmc_message msg{};
                msg.kind = EVMC_CALL;
                msg.gas = p.gas;
                msg.input_data = p.input.data();
                msg.input_size = p.input.size();
                const auto t0 = std::chrono::steady_clock::now();
                for (int i = 0; i < p.reps; ++i)
                    (void)vm.execute(host, EVMC_OSAKA, msg, p.code.data(), p.code.size());
                const auto t1 = std::chrono::steady_clock::now();
                res[p.name][name].push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
            }
    std::ostringstream o;
    o << "{";
    bool fp = true;
    for (auto& [pn, byv] : res)
    {
        o << (fp ? "" : ",") << "\"" << pn << "\":{";
        fp = false;
        bool fv = true;
        for (auto& [vn, ts] : byv)
        {
            o << (fv ? "" : ",") << "\"" << vn.substr(vn.rfind('/') + 1) << "\":[";
            fv = false;
            for (size_t i = 0; i < ts.size(); ++i)
                o << (i ? "," : "") << ts[i];
            o << "]";
        }
        o << "}";
    }
    o << "}\n";
    std::fputs(o.str().c_str(), stdout);
}
