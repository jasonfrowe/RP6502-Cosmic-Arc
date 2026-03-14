# Cosmic Arc for RP6502

A modern reimagining of the 1982 Atari 2600 classic **Cosmic Arc** by Imagic, rewritten for the **Picocomputer 6502 (RP6502)** using the LLVM-MOS SDK.

## Project Goal
This project serves as a template and development bed for creating a high-fidelity version of Cosmic Arc. Leveraging the RP6502's advanced graphics capabilities (sprites, 16-bit color, and larger memory), we aim to move beyond the technical limitations of the original Atari 2600 hardware while preserving the core gameplay loop:
- Defending the mothership from planetary defenses.
- Deploying the shuttle to rescue planetary inhabitants.
- Navigating the starfield.

## Prerequisites

### 1. LLVM-MOS SDK
The project requires the [LLVM-MOS SDK](https://llvm-mos.org/wiki/Welcome) to be installed and in your `PATH`.
- **VSCode Tip:** If LLVM-MOS conflicts with your system LLVM, add the following to `.vscode/settings.json`:
  ```json
  {
      "cmake.environment": {
          "PATH": "~/llvm-mos/bin:${env:PATH}"
      }
  }
  ```

### 2. Python Environment
Python 3.9+ is required for the build tools and asset pipeline.
- Create and activate a virtual environment:
  ```bash
  python3 -m venv .venv
  source .venv/bin/activate  # macOS/Linux
  # or
  .venv\Scripts\activate     # Windows
  ```
- Install dependencies:
  ```bash
  pip install -r requirements.txt
  ```

## Setup & Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/picocomputer/vscode-llvm-mos-cosmic-arc
   cd vscode-llvm-mos-cosmic-arc
   code .
   ```
2. **Configure CMake:**
   When prompted by the VSCode CMake extension, select **[Unspecified]** for the kit, or let it detect the LLVM-MOS toolchain.

## Building & Running

- **Build & Upload:** Press **F5** in VSCode. This will:
  1. Compile the C/ASM source code using LLVM-MOS.
  2. Pack the assets into an RP6502 ROM.
  3. Upload the ROM to the Picocomputer via USB.
- **Serial Connection:** The `tools/rp6502.py` script manages the connection. The first time you run the project, it will create a `.rp6502` configuration file. Adjust the `device` path if the auto-detection fails.

## Asset Pipeline

This project includes custom tools for preparing game assets:

- **`tools/convert_sprite.py`**: Converts PNG images into RP6502-compatible binary formats.
  - Supports **Sprite** mode (16-bit RGB555).
  - Supports **Tile** mode (4-bit indexed color).
  - Usage: `python tools/convert_sprite.py Sprites/my_sprite.png -o build/my_sprite.bin --mode sprite`

- **`Sprites/`**: Contains original artwork, including:
  - `CosmicArc.aseprite`: Source artwork (Aseprite format).
  - `CosmicArc.png`: Exported sprite sheet.

## Project Structure
- `src/`: C and Assembly source code.
- `tools/`: Python scripts for asset conversion and device communication.
- `Sprites/`: Raw and exported graphical assets.
- `CMakeLists.txt`: Build configuration and asset definitions.

## License
This project is licensed under the terms specified in the `LICENSE` file.
The original "Cosmic Arc" gameplay and branding are the property of their respective owners. This is a non-commercial fan project.
