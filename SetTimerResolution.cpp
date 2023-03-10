#include "args.hxx"
#include <iomanip>
#include <iostream>
#include <windows.h>

typedef NTSTATUS(CALLBACK *NTQUERYTIMERRESOLUTION)(
    OUT PULONG MinimumResolution,
    OUT PULONG MaximumResolution,
    OUT PULONG CurrentResolution);

typedef NTSTATUS(CALLBACK *NTSETTIMERRESOLUTION)(
    IN ULONG DesiredResolution,
    IN BOOLEAN SetResolution,
    OUT PULONG CurrentResolution);

typedef BOOL(WINAPI *PSET_PROCESS_INFORMATION)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);

int main(int argc, char **argv) {
    std::string version = "0.1.0";

    args::ArgumentParser parser("SetTimerResolution " + version + "\nGitHub - https://github.com/amitxv");
    args::HelpFlag help(parser, "help", "display this help menu", {"help"});
    args::ValueFlag<int> resolution(parser, "", "specify the desired resolution in 100-ns units", {"resolution"}, args::Options::Required);
    args::Flag no_console(parser, "no-console", "hide the console window", {"no-console"});

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
        std::cerr << parser;
        return 1;
    }

    if (no_console) {
        FreeConsole();
    }

    ULONG MinimumResolution, MaximumResolution, CurrentResolution;

    HMODULE ntdll = LoadLibrary(L"ntdll.dll");
    HMODULE kernel32 = LoadLibrary(L"kernel32.dll");

    if (!ntdll or !kernel32) {
        std::cerr << "LoadLibrary failed\n";
        return 1;
    }

    NTQUERYTIMERRESOLUTION NtQueryTimerResolution = (NTQUERYTIMERRESOLUTION)GetProcAddress(ntdll, "NtQueryTimerResolution");
    NTSETTIMERRESOLUTION NtSetTimerResolution = (NTSETTIMERRESOLUTION)GetProcAddress(ntdll, "NtSetTimerResolution");
    PSET_PROCESS_INFORMATION SetProcessInformation = (PSET_PROCESS_INFORMATION)GetProcAddress(kernel32, "SetProcessInformation");

    // does not exist in Windows 7
    if (SetProcessInformation) {
        PROCESS_POWER_THROTTLING_STATE PowerThrottling;
        RtlZeroMemory(&PowerThrottling, sizeof(PowerThrottling));

        PowerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        PowerThrottling.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
        PowerThrottling.StateMask = 0;

        SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &PowerThrottling, sizeof(PowerThrottling));
    }

    if (NtQueryTimerResolution(&MinimumResolution, &MaximumResolution, &CurrentResolution)) {
        std::cerr << "NtQueryTimerResolution failed\n";
        return 1;
    }

    if (NtSetTimerResolution(args::get(resolution), true, &CurrentResolution)) {
        std::cerr << "NtSetTimerResolution failed\n";
        return 1;
    }

    std::cout << std::fixed << std::setprecision(6) << "Resolution set to: " << (CurrentResolution / 10000.0) << "ms\n";
    Sleep(INFINITE);
}
