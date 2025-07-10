#!/bin/python

import os
import argparse
import sys
from pathlib import Path
import subprocess
import json
import glob

# -----------------------------------------------------------------------------

color_debug = "\u001B[96m"
color_info = "\u001B[94m"
color_warn = "\u001B[93m"
color_fatal = "\u001B[91m"
color_reset = "\u001B[0m"

clear_line = "\x1b[1K\r"

def log_debug(message):
    print(f" [{color_debug}INFO{color_reset}] {message}")

def log_info(message):
    print(f" [{color_info}INFO{color_reset}] {message}")

def log_info_progress(message):
    print(f"{clear_line} [{color_info}INFO{color_reset}] {message}", end = '')

def log_info_progress_clear():
    print(clear_line, end = '\r')

def log_info_progress_end():
    print()

def log_warn(message):
    print(f" [{color_warn}WARN{color_reset}] {message}")

def log_error(message):
    print(f"[{color_fatal}ERROR{color_reset}] {message}")

def panic(message, code = 1):
    print(f"[{color_fatal}FATAL{color_reset}] {message}")
    sys.exit(code)

# ------------------------------------------------------------------------------
#       Arguments
# ------------------------------------------------------------------------------

parser = argparse.ArgumentParser()
parser.add_argument("-U", "--update", action="store_true", help="Update")
parser.add_argument("-V", "--verbose", action="store_true", help="Verbose")
parser.add_argument("-M", "--mode", default="RelWithDebInfo", help="Set build mode")
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
            log_info(f"Updating [{name}]")
            subprocess.Popen(["git", "pull"], executable = git_path, cwd = path).wait()
            subprocess.Popen(["git", "submodule", "update", "--depth", "1", "--recursive"], executable = git_path, cwd = path).wait()
    else:
        log_info(f"Cloning [{name}]")
        subprocess.Popen(["git", "clone", "-b", branch, "--recursive", url, path], executable = git_path).wait()

    return path

hg_path = shutil.which("hg")

def hg_source(name, url):
    path = f"{vendor_folder}/{name}"

    if os.path.exists(f"{path}"):
        if args.update:
            panic("TODO")
    else:
        log_info(f"Cloning [{name}]")
        subprocess.Popen(["hg", "clone", url, path], executable = hg_path).wait()

    return path

defines = []
sources = []
includes = []
source_includes = {}
links = []
path_links = []

compiler_warning_flags = [
    # "-Wpedantic",
    "-Wall",

    "-Wno-missing-field-initializers",
    "-Wno-gnu-conditional-omitted-operand",
    "-Wno-comment",
    "-Wno-binding-in-condition",
    "-Wno-deprecated-declarations",
    "-Wno-unused-function",
    "-Wno-nested-anon-types",
    "-Wno-unused-variable",
    "-Wno-unused-but-set-variable",

    "-Wno-gnu-label-as-value",
    "-Wno-c99-designator",
    "-Wno-gnu-case-range",

    "-Wimplicit-fallthrough",
]

build_mode_flags = {
    "Debug":          [ "-g", "-O0", "-fsanitize=undefined,address" ],
    "RelWithDebInfo": [ "-g", "-O2", "-fno-omit-frame-pointer" ],
    "Release":        [ "-O2" ],
}

build_mode = args.mode
if not build_mode in build_mode_flags:
    panic(f"Unrecognised build mode: {build_mode}")
else:
    log_info(f"{build_mode}")

def define(v):
    global defines
    defines += [v]

def source(file, external):
    global sources
    sources += [(file, external)]

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
    # compiler = "clang++"
    compiler = "g++"

    compiler_path = shutil.which("ccache")

    compile_commands = []

    for s, external in sources:
        filename = Path(s.replace('\\', '-').replace('/', '-')).with_suffix(".o")
        output = f"{object_folder}/{filename}"

        links += [output]

        command  = [compiler, "-c", "-std=c++26"]

        if args.verbose:
            command += ["-v"]

        command += ["-o", output, s]

        if not external:
            command += ["-Werror"]

        command += compiler_warning_flags
        command += build_mode_flags[build_mode]

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

    log_info_progress(f"Compiling [0/{len(compile_procs)}]")
    compiles_finished = 0

    for command, p in compile_procs:
        code = p.wait()
        if code != 0:
            any_failed = True
            log_info_progress_end()
            log_error(f"compile command failed with code {code}")
            log_error(f"  args: {command}")
            _, err = p.communicate()
            print(err.decode('utf-8'))

        compiles_finished = compiles_finished + 1
        log_info_progress(f"Compiling [{compiles_finished}/{len(compile_procs)}]")

    log_info_progress_end()

    if any_failed:
        log_error("Aborting link due to failed compilation")
        return

    link_command  = [compiler, "-fuse-ld=lld"]
    if args.verbose:
        link_command += ["-v"]

    link_command += ["-o", f"{build_root}/{exe_name}"]

    link_command += compiler_warning_flags
    link_command += build_mode_flags[build_mode]

    for l in links:
        link_command += [l]

    for l in path_links:
        link_command += [f"-l{l}"]

    log_info_progress("Linking   [0/1]")

    if args.verbose:
        log_info_progress_end()
        log_debug(f"Linking [{exe_name}]: {link_command}")

    p = subprocess.Popen(args = link_command, executable = compiler_path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    code = p.wait()
    if code != 0:
        log_info_progress_end()
        log_error(f"link command failed with code {code}")
        log_error(f"  args: {link_command}")
        _, err = p.communicate()
        print(err.decode('utf-8'))
    else:
        log_info_progress("Linking   [1/1]")
        log_info_progress_end()

    log_info_progress_end()

# -----------------------------------------------------------------------------

dyncall_dir = hg_source("dyncall", "https://dyncall.org/pub/dyncall/dyncall/")
include(f"{dyncall_dir}")
# link(f"{dyncall_dir}/dynload/libdynload_s.a")
# link(f"{dyncall_dir}/dyncall/libdyncall_s.a")

# -----------------------------------------------------------------------------

# zstd_dir = git_source("zstd", "https://github.com/facebook/zstd.git", "dev")
# include(f"{zstd_dir}/lib")
# for d in [
#     "common",
#     "compress",
# ]:
#     for s in glob.glob(f"{zstd_dir}/lib/{d}/*.c"):
#         source(s, external = True)

# -----------------------------------------------------------------------------

unordered_dense_dir = git_source("unordered_dense", "https://github.com/martinus/unordered_dense.git", "main")
include(f"{unordered_dense_dir}/include")

# -----------------------------------------------------------------------------

simdutf_dir = git_source("simdutf", "https://github.com/simdutf/simdutf.git", "master")
simdutf_src = f"{simdutf_dir}/src/simdutf.cpp"
source(simdutf_src, external = True)
source_include(simdutf_src, f"{simdutf_dir}/src")
include(f"{simdutf_dir}/include")

# -----------------------------------------------------------------------------

backward_dir = git_source("backward-cpp", "https://github.com/bombela/backward-cpp.git", "master")
if is_linux:
    define("BACKWARD_HAS_BFD=1")
    path_link("bfd")
source(f"{backward_dir}/backward.cpp", external = True)

# -----------------------------------------------------------------------------

for s in glob.glob("src/**/*.cpp", recursive = True):
    source(s, external = False)
include("src")

for s in glob.glob("test/**/*.cpp", recursive = True):
    source(s, external = False)
include("test")

# -----------------------------------------------------------------------------

build()
