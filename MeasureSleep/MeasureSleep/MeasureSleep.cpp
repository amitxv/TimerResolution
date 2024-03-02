#include "args.hxx"
#include <format>
#include <iomanip>
#include <iostream>
#include <vector>
#include <Windows.h>

extern "C" NTSYSAPI NTSTATUS NTAPI NtQueryTimerResolution(PULONG MinimumResolution, PULONG MaximumResolution, PULONG CurrentResolution);

int main(int argc, char** argv) {
    std::string version = "0.1.7";

    args::ArgumentParser parser("MeasureSleep Version " + version + " - GPLv3\nGitHub - https://github.com/amitxv\n");
    args::HelpFlag help(parser, "help", "display this help menu", { "help" });
    args::ValueFlag<int> samples(parser, "", "measure the Sleep(1) deltas for a specified amount of samples then compute the maximum, average, minimum and stdev from the collected samples", { "samples" });

    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help) {
        std::cout << parser;
        return 0;
    } catch (args::ParseError e) {
        std::cerr << e.what();
        std::cerr << parser;
        return 1;
    } catch (args::ValidationError e) {
        std::cerr << e.what();
        std::cerr << parser.Help();
        return 1;
    }

    if (samples && args::get(samples) < 2) {
        std::cerr << "--samples must be larger than 1\n";
        return 1;
    }

    ULONG minimum_resolution, maximum_resolution, current_resolution;
    LARGE_INTEGER start, end, freq;
    std::vector<double> sleep_delays;

    QueryPerformanceFrequency(&freq);

    if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)) {
        std::cerr << "SetPriorityClass failed\n";
        return 1;
    }

    for (int i = 1;; i++) {
        // get current resolution
        if (NtQueryTimerResolution(&minimum_resolution, &maximum_resolution, &current_resolution) != 0) {
            std::cerr << "NtQueryTimerResolution failed\n";
            return 1;
        }

        // benchmark Sleep(1)
        QueryPerformanceCounter(&start);
        Sleep(1);
        QueryPerformanceCounter(&end);

        double delta_s = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;
        double delta_ms = delta_s * 1000;
        double delta_from_sleep = delta_ms - 1;

        std::cout << std::fixed << std::setprecision(4) << "Resolution: " << (current_resolution / 10000.0) << "ms, Sleep(1) slept " << delta_ms << "ms (delta: " << delta_from_sleep << ")\n";

        if (samples) {
            sleep_delays.push_back(delta_from_sleep);

            if (i == args::get(samples)) {
                break;
            }

            Sleep(100);
        } else {
            Sleep(1000);
        }
    }

    if (samples) {
        // discard first trial since it is almost always invalid
        sleep_delays.erase(sleep_delays.begin());

        sort(sleep_delays.begin(), sleep_delays.end());

        size_t size = sleep_delays.size();

        double sum = 0.0;
        for (double delay : sleep_delays) {
            sum += delay;
        }

        double average = sum / size;

        // stdev
        double standard_deviation = 0.0;

        for (double delay : sleep_delays) {
            standard_deviation += pow(delay - average, 2);
        }

        double stdev = sqrt(standard_deviation / (size - 1));

        std::cout << "\nResults from " << args::get(samples) - 1 << " samples\n";

        std::cout << "\nMax: " << sleep_delays.back() << "\n";
        std::cout << "Avg: " << average << "\n";
        std::cout << "Min: " << sleep_delays.front() << "\n";
        std::cout << "STDEV: " << stdev << "\n";
    }
}
