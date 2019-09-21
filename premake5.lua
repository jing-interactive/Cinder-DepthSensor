-- https://github.com/premake/premake-core/wiki

local action = _ACTION or ""

solution "OpenDepthSensor"
    location (action)
    configurations { "Debug", "Release" }
    language "C++"

    configuration "vs*"
        platforms {"x64"}
        defines {
            "_CRT_SECURE_NO_WARNINGS",
            "_CRT_SECURE_NO_DEPRECATE",
        }

        cppdialect "C++11"

        disablewarnings {
            "4244",
            "4305",
            "4996",
        }

        staticruntime "On"

        configuration "x64"
            libdirs {
                "lib/msw/x64",
            }
            targetdir ("lib/msw/x64")

    configuration "macosx"
        platforms {"x64"}
        cppdialect "gnu++11"

        sysincludedirs {
            "include",
            "/usr/local/include/libusb-1.0",
            "../../include",
            "3rdparty",
            "3rdparty/librealsense/include",
            "3rdparty/openni2/include",
        }

        libdirs {
            "lib/macosx",
        }
        targetdir ("lib/macosx")

    flags {
        "MultiProcessorCompile"
    }

    configuration "Debug"
        defines { "DEBUG" }
        symbols "On"
        targetsuffix "-d"

    configuration "Release"
        defines { "NDEBUG" }
        optimize "On"

    project "OpenDepthSensor"
        kind "StaticLib"

        includedirs {
            "include",
            "../../include",
            "3rdparty",
            "3rdparty/ImiSDK",
            "3rdparty/librealsense/include",
            "3rdparty/v1/sdk/inc",
            "3rdparty/v2/sdk/inc",
            "3rdparty/openni2/include",
            "3rdparty/k4a",
        }

        files {
            "include/*",
            "src/*",
            "3rdparty/openni2/**",
            "3rdparty/librealsense/include/**",
            "3rdparty/librealsense/src/*",
            "3rdparty/k4a/*",
        }

        configuration "vs*"
            defines {
                "RS_USE_WMF_BACKEND",
            }

            files {
                "3rdparty/v1/**",
                "3rdparty/v2/**",
            }

        configuration "macosx"
            defines {
                "RS_USE_LIBUVC_BACKEND",
            }

            files {
                "3rdparty/librealsense/src/libuvc/*",
            }
