# LightRec

LightRec is a hyper-optimized, native C++20 background screen recording and "shadowplay" style replay application for Windows. It utilizes a zero-copy GPU path to encode gameplay and screen activity with minimal CPU usage and an extremely small memory footprint.

## Prerequisites

To compile and run LightRec, ensure you have the following installed on your system:

- **Operating System:** Windows 10 or Windows 11 (64-bit).
- **Visual Studio 2022:** You must have the **"Desktop development with C++"** workload installed.
- **Windows SDK:** Ensure you have the latest Windows 10 or 11 SDK installed (usually included with the C++ workload).
- **CMake:** Version 3.24 or higher. Make sure CMake is added to your system `PATH`.
- **Hardware:** A modern GPU (Nvidia, AMD, or Intel) with hardware encoding support (NVENC, AMF, or QSV).

## Getting Started

### 1. Clone the repository

Open your terminal or PowerShell and clone the project:

```bash
git clone https://github.com/your-username/LightRec.git
cd LightRec
```

### 2. Configure and Build

LightRec uses CMake to handle its build system and dependencies (like ImGui and Intel VPL).

Generate the build files and compile the executable in `Release` mode using the following commands from the root directory:

```bash
# Configure the CMake project
cmake -B build

# Compile the project
cmake --build build --config Release
```

*Note: You must build in `Release` mode to achieve the target performance metrics (< 2% CPU usage).*

### 3. Run the Application

Once successfully compiled, the executable will be located in the `build/Release` folder. You can launch it directly from the terminal:

```bash
.\build\Release\LightRec.exe
```

## Features

- **Zero-Copy Pipeline:** Direct texture sharing between the Windows Graphics Capture API (DXGI) and the hardware encoder, preventing slow copies to system RAM.
- **Hardware Agnostic:** Automatically probes and utilizes Intel QSV, NVIDIA NVENC, or AMD AMF depending on your system for the lowest overhead.
- **Native UI:** Utilizes a lightweight system tray icon and native Win32 dialogs to keep the application footprint minuscule (No Electron, no Chromium).
- **In-Game Overlay:** A built-in transparent Direct3D overlay to show your recording status, memory footprint, and frame rate.

## Usage

- **System Tray:** Upon launching, LightRec minimizes directly to your system tray. 
- **Settings:** Double-click the tray icon to open the configuration dialog.
- **Menu:** Right-click the tray icon to `Toggle Replay`, `Save a Clip`, `Open the Overlay`, or `Exit`.
- **Hotkeys:** The default global hotkey to save a clip is `Alt + Shift + F10` (this can be customized in the settings).

## Architecture

- **C++20:** Modern C++ idioms, concepts, and standard library features.
- **Lock-free Queues:** Highly efficient `RingBuffer` using `std::atomic` to manage A/V packets without mutex contention on the critical path.
- **Multi-threaded:** Dedicated threads for Audio capture, Video capture, Muxing, and UI to ensure no frame drops.
