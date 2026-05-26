#!/usr/bin/env python
import os
import sys

env = SConscript("godot-cpp/SConstruct")

# For reference:
# - CCFLAGS are compilation flags shared between C and C++
# - CFLAGS are for C-specific compilation flags
# - CXXFLAGS are for C++-specific compilation flags
# - CPPFLAGS are for pre-processor flags
# - CPPDEFINES are for pre-processor defines
# - LINKFLAGS are for linking flags

# tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(CPPPATH=["src/"])
sources = (
    Glob("src/*.cpp")
    + Glob("src/world/*.cpp")
    + Glob("src/path/*.cpp")
    + Glob("src/data/*.cpp")
    + Glob("src/components/*.cpp")
    + Glob("src/core/*.cpp")
    + Glob("src/entities/*.cpp")
)

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "spacetraveller/bin/GameWorld.{}.{}.framework/GameWorld.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
elif env["platform"] == "ios":
    if env["ios_simulator"]:
        library = env.StaticLibrary(
            "spacetraveller/bin/GameWorld.{}.{}.simulator.a".format(env["platform"], env["target"]),
            source=sources,
        )
    else:
        library = env.StaticLibrary(
            "spacetraveller/bin/GameWorld.{}.{}.a".format(env["platform"], env["target"]),
            source=sources,
        )
else:
    library = env.SharedLibrary(
        "spacetraveller/bin/GameWorld{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)
