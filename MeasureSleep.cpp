#include "args.hxx"
#include <format>
#include <iomanip>
#include <iostream>
#include <vector>
#include <windows.h>

typedef NTSTATUS(CALLBACK *NTQUERYTIMERRESOLUTION)(
    OUT PULONG MinimumResolution,
    OUT PULONG MaximumResolution,
    OUT PULONG CurrentResolution);

int main(int argc, char **argv) {
    std::string version = "0.1.1";

    args::ArgumentParser parser("MeasureSleep " + version + "\nGitHub - https://github.com/amitxv");
    args::HelpFlag help(parser, "help", "display this help menu", {"help"});
    args::ValueFlag<int> samples(parser, "", "measure the Sleep(1) deltas for a specified amount of samples then compute the maximum, average, minimum and stdev from the collected samples", {"samples"});

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

    ULONG MinimumResolution, MaximumResolution, CurrentResolution;
    LARGE_INTEGER start, end, freq;
    std::vector<double> sleep_delays;

    QueryPerformanceFrequency(&freq);

    HMODULE ntdll = LoadLibrary(L"NtDll.dll");

    if (!ntdll) {
        std::cerr << "LoadLibrary failed\n";
        return 1;
    }

    NTQUERYTIMERRESOLUTION NtQueryTimerResolution = (NTQUERYTIMERRESOLUTION)GetProcAddress(ntdll, "NtQueryTimerResolution");

    for (int i = 1;; i++) {
        // get current resolution
        if (NtQueryTimerResolution(&MinimumResolution, &MaximumResolution, &CurrentResolution) != 0) {
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

        std::cout << std::fixed << std::setprecision(6) << "Resolution: " << (CurrentResolution / 10000.0) << "ms, Sleep(1) slept " << delta_ms << "ms (delta: " << delta_from_sleep << ")\n";

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
        double sum = 0;
        for (double delay : sleep_delays) {
            sum += delay;
        }

        auto max = std::max_element(sleep_delays.begin(), sleep_delays.end());
        double average = sum / sleep_delays.size();
        auto min = std::min_element(sleep_delays.begin(), sleep_delays.end());

        // stdev
        double standard_deviation = 0.0;

        for (double delay : sleep_delays) {
            standard_deviation += pow(delay - average, 2);
        }

        double stdev = sqrt(standard_deviation / 10);

        std::cout << "\nMax: " << *max << "\n";
        std::cout << "Avg: " << average << "\n";
        std::cout << "Min: " << *min << "\n";
        std::cout << "STDEV: " << stdev << "\n";
    }
}
