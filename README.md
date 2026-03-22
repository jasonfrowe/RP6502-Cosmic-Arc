# Cosmic Arc for RP6502

A modern reimagining of the 1982 Atari 2600 classic **Cosmic Ark** by Imagic, rewritten for the **Picocomputer 6502 (RP6502)** using the LLVM-MOS SDK.

---

## Story

The sun of Alpha Ro is fading fast! Soon it will flicker out. The Cosmic Ark races to save creatures from doomed planets in that solar system. Meteor showers bombard the Ark, threatening its Atlantean crew — and planetary defense systems make this mission of mercy doubly treacherous! Time and energy slip away. Work fast, or these defenceless little beasties will disappear for all time.

---

## How to Play

### Starting the Game
From the title / demo screen, press **Space** (or **A** on a gamepad) to begin. Release and press again to start — a press-and-release is required so the game starts intentionally.

---

### Game Play

Cosmic Arc moves through two distinct phases:

#### 1 — Asteroid Shower (Deep Space)

The Cosmic Ark battles its way through an asteroid shower while drifting through deep space.

- Fire your laser cannon in any of four directions.
- **Keyboard:** tap an **Arrow key** to fire in that direction.
- **Gamepad:** tap the **D-pad** or **left stick** in the direction you wish to fire.
- The joystick / arrow key must return to the neutral (centred) position between each shot.
- Firing costs 1 energy unit — don't spray blindly.
- After destroying 8 asteroids the Ark descends toward the nearest planet.

> **Note:** Asteroids can approach from any side. Stay alert and prioritise threats closest to the Ark.

#### 2 — Shuttleship Rescue (Planet Surface)

If you survived the asteroid shower, the Cosmic Ark descends and lands. A shuttleship launches from the Ark's bay by holding down on the D-pad / left stick.

**Piloting the shuttleship:**

| Action | Keyboard | Gamepad |
|---|---|---|
| Move left | ← Left Arrow | D-pad / left stick left |
| Move right | → Right Arrow | D-pad / left stick right |
| Tractor beam | Space | A button |

- Line up the shuttleship directly over a beastie, then **press and hold** the tractor beam button.
- Keep the beam active until the creature is safely aboard — releasing too early drops it back to the surface.
- A blip sounds when a beastie is successfully captured.
- Try to capture **both** beasties and return before the warning klaxon sounds. If you manage it, the Ark's energy is **fully restored**.
- If the warning sounds before both are caught, the shuttleship automatically returns. Defend the Ark through the next asteroid wave, then the Ark will return to the same planet to complete the rescue.

> **Beware!** Automatic planetary defense turrets are located on either side of the planet surface. They move up and down and fire at intervals. A direct hit costs energy and releases any beastie the shuttleship was carrying.

---

### Energy (Shields)

The shield bar at the bottom of the screen represents your energy reserves. The game ends when energy is depleted.

| Event | Energy Change |
|---|---|
| Destroy an asteroid | +1 |
| Capture a beastie | +12 |
| Capture both beasties before the klaxon | **Full restore** |
| Asteroid strikes the Ark | −12 |
| Fire the laser | −1 |

---

### Scoring

| Event | Points |
|---|---|
| Destroy an asteroid | 10 |
| Capture both beasties and return to Ark | 1 000 |

Your score is displayed at the bottom of the screen.

---

### Tips

- Prioritise beastie rescues — the +12 energy per capture and the full-restore bonus are the only reliable way to sustain long runs.
- Beasties are skittish and will dodge the shuttleship. Anticipate their movement and cut them off rather than chasing.
- Planetary defense turrets cannot be destroyed. Learn their firing rhythm and move between shots.
- If the full-rescue bonus is out of reach, at least capture one beastie before the klaxon to offset the energy cost of the coming asteroid wave.

---

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
