# minimal_spectrum_analyzer
A tiny, embeddable command-line sound visualizer.

Supports Linux (PulseAudio) and Windows (WASAPI).

<img width="400" alt="ScreenShot" src="https://github.com/agehama/minimal_spectrum_analyzer/blob/images/analyzer.gif">

## Building

### Ubuntu

- C++ compiler that supports C++17
- CMake
- PulseAudio development library

Install required packages.

```
sudo apt install cmake libpulse-dev
```

Clone and build source code.

```
git clone --recursive https://github.com/agehama/minimal_spectrum_analyzer.git
mkdir build
cd build
cmake ../minimal_spectrum_analyzer .
make
```

### Windows

Download and install prerequisites.
- Visual Studio 2019 https://visualstudio.microsoft.com/downloads/
- CMake https://cmake.org/download/
- Git for Windows https://gitforwindows.org/

Clone source code.

```
git clone --recursive https://github.com/agehama/minimal_spectrum_analyzer.git
```

Configure and Generate with CMake.
![image](https://user-images.githubusercontent.com/4939010/117149605-541fe780-adf2-11eb-8fb1-2e36ca4f36c3.png)

Open and build project with Visual Studio.
