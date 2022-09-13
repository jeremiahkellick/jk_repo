# jk_repo

My monorepo for various C projects that may share code

## Build

You are meant to be able to build any program in the repo with a single command. You only need to specify the program's main source file in the build command. Dependencies are found recursively and automatically included in the build.

For example, here's how to build and run the hello_world.c program. First, navigate to the jk_repo directory in your terminal. Next, run the following commands depending on your operating system.

### Windows (MSVC)

Build
```
.\bin\c.bat .\src\learning_c\hello_world.c
```

Run
```
.\build\hello_world.exe
```

### Mac / Linux (GCC)

Build
```
./bin/c ./src/learning_c/hello_world.c
```

Run
```
./build/hello_world
```
