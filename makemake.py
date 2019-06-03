#!/usr/bin/env python3

# this script analyzes all the source files and extracts any header dependencies it uses.
# it then constructs a makefile for automated building with partial compilation.
# the output makefile is additionally optimized for parallel compilation.

import os, shutil, re

# --------------------------------------

# name of the output makefile
out_name = "makefile"

# name of the exe file to produce (as a result of running the generated makefile)
exe_name = "csx.exe"

# the list of source directories - their contents are searched for .cpp files
source_dirs = [ "./", "src/" ]

# the directory to place object files in
objdir = "obj/"

build_info = [
    type("obj", (object,), x) for x in [
        { "name" : "release",     "compile" : "g++ -O4 -Wall -Wextra -Wpedantic -Wshadow -std=c++17 -c {}",                                             "link" : "g++ {} -lstdc++fs" },
        { "name" : "debug",       "compile" : "g++ -Og -Wall -Wextra -Wpedantic -Wshadow -std=c++17 -c {} -Wno-maybe-uninitialized",                    "link" : "g++ {} -lstdc++fs" },
        { "name" : "release-san", "compile" : "clang++ -O3 -Wall -Wextra -Wpedantic -Wshadow -std=c++17 -fsanitize=undefined -fsanitize=address -c {}", "link" : "clang++ -fsanitize=undefined -fsanitize=address {} -lstdc++fs" },
        { "name" : "debug-san",   "compile" : "clang++ -Og -Wall -Wextra -Wpedantic -Wshadow -std=c++17 -fsanitize=undefined -fsanitize=address -c {}", "link" : "clang++ -fsanitize=undefined -fsanitize=address {} -lstdc++fs" },
    ]
]

# --------------------------------------

source = []
for dir in source_dirs:
    for name in os.listdir(dir):
        if name.endswith(".cpp"):
            dep = []
            with open(dir + name, "r") as file:
                for line in file:
                    match = re.fullmatch("\\s*#include \"(.*)\"\\s*", line)
                    if match: dep.append(match.groups()[0])
            source.append(type("obj", (object,), { "name" : name, "dir" : dir, "dep" : dep, "size" : os.path.getsize(dir + name) }))

source.sort(key=lambda s: s.size, reverse=True)

print("Parsed Dependencies:\n")
for s in source:
    print(f"{s.dir + s.name} ({s.size}):")
    for d in s.dep: print(f"\t{s.dir + d}")
    print()

# --------------------------------------

if os.path.exists(objdir): shutil.rmtree(objdir)

with open(out_name, "w") as out:
    objsets = []

    for build in build_info:
        os.makedirs(objdir + build.name)

        objs = ""
        for s in source: objs += f" {objdir + build.name}/{s.name[:-4]}.o"
        objsets.append(objs)

        out.write(f"{build.name}:{objs}\n\t{build.link.format(objs)} -o {exe_name}\n\n")

        for s in source:
            dep = ""
            for d in s.dep: dep += " {}".format(s.dir + d)
            obj_path = f"{objdir + build.name}/{s.name[:-4]}.o"
            out.write(f"{obj_path}:{dep}\n\t{build.compile.format(s.dir + s.name)} -o {obj_path}\n\n")
    
    clean_cmd = f"clean:\n\trm -f {exe_name}"
    for set in objsets: clean_cmd += f"\n\trm -f {set}"
    out.write(clean_cmd + "\n\n")
