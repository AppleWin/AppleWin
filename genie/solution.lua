------------------------------------------------------------------------------
-- GENie configuration script for AppleWin
--
-- GENie is project generator tool. It generates compiler solution and project
-- files from Lua scripts and supports many different compiler versions.
--
-- This configuration script generates Visual Studio solution and project
-- files for any Visual Studio version of interest. For instance, to generate
-- a solution for Visual Studio 2019, open a command prompt in the directory
-- containing this file, and type:
--
--   genie vs2019
--
-- This will create a Visual Studio solution file called AppleWin.sln in the
-- same directory.
--
-- See https://github.com/bkaradzic/GENie for more information.
--
-- Full documentation:
--  https://github.com/bkaradzic/GENie/blob/master/docs/scripting-reference.md
------------------------------------------------------------------------------

-- Add the genie "clean" action
newaction {
    trigger     = "clean",
    description = "Clean the build directory",
}

-- Display usage if no action is specified on the command line.
if _ACTION == nil then
    return
end

-- We're always building for Windows.
_OPTIONS['os'] = "windows"

-- Prepare commonly-used directory variables.
solutionDir = path.getabsolute(".")
rootDir     = path.getabsolute("..")
targetDir   = path.join(rootDir, "build")
objDir      = path.join(rootDir, "build/obj")
srcDir      = path.join(rootDir, "source")
rsrcDir     = path.join(rootDir, "resource")

if _ACTION == "clean" then
    os.rmdir(targetDir)
    return
end

if not string.match(_ACTION, "vs.*") then
    print("ERROR: Only visual studio is currently supported.\n")
    return
end

if _ACTION == "vs2019" then
    premake.vstudio.diagformat = "Column"
end


------------------------------------------------------------------------------
-- AppleWin solution
--
-- Generate the Visual Studio solution file.
------------------------------------------------------------------------------
solution("AppleWin")
    location(solutionDir)
    startproject("AppleWin")

    language "C++"

    configurations {
        "Debug",
        "DebugNoDX",
        "Release",
        "ReleaseNoDX",
    }

    platforms {
        "x32",
    }

    flags {
        "NativeWChar",
        "Symbols",
    }

    configuration { "Debug*" }
        defines {
            "_DEBUG",
        }
        flags {
            "DebugRuntime",
        }

    configuration { "Release*" }
        defines {
            "NDEBUG",
        }
        flags {
            "OptimizeSpeed",
            "ReleaseRuntime",
        }

    configuration { "windows" }
        defines {
            "_WINDOWS",
            "WIN32",
        }

    configuration {} -- reset


------------------------------------------------------------------------------
-- set_output_dirs
--
-- Helper function to set a project's target and object directories.
------------------------------------------------------------------------------
function set_output_dirs(name)
    configuration { "Debug" }
        targetdir(path.join(targetDir, "Debug"))
        objdir(path.join(targetDir, "Debug/obj", name))

    configuration { "DebugNoDX" }
        defines { "NO_DIRECT_X" }
        targetdir(path.join(targetDir, "DebugNoDX"))
        objdir(path.join(targetDir, "DebugNoDX/obj", name))

    configuration { "Release" }
        targetdir(path.join(targetDir, "Release"))
        objdir(path.join(targetDir, "Release/obj", name))

    configuration { "ReleaseNoDX" }
        defines { "NO_DIRECT_X" }
        targetdir(path.join(targetDir, "ReleaseNoDX"))
        objdir(path.join(targetDir, "ReleaseNoDX/obj", name))

    configuration {} -- reset
end

------------------------------------------------------------------------------
-- AppleWin project
--
-- Generate the AppleWin executable project
------------------------------------------------------------------------------
project("AppleWin")
    kind "WindowedApp"
    uuid(os.uuid("app-AppleWin"))
    set_output_dirs("AppleWin")

    flags {
        "StaticRuntime",
        "WinMain",
    }

    -- Add -D compile options
    defines {
        "NO_DSHOW_STRSAFE",
        "YAML_DECLARE_STATIC",
        "_CRT_SECURE_NO_DEPRECATE",
    }

    -- Add additional compile options
    buildoptions {
        "/utf-8",
    }

    -- List include paths
    includedirs {
        path.join(srcDir, "cpu"),
        path.join(srcDir, "emulator"),
        path.join(srcDir, "debugger"),
        path.join(rootDir, "zlib"),
        path.join(rootDir, "zip_lib"),
        path.join(rootDir, "libyaml/include"),
    }

    -- List all files that should appear in the project
    files {
        path.join(srcDir, "**.h"),
        path.join(srcDir, "**.cpp"),
        path.join(srcDir, "**.inl"),
        path.join(rsrcDir, "*"),
        path.join(rootDir, "bin/History.txt"),
        path.join(rootDir, "docs/CodingConventions.txt"),
        path.join(rootDir, "docs/Debugger_Changelog.txt"),
        path.join(rootDir, "docs/FAQ.txt"),
        path.join(rootDir, "docs/ToDo.txt"),
        path.join(rootDir, "docs/Video_Cleanup.txt"),
        path.join(rootDir, "docs/Wishlist.txt"),
    }
    removefiles {
        path.join(srcDir, "Peripheral_Clock*"),
    }

    -- Libraries that must be linked to build the executable
    links {
        "advapi32",
        "comctl32",
        "comdlg32",
        "dinput8",
        "dsound",
        "dxguid",
        "gdi32",
        "htmlhelp",
        "ole32",
        "shell32",
        "strmiids",
        "user32",
        "version",
        "winmm",
        "wsock32",
        "yaml",
        "zlib",
        "zip_lib",
        "HookFilter",
        "TestCPU6502",
    }

    -- Set up precompiled headers
    configuration { "vs*" }
        pchheader("StdAfx.h")
        pchsource(path.join(srcDir, "StdAfx.cpp"))
        nopch {
            path.join(srcDir, "Tfe/*.cpp"),
            path.join(srcDir, "Z80VICE/*.cpp"),
        }
    configuration {} -- reset

    -- Run the CPU unit test before building
    prebuildcommands { "echo Performing unit-test: TestCPU6502" }
    configuration { "Debug" }
        prebuildcommands { path.join(targetDir, "Debug", "TestCPU6502") }
    configuration { "DebugNoDX" }
        prebuildcommands { path.join(targetDir, "DebugNoDX", "TestCPU6502") }
    configuration { "Release" }
        prebuildcommands { path.join(targetDir, "Release", "TestCPU6502") }
    configuration { "ReleaseNoDX" }
        prebuildcommands { path.join(targetDir, "ReleaseNoDX", "TestCPU6502") }
    configuration {} -- reset


    -- Organize files into VS filter folders
    vpaths {
        ["Docs"] = {
            path.join(rootDir, "docs/**"),
            path.join(rootDir, "bin/**"),
        },
        ["Resource Files"] = {
            path.join(rootDir, "resource/**"),
        },
        ["Source Files"] = {
            path.join(srcDir, "Applewin.*"),
            path.join(srcDir, "StdAfx.*"),
        },
        ["Source Files/_Headers"] = {
            path.join(srcDir, "Common.h"),
            path.join(rsrcDir, "resource.h"),
        },
        ["Source Files/CommonVICE"] = {
            path.join(srcDir, "CommonVICE/*"),
        },
        ["Source Files/Configuration"] = {
            path.join(srcDir, "Configuration/*"),
        },
        ["Source Files/CPU"] = {
            path.join(srcDir, "CPU*"),
            path.join(srcDir, "cpu/*"),
        },
        ["Source Files/Debugger"] = {
            path.join(srcDir, "Debugger/*"),
        },
        ["Source Files/Disk"] = {
            path.join(srcDir, "Disk*"),
            path.join(srcDir, "Harddisk*"),
        },
        ["Source Files/Emulator"] = {
            path.join(srcDir, "6821*"),
            path.join(srcDir, "AY8910*"),
            path.join(srcDir, "Joystick*"),
            path.join(srcDir, "Keyboard*"),
            path.join(srcDir, "LanguageCard*"),
            path.join(srcDir, "Log*"),
            path.join(srcDir, "Memory*"),
            path.join(srcDir, "Mockingboard*"),
            path.join(srcDir, "MouseInterface*"),
            path.join(srcDir, "NoSlotClock*"),
            path.join(srcDir, "ParallelPrinter*"),
            path.join(srcDir, "Registry*"),
            path.join(srcDir, "Riff*"),
            path.join(srcDir, "SAM*"),
            path.join(srcDir, "SaveState*"),
            path.join(srcDir, "SerialComms*"),
            path.join(srcDir, "SoundCore*"),
            path.join(srcDir, "Speaker*"),
            path.join(srcDir, "Speech*"),
            path.join(srcDir, "SSI263Phonemes.h"),
            path.join(srcDir, "Tape*"),
            path.join(srcDir, "YamlHelper*"),
            path.join(srcDir, "z80emu*"),
        },
        ["Source Files/Model"] = {
            path.join(srcDir, "Pravets*"),
        },
        ["Source Files/Uthernet"] = {
            path.join(srcDir, "Tfe/*"),
        },
        ["Source Files/Video"] = {
            path.join(srcDir, "Frame*"),
            path.join(srcDir, "NTSC*"),
            path.join(srcDir, "RGB*"),
            path.join(srcDir, "Video*"),
        },
        ["Source Files/Z80VICE"] = {
            path.join(srcDir, "Z80VICE/*"),
        },
    }


------------------------------------------------------------------------------
-- yaml project
--
-- Generate the yaml static library.
------------------------------------------------------------------------------
project("yaml")
    kind "StaticLib"
    uuid(os.uuid("lib-yaml"))
    set_output_dirs("yaml")

    flags {
        "StaticRuntime",
    }

    includedirs {
        path.join(rootDir, "libyaml/win32"),
        path.join(rootDir, "libyaml/include"),
    }

    defines {
        "_LIB",
        "HAVE_CONFIG_H",
        "YAML_DECLARE_STATIC",
        "_CRT_SECURE_NO_WARNINGS",
    }

    files {
        path.join(rootDir, "libyaml/**.h"),
        path.join(rootDir, "libyaml/**.c"),
    }

    vpaths {
        ["Source Files"] = { path.join(rootDir, "libyaml/*") },
    }


------------------------------------------------------------------------------
-- zlib project
--
-- Generate the zlib static library.
------------------------------------------------------------------------------
project("zlib")
    kind "StaticLib"
    uuid(os.uuid("lib-zlib"))
    set_output_dirs("zlib")

    flags {
        "StaticRuntime",
    }

    defines {
        "_LIB",
        "_CRT_SECURE_NO_WARNINGS",
    }

    files {
        path.join(rootDir, "zlib/**.h"),
        path.join(rootDir, "zlib/**.c"),
    }


------------------------------------------------------------------------------
-- zip_lib project
--
-- Generate the zip_lib static library.
------------------------------------------------------------------------------
project("zip_lib")
    kind "StaticLib"
    uuid(os.uuid("lib-zip_lib"))
    set_output_dirs("zip_lib")

    flags {
        "StaticRuntime",
    }

    includedirs {
        path.join(rootDir, "zlib"),
    }

    defines {
        "_LIB",
        "_CRT_SECURE_NO_WARNINGS",
    }

    files {
        path.join(rootDir, "zip_lib/**.h"),
        path.join(rootDir, "zip_lib/**.c"),
    }



------------------------------------------------------------------------------
-- HookFilter DLL project
--
-- Generate the HookFilter DLL.
------------------------------------------------------------------------------
project("HookFilter")
    kind "SharedLib"
    uuid(os.uuid("lib-HookFilter"))
    set_output_dirs("HookFilter")

    includedirs {
        path.join(rootDir, "zlib"),
    }

    defines {
        "_USRDLL",
        "HOOKFILTER_EXPORTS",
    }

    files {
        path.join(rootDir, "HookFilter/**.cpp"),
    }


------------------------------------------------------------------------------
-- TestCPU6502 application project
--
-- Generate the TestCPU6502 executable.
------------------------------------------------------------------------------
project("TestCPU6502")
    kind "ConsoleApp"
    uuid(os.uuid("app-TestCPU6502"))
    set_output_dirs("TestCPU6502")

    defines {
        "_CONSOLE",
        "_LIB",
    }

    files {
        path.join(rootDir, "test/TestCPU6502/**.h"),
        path.join(rootDir, "test/TestCPU6502/**.cpp"),
    }
