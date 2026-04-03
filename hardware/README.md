# Hardware — KiCad PCB Design Files

This directory contains the KiCad 7 project for the ESP32-S3 Robotics Controller PCB.

## Files

| File | Description |
|---|---|
| `ESP32RoboticsController.kicad_pro` | KiCad project file |
| `ESP32RoboticsController.kicad_sch` | Schematic |
| `ESP32RoboticsController.kicad_pcb` | PCB layout |
| `ESP32RoboticsController.kicad_dru` | Design rules |
| `fp-lib-table` | Footprint library table |
| `sym-lib-table` | Symbol library table |
| `gerbers/` | Fabrication-ready Gerber files |
| `bom/` | Bill of materials |
| `datasheets/` | Component datasheets |

## PCB Stackup

| Layer | Role |
|---|---|
| F.Cu (Top) | Signal routing + high-current paths |
| In1.Cu | Solid GND plane |
| In2.Cu | VM + regulated power distribution |
| B.Cu (Bottom) | Additional signals + local pours |

## Key Components

| Reference | Part | Package |
|---|---|---|
| U1 | ESP32-S3-WROOM-1 | SMD module |
| U2 | DRV8871 (Motor 1) | HSOP-8 |
| U3 | DRV8871 (Motor 2) | HSOP-8 |
| U4 | 3.3 V LDO regulator | SOT-223 |
| J1–J4 | Ultrasonic sensor headers | 4-pin 2.54 mm |
| J5–J6 | Hall sensor headers | 3-pin 2.54 mm |
| J7–J8 | Motor terminals | 2-pin screw terminal |
| J9 | Expansion GPIO header | 8-pin 2.54 mm |
| J10 | SPI display header | 7-pin 2.54 mm |
| SW1–SW3 | Push buttons | SMD tactile |
| DSP1 | SPI display (e.g., ST7735) | connector |

## Opening the Project

1. Install [KiCad 7](https://www.kicad.org/download/)
2. Open `ESP32RoboticsController.kicad_pro`
3. The schematic opens in Eeschema; the PCB opens in Pcbnew

## Generating Gerbers

In Pcbnew: **File → Fabrication Outputs → Gerbers**

Recommended layers for 4-layer fabrication:
- F.Cu, In1.Cu, In2.Cu, B.Cu
- F.SilkS, B.SilkS
- F.Mask, B.Mask
- Edge.Cuts
- F.Paste

## Design Rules

- Min trace width: 0.2 mm (signal), 0.8 mm (power), 2.0 mm (motor VM)
- Min via drill: 0.3 mm
- Min clearance: 0.2 mm
- Board thickness: 1.6 mm
