# minimal_spectrum_analyzer
A tiny, embeddable command-line sound visualizer.

Supports Linux (PulseAudio) and Windows (WASAPI).

![analyzer_image](https://user-images.githubusercontent.com/4939010/117427977-8f402900-af60-11eb-9d38-c56b4ce0216c.gif)

## Building

### Ubuntu

- C++ compiler that supports C++17
- CMake
- PulseAudio development library

Install required packages.

```
$ sudo apt install cmake libpulse-dev
```

Clone and build source code.

```
$ git clone --recursive https://github.com/agehama/minimal_spectrum_analyzer.git
$ mkdir build
$ cd build
$ cmake ../minimal_spectrum_analyzer .
$ make
```

### Windows

Download and install prerequisites.
- Visual Studio 2019 https://visualstudio.microsoft.com/downloads/
- CMake https://cmake.org/download/
- Git for Windows https://gitforwindows.org/

Clone source code with Git.

```
$ git clone --recursive https://github.com/agehama/minimal_spectrum_analyzer.git
```

Configure and Generate Visual Studio project.
![image](https://user-images.githubusercontent.com/4939010/117149605-541fe780-adf2-11eb-8fb1-2e36ca4f36c3.png)

Open generated project and build with Visual Studio.


## Embedding
To embed the spectrum display into other software, you can receive the text via standard output.

![embedding_image](https://user-images.githubusercontent.com/4939010/117443945-c409ab80-af73-11eb-83d5-8f7376a27261.gif)

Here is an example of how to pass it through a temporary file.

First, start a background process that redirects the stdout of `./analyzer` to a log file.
Make sure to turn off the axis display and set the delimiter to LF.
```
$ ./analyzer --axis off --line_feed LF > analyzer_log &
```

Then ./analyzer_log will contain time-series spectral data separated by LF, as shown below.
```
$ tail -n 10 ./analyzer_log
⣀⣠⣶⣾⣿⣷⣦⣤⣀⣠⣄⡀⢀⡀⣠⡀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⠀⡀⣀⠀⠀⠀
⣠⣤⣶⣾⣿⣷⣦⣤⣀⣀⣀⡀⢀⠀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⠀⠀⣀⠀⠀⠀
⣤⣴⣶⣶⣶⣦⣤⣤⣀⣀⢀⡀⠀⠀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡀⠀⠀⠀
⣴⣶⣶⣶⣶⣦⣤⣤⣀⣀⢀⠀⠀⠀⣀⠀⠀⠀⠀⡀⠀⠀⠀⠀⠀⠀⠀⠀⡀⠀⠀⠀
⣴⣶⣶⣶⣶⣦⣤⣤⣀⢀⠀⠀⠀⠀⣀⠀⠀⠀⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⡀⠀⠀⠀
⣴⣶⣶⣶⣶⣤⣤⣄⣀⠀⠀⠀⠀⠀⣀⠀⠀⡀⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⡀⠀⠀⠀
⣴⣶⣶⣶⣶⣤⣤⣄⣀⠀⠀⠀⠀⠀⣀⠀⠀⡀⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⡀⠀⠀⠀
⣴⣶⣶⣶⣶⣦⣤⣤⣀⠀⠀⠀⠀⠀⣀⠀⢀⡀⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⡀⠀⠀⠀
⣤⣴⣶⣶⣶⣦⣤⣤⣀⣀⠀⠀⢀⣀⣤⣀⣀⡀⢀⣀⡀⢀⡀⣀⢀⣀⣀⣀⣄⣀⣀⣀
⣤⣤⣶⣶⣦⣤⣄⣤⣀⣀⡀⠀⣀⣠⣤⣤⣤⣀⣠⣄⣤⣠⣄⣤⣀⣤⣤⣠⣦⣤⣤⣤
```

Then, call `tail -n 1 . /analyzer_log` from shell, python, or any other environment you like to embed the spectrum display into your program.

