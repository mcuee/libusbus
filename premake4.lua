
solution "usbus"
    configurations { "Debug", "Release" }

    project "usbus"
        kind "StaticLib"
        language "C"

        files "src/*.c"
        includedirs { "src" }

        if os.is("macosx") then
            defines { "PLATFORM_OSX" }
            files { "src/platform/iokit.c" }
        elseif os.is("windows") then
            defines { "PLATFORM_WIN" }
            files { "src/platform/winusb.c" }
        end

        configuration "Debug"
            defines { "DEBUG" }
            flags { "Symbols" }

        configuration "Release"
            defines { "RELEASE" }
            flags { "Optimize" }

    project "enumerator"
        kind "ConsoleApp"
        language "C"

        files { "example/enumerator/*.c" }
        includedirs { "src" }
        links { "usbus" }
        if os.is("macosx") then
            links { "IOKit.framework", "CoreFoundation.framework" }
        elseif os.is("windows") then
            links { "setupapi", "winusb" }
        end
