#!/bin/python

import os
import argparse
import sys
from pathlib import Path
import subprocess
import json
import glob

# -----------------------------------------------------------------------------

color_info = "\u001B[94m"
color_warn = "\u001B[93m"
color_fatal = "\u001B[91m"
color_reset = "\u001B[0m"

def info(message):
    print(f" [{color_info}INFO{color_reset}] {message}")

def warn(message):
    print(f" [{color_warn}WARN{color_reset}] {message}")

def error(message):
    print(f"[{color_fatal}ERROR{color_reset}] {message}")

def fatal(message, code = 1):
    print(f"[{color_fatal}FATAL{color_reset}] {message}")
    sys.exit(code)

# ------------------------------------------------------------------------------
#       Arguments
# ------------------------------------------------------------------------------

parser = argparse.ArgumentParser()
parser.add_argument("-U", "--update", action="store_true", help="Update")
parser.add_argument("-V", "--verbose", action="store_true", help="Verbose")
args = parser.parse_args()

# ------------------------------------------------------------------------------

is_windows = os.name == "nt"
is_linux = not is_windows

file_separator = '\\' if is_windows else '/'

exe_name = "lang"
if is_windows:
    exe_name += ".exe"

build_root = ".build"
vendor_folder    = f"{build_root}/3rdparty"
object_folder = f"{build_root}/objects"

os.makedirs(build_root, exist_ok = True)
os.makedirs(vendor_folder, exist_ok = True)
os.makedirs(object_folder, exist_ok = True)

# ------------------------------------------------------------------------------
#       Functions
# ------------------------------------------------------------------------------

import shutil

git_path = shutil.which("git")

def git_source(name, url, branch, dumb = False):
    path = f"{vendor_folder}/{name}"

    if os.path.exists(f"{path}"):
        if args.update:
            info(f"Updating [{name}]")
            subprocess.Popen(["git", "pull"], executable = git_path, cwd = path).wait()
            subprocess.Popen(["git", "submodule", "update", "--depth", "1", "--recursive"], executable = git_path, cwd = path).wait()
    else:
        info(f"Cloning [{name}]")
        subprocess.Popen(["git", "clone", "-b", branch, "--recursive", url, path], executable = git_path).wait()

    return path

defines = []
sources = []
includes = []
source_includes = {}
links = []
path_links = []

def define(v):
    global defines
    defines += [v]

def source(file):
    global sources
    sources += [file]

def include(dir):
    global includes
    includes += [dir]

def source_include(file, dir):
    global source_includes

    source_includes[file] = source_includes.setdefault(file, []) + [dir]

def link(link):
    global links
    links += [link]

def path_link(link):
    global path_links
    path_links += [link]

def build():
    global links

    compile_procs = []
    compiler = "clang++"

    compiler_path = shutil.which("ccache")

    compile_commands = []

    info("compiling...")

    for s in sources:
        filename = Path(s.replace('\\', '-').replace('/', '-')).with_suffix(".o")
        output = f"{object_folder}/{filename}"

        links += [output]

        command  = [compiler, "-c", "-std=c++26"]

        if args.verbose:
            command += ["-v"]

        command += ["-g", "-O0", "-fno-omit-frame-pointer"]

        command += [
            # "-Werror",
            "-Wpedantic",
            "-Wall",

            "-Wno-missing-field-initializers",
            "-Wno-gnu-conditional-omitted-operand",
            "-Wno-comment",
            "-Wno-binding-in-condition",
            "-Wno-deprecated-declarations",
            "-Wno-unused-function",
            "-Wno-nested-anon-types",
            ]

        command += ["-o", output, s]

        for i in includes:
            command += ["-I", i]

        if s in source_includes:
            for i in source_includes[s]:
                command += ["-I", i]

        for d in defines:
            command += [f"-D{d}"]

        compile_commands += [{
            "directory": os.getcwd(),
            "command": ' '.join(command),
            "file": f"{os.getcwd()}/{s}",
            "output": f"{os.getcwd()}/{output}"
        }]

        p = subprocess.Popen(args = command, executable = compiler_path, cwd = os.getcwd(), stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        compile_procs += [(command, p)]

    with open(f"{build_root}/compile_commands.json", "w") as out:
        json.dump(compile_commands, out, indent = 2)

    any_failed = False

    for command, p in compile_procs:
        code = p.wait()
        if code != 0:
            any_failed = True
            error(f"compile command failed with code {code}")
            error(f"  args: {command}")
            _, err = p.communicate()
            print(err.decode('utf-8'))

    if any_failed:
        error("Aborting link due to failed compilation")
        return

    link_command  = [compiler, "-fuse-ld=lld"]
    if args.verbose:
        link_command += ["-v"]

    link_command += ["-g", "-O0", "-fno-omit-frame-pointer"]

    link_command += [
        # "-Werror",
        "-Wpedantic",
        "-Wall",
        ]

    link_command += ["-o", f"{build_root}/{exe_name}"]

    for l in links:
        link_command += [l]

    for l in path_links:
        link_command += [f"-l{l}"]

    info("linking...")

    p = subprocess.Popen(args = link_command, executable = compiler_path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    code = p.wait()
    if code != 0:
        error(f"link command failed with code {code}")
        error(f"  args: {link_command}")
        _, err = p.communicate()
        print(err.decode('utf-8'))

# -----------------------------------------------------------------------------

simdutf_dir = git_source("simdutf", "https://github.com/simdutf/simdutf.git", "master")
simdutf_src = f"{simdutf_dir}/src/simdutf.cpp"
source(simdutf_src)
source_include(simdutf_src, f"{simdutf_dir}/src")
include(f"{simdutf_dir}/include")

# -----------------------------------------------------------------------------

backward_dir = git_source("backward-cpp", "https://github.com/bombela/backward-cpp.git", "master")
if is_linux:
    define("BACKWARD_HAS_BFD=1")
    path_link("bfd")
source(f"{backward_dir}/backward.cpp")

# -----------------------------------------------------------------------------

for s in glob.glob("src/**/*.cpp", recursive = True):
    source(s)
include("src")

# -----------------------------------------------------------------------------

build()
