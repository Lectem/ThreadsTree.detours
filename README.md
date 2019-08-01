# ThreadsTree.Detours

A Detours .dll to display threads of a program as a tree in the .dot format.

## Usage

This project is using [Detours](https://github.com/microsoft/Detours) to patch functions.

Start by building ThreadsTree.detours.dll using CMake.

Then use the pre-built `external/Detours/bin.X86/with-dll.exe` executable to inject the detours dll (requires administrator privileges):

```
with-dll.exe -d:ThreadsTree.detours.dll YourProgram.exe
```

Note that it will spawn YourProgram.exe as a subprocess, so you might be interested in the following Visual Studio extension [Microsoft Child Process Debugging Power Tool](https://marketplace.visualstudio.com/items?itemName=vsdbgplat.MicrosoftChildProcessDebuggingPowerTool).

A ThreadTrees.dot will be created in the current directory, in the dot GraphViz format. Can be viewed online with https://edotor.net/ for example.


Example output available here: https://gist.github.com/Lectem/1d71458dac6b1e22eb5c0c4f0de72313


## Requirements :

- CMake (buildsystem)
- Visual Studio (compiler)
- Fetching fmt submodule (can easily be removed)