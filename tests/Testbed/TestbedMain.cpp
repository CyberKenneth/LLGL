/*
 * TestbedMain.cpp
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#include "TestbedContext.h"
#include <string>
#include <regex>
#include <exception>
#include <stdio.h>

#ifdef _WIN32
#   include <Windows.h>
#endif


using namespace LLGL;

static unsigned RunRendererIndependentTests(int argc, char* argv[])
{
    Log::Printf("Run renderer independent tests\n");
    TestbedContext::PrintSeparator();
    unsigned failures = TestbedContext::RunRendererIndependentTests(argc, argv);
    TestbedContext::PrintSeparator();
    return failures;
}

static unsigned RunTestbedForRenderer(const char* moduleName, int version, int argc, char* argv[])
{
    if (version != 0)
        Log::Printf("Run Testbed: %s (%d)\n", moduleName, version);
    else
        Log::Printf("Run Testbed: %s\n", moduleName);
    TestbedContext::PrintSeparator();
    TestbedContext context{ moduleName, version, argc, argv };
    unsigned failures = context.RunAllTests();
    TestbedContext::PrintSeparator();
    Log::Printf("\n");
    return failures;
}

struct ModuleAndVersion
{
    std::string name;
    int         version;

    ModuleAndVersion(const char* name, int version = 0) :
        name    { name    },
        version { version }
    {
    }

    ModuleAndVersion(const std::string& name, int version = 0) :
        name    { name    },
        version { version }
    {
    }
};

static ModuleAndVersion GetRendererModule(const std::string& name)
{
    if (name == "gl" || name == "opengl")
        return "OpenGL";
    if (name == "vk" || name == "vulkan")
        return "Vulkan";
    if (name == "mt" || name == "mtl" || name == "metal")
        return "Metal";
    if (name == "d3d11" || name == "dx11" || name == "direct3d11")
        return "Direct3D11";
    if (name == "d3d12" || name == "dx12" || name == "direct3d12")
        return "Direct3D12";
    if (name == "null")
        return "Null";
    if (std::regex_match(name, std::regex(R"(gl\d{3})")))
        return ModuleAndVersion{ "OpenGL", std::atoi(name.c_str() + 2) };
    if (std::regex_match(name, std::regex(R"(opengl\d{3})")))
        return ModuleAndVersion{ "OpenGL", std::atoi(name.c_str() + 6) };
    return name.c_str();
}

// Returns true of the specified list of program arguments contains the search string
static bool HasProgramArgument(int argc, char* argv[], const char* search)
{
    for (int i = 1; i < argc; ++i)
    {
        if (::strcmp(argv[i], search) == 0)
            return true;
    }
    return false;
}

static void PrintHelpDocs()
{
    Log::Printf(
        "Testbed MODULES* OPTIONS*\n"
        "  -> Runs LLGL's unit tests\n"
        "\n"
        "MODULE:\n"
        "  gl, gl[VER], opengl, opengl[VER] ... OpenGL module with optional version, e.g. gl330\n"
        "  vk, vulkan ......................... Vulkan module\n"
        "  mt, mtl, metal ..................... Metal module\n"
        "  d3d11, dx11, direct3d11 ............ Direct3D 11 module\n"
        "  d3d12, dx12, direct3d12 ............ Direct3D 12 module\n"
        "\n"
        "OPTIONS:\n"
        "  -d, --debug ........................ Enable validation debug layers\n"
        "  -f, --fast ......................... Run fast test; skips certain configurations\n"
        "  -g, --greedy ....................... Keep running each test even after failure\n"
        "  -h, --help ......................... Print this help document\n"
        "  -p, --pedantic ..................... Disable diff-checking threshold\n"
        "  -s, --santiy-check ................. Print some test results even on success\n"
        "  -t, --timing ....................... Print timing results\n"
        "  -v, --verbose ...................... Print more information\n"
        "  --amd .............................. Prefer AMD device\n"
        "  --intel ............................ Prefer Intel device\n"
        "  --nvidia ........................... Prefer NVIDIA device\n"
    );
}

static int GuardedMain(int argc, char* argv[])
{
    Log::RegisterCallbackStd();

    // If -h or --help is specified, only print help documentation and exit
    if (HasProgramArgument(argc, argv, "-h") || HasProgramArgument(argc, argv, "--help"))
    {
        PrintHelpDocs();
        return 0;
    }

    // Gather all explicitly specified module names
    std::vector<ModuleAndVersion> enabledModules;
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] != '-')
            enabledModules.push_back(GetRendererModule(argv[i]));
    }

    if (enabledModules.empty())
    {
        std::vector<std::string> availableModules = RenderSystem::FindModules();
        enabledModules.reserve(availableModules.size());
        for (const std::string& module : availableModules)
            enabledModules.push_back(module);
    }

    unsigned modulesWithFailedTests = 0;

    // Run renderer independent tests
    if (RunRendererIndependentTests(argc - 1, argv + 1) != 0)
        ++modulesWithFailedTests;

    // Run renderer specific tests
    for (const ModuleAndVersion& module : enabledModules)
    {
        if (RunTestbedForRenderer(module.name.c_str(), module.version, argc - 1, argv + 1) != 0)
            ++modulesWithFailedTests;
    }

    // Print summary
    if (modulesWithFailedTests == 0)
        Log::Printf(" ==> ALL MODULES PASSED\n");
    else if (modulesWithFailedTests == 1)
        Log::Printf(" ==> 1 MODULE FAILED\n");
    else if (modulesWithFailedTests > 1)
        Log::Printf(" ==> %u MODULES FAILED\n", modulesWithFailedTests);

    #ifdef _WIN32
    system("pause");
    #endif

    // Return number of failed modules as error code
    return static_cast<int>(modulesWithFailedTests);
}

#ifdef _WIN32

// Declare function that is not directly exposed in LLGL
namespace LLGL
{
    LLGL_EXPORT UTF8String DebugStackTrace(unsigned firstStackFrame = 0, unsigned maxNumStackFrames = 64);
};

static LONG WINAPI TestbedVectoredExceptionHandler(EXCEPTION_POINTERS* e)
{
    if ((e->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) == 0)
    {
        LLGL::UTF8String stackTrace = DebugStackTrace();
        ::fprintf(
            stderr,
            "Exception during test run: Address=%p, Code=0x%08X\n"
            "Callstack:\n"
            "----------\n"
            "%s\n",
            e->ExceptionRecord->ExceptionAddress, e->ExceptionRecord->ExceptionCode, stackTrace.c_str()
        );
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

#endif // /_WIN32

int main(int argc, char* argv[])
{
    #ifdef _WIN32

    AddVectoredExceptionHandler(1, TestbedVectoredExceptionHandler);
    __try
    {
        return GuardedMain(argc, argv);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        ::fflush(stderr);
        return 1;
    }

    #else

    try
    {
        return GuardedMain(argc, argv);
    }
    catch (const std::exception& e)
    {
        ::fprintf(stderr, "Exception during test run: %s\n", e.what());
        ::fflush(stderr);
        return 1;
    }

    #endif
}


