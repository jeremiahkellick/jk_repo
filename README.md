# jk_repo

My monorepo for various C projects that may share code

## Compilation

This repo uses a lightweight, custom build program called `jk_build`

### Quickstart

Open a terminal and change directory to the root of this repository. Run the following commands. If
it works, the last one should print "Hello, world!".

Windows
```
.\bin\build_jk_build
.\bin\jk_build .\jk_src\learning_c\hello_world.c
.\build\hello_world
```

macOS/Linux
```
./bin/build_jk_build
./bin/jk_build ./jk_src/learning_c/hello_world.c
./build/hello_world
```

### Prerequisites

`jk_build` defaults to MSVC on Windows, Clang on macOS, and GCC on Linux. If you'd prefer to use a
different compiler than the default on your platform, use the `-c` option. You can see more on that
by running `jk_build --help`.

`jk_build` will fail if the desired compiler is not in your `PATH`. On Windows, it looks for `cl`.
For `cl` to be found, you'll either need to run `jk_build` from a
[Developer Command Prompt](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170#developer_command_prompt_shortcuts)
or execute
[vcvarsall.bat](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170#developer_command_file_locations)
before using `jk_build`.

### Building jk_build

To build `jk_build`, you can use the `bin\build_jk_build.bat` Batch script (Windows) or the
`bin/build_jk_build` sh script (macOS/Linux). You could also simply pass
`jk_src/jk_build/jk_build.c` to your preferred compiler.

Open a terminal and change directory to the root of this repository. Run the
following command.

Windows
```
.\bin\build_jk_build
```
 
macOS/Linux
```
./bin/build_jk_build
```

This will produce an executable you can use to compile any other program in this repository. It will
be located at `bin/jk_build`. For convenience, you might want to add the `bin/` directory to your
`PATH`.

### Building other programs

A program in this repo is simply any `.c` or `.cpp` file that defines an entry point function. To
build any of these programs, all you need to do is pass that file as an argument to `jk_build`. You
don't need to pass in dependencies as arguments because `jk_build` will find them for you and
include them when it invokes a compiler.

#### Build
Windows
```
.\bin\jk_build .\jk_src\learning_c\hello_world.c
```
macOS/Linux
```
./bin/jk_build ./jk_src/learning_c/hello_world.c
```

This will produce an execuable with the same name as the `.c` file, located in the `build/`
directory.

#### Run
Windows
```
.\build\hello_world
```
macOS/Linux
```
./build/hello_world
```

### Configuring jk_build

`jk_build` supports a few command line options you can see by running `jk_build --help`, but most
compilation options will need to be configured by editing the jk_build source at
`jk_src/jk_build/jk_build.c`. Edit the source to add whichever compiler flags you want then rebuild
it by running the `build_jk_build` script. Note that jk_build is capable of building itself, but
that will put the resulting executable in the `build/` directory instead of `bin/`.
