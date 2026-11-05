// gart - command-line ray-tracing simulator.
//
// Scenarios (defaults reproduce the original Visual Studio programs):
//   anechoic  4x4x4 m room, all walls absorbing, receiver 2 m below the source
//   case1     4x4x4 m room, rigid floor, receiver moved vertically below the source
//   case2     10x10x4 m room, rigid floor, receiver moved horizontally from the source
//   grid      10x10x4 m room, rigid floor, SPL map on a horizontal slice
//
// Example: gart case2 --rays 3e6 --freq 500 --out case2_500Hz.csv
#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gart/analytical.hpp"
#include "gart/parallel.hpp"

using namespace gart;

namespace {

constexpr std::string_view kUsage = R"(usage: gart <anechoic|case1|case2|grid> [options]

options:
  --rays N          number of rays            (default 1e6; anechoic 5e6)
  --freq HZ         source frequency          (default 250)
  --power DB        source power level        (default 100)
  --order N         max reflection order      (default 1; anechoic 0)
  --radius M        receiver radius           (default 0.025)
  --floor RE[,IM]   floor reflection coeff.   (default 1,0; anechoic 0)
  --walls RE[,IM]   other walls' coefficient  (default 0)
  --step M          receiver step / grid cell (default 0.05; grid 0.1)
  --slice-z M       grid slice height         (default 2)
  --seed N          random seed               (default 1)
  --threads N       worker threads, 0 = all   (default 0)
  --out FILE        CSV output                (default <scenario>.csv)
)";

struct Options {
    std::string scenario;
    std::map<std::string, std::string, std::less<>> values;

    [[nodiscard]] std::optional<std::string_view> get(std::string_view key) const {
        if (auto it = values.find(key); it != values.end()) return it->second;
        return std::nullopt;
    }
    [[nodiscard]] double num(std::string_view key, double fallback) const {
        if (auto v = get(key)) return std::stod(std::string(*v));
        return fallback;
    }
    [[nodiscard]] Complex complex(std::string_view key, Complex fallback) const {
        auto v = get(key);
        if (!v) return fallback;
        const std::string s(*v);
        const auto comma = s.find(',');
        if (comma == std::string::npos) return {std::stod(s), 0.0};
        return {std::stod(s.substr(0, comma)), std::stod(s.substr(comma + 1))};
    }
};

std::optional<Options> parse(int argc, char** argv) {
    if (argc < 2) return std::nullopt;
    Options o;
    o.scenario = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::string_view a = argv[i];
        if (!a.starts_with("--") || i + 1 >= argc) return std::nullopt;
        o.values[std::string(a.substr(2))] = argv[++i];
    }
    return o;
}

struct Timer {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    [[nodiscard]] double ms() const {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
};

void printScene(const Scene& s, std::string_view name) {
    std::cout << "scenario      " << name << '\n'
              << "room (LxWxH)  " << s.room.length << " x " << s.room.width << " x " << s.room.height << " m\n"
              << "source        (" << s.source.position.x << ", " << s.source.position.y << ", " << s.source.position.z
              << ")  " << s.source.powerDb << " dB (" << s.source.powerWatts() << " W), " << s.source.frequency
              << " Hz\n"
              << "rays          " << s.settings.rayCount << ", max order " << s.settings.maxOrder << '\n';
}

int run(const Options& o) {
    const bool anechoic = o.scenario == "anechoic";
    Scene s;
    s.source.frequency = o.num("freq", 250);
    s.source.powerDb = o.num("power", 100);
    s.settings.rayCount = static_cast<std::uint64_t>(o.num("rays", anechoic ? 5e6 : 1e6));
    s.settings.maxOrder = static_cast<int>(o.num("order", anechoic ? 0 : 1));
    s.settings.seed = static_cast<std::uint64_t>(o.num("seed", 1));
    const double radius = o.num("radius", 0.025);
    const auto threads = static_cast<unsigned>(o.num("threads", 0));
    const Complex floorR = o.complex("floor", anechoic ? Complex{} : Complex{1, 0});
    const Complex wallR = o.complex("walls", Complex{});

    if (o.scenario == "anechoic" || o.scenario == "case1") s.room = {4, 4, 4};
    else if (o.scenario == "case2" || o.scenario == "grid") s.room = {10, 10, 4};
    else {
        std::cerr << "unknown scenario '" << o.scenario << "'\n" << kUsage;
        return 2;
    }
    s.room.R.fill(wallR);
    s.room.R[Bottom] = floorR;

    if (o.scenario == "case2" || o.scenario == "grid") s.source.position = {s.room.length / 2, s.room.width / 2, 1.0};
    else s.source.position = {s.room.length / 2, s.room.width / 2, 3.0};

    if (auto err = validate(s); !err.empty()) {
        std::cerr << "invalid scene: " << err << '\n';
        return 2;
    }

    const std::string outPath = std::string(o.get("out").value_or(o.scenario + ".csv"));
    std::ofstream out(outPath);
    if (!out) {
        std::cerr << "cannot open " << outPath << '\n';
        return 1;
    }
    out.precision(10);
    printScene(s, o.scenario);
    const auto images = imageSources(s, s.settings.maxOrder);
    Timer timer;

    if (o.scenario == "grid") {
        const double step = o.num("step", 0.1);
        GridReceivers g;
        g.axis = 2;
        g.offset = o.num("slice-z", 2.0);
        g.nu = static_cast<int>(s.room.length / step);
        g.nv = static_cast<int>(s.room.width / step);
        g.du = s.room.length / g.nu;
        g.dv = s.room.width / g.nv;
        g.radius = radius;
        const Accumulator acc = traceParallel(s, g, threads);
        out << "x,y,spl_raytraced,hits,spl_analytical\n";
        for (std::size_t i = 0; i < g.size(); ++i) {
            const Vec3 c = g.center(i);
            out << c.x << ',' << c.y << ',' << toSpl(acc.pressure[i]) << ',' << acc.hits[i] << ','
                << toSpl(imageSourcePressure(s, images, c)) << '\n';
        }
    } else {
        PointReceivers rx;
        rx.radius = radius;
        std::vector<double> distance;
        const double step = o.num("step", 0.05);
        const Vec3 src = s.source.position;
        if (anechoic) {
            rx.centers = {{src.x, src.y, 1.0}};
            distance = {src.z - 1.0};
        } else if (o.scenario == "case1") {
            for (double d = step; d < src.z - step * 0.5; d += step) {
                rx.centers.push_back({src.x, src.y, d});
                distance.push_back(d);
            }
        } else {
            for (double d = step; d < 5.0 - step * 0.5; d += step) {
                rx.centers.push_back({src.x + d, src.y, 2.0});
                distance.push_back(d);
            }
        }
        const Accumulator acc = traceParallel(s, rx, threads);
        out << "d,spl_raytraced,hits,spl_analytical,error_db\n";
        for (std::size_t i = 0; i < rx.size(); ++i) {
            const double rt = toSpl(acc.pressure[i]);
            const double an = toSpl(imageSourcePressure(s, images, rx.centers[i]));
            out << distance[i] << ',' << rt << ',' << acc.hits[i] << ',' << an << ',' << rt - an << '\n';
            if (anechoic)
                std::cout << "SPL ray-traced " << rt << " dB, analytical " << an << " dB, error " << rt - an
                          << " dB, hits " << acc.hits[i] << '\n';
        }
    }

    const double ms = timer.ms();
    std::cout << "traced in     " << ms << " ms ("
              << static_cast<double>(s.settings.rayCount) / (ms / 1000.0) / 1e6 << " M rays/s)\n"
              << "wrote         " << outPath << '\n';
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const auto opts = parse(argc, argv);
    if (!opts) {
        std::cerr << kUsage;
        return 2;
    }
    try {
        return run(*opts);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
