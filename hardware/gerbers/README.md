# Gerber Files

Place fabrication-ready Gerber files in this directory after generating them from KiCad.

## Required Files for 4-Layer PCB Fabrication

| File | Layer |
|---|---|
| `ESP32RoboticsController-F_Cu.gbr` | Top copper |
| `ESP32RoboticsController-In1_Cu.gbr` | Inner layer 1 (GND plane) |
| `ESP32RoboticsController-In2_Cu.gbr` | Inner layer 2 (Power) |
| `ESP32RoboticsController-B_Cu.gbr` | Bottom copper |
| `ESP32RoboticsController-F_SilkS.gbr` | Front silkscreen |
| `ESP32RoboticsController-B_SilkS.gbr` | Back silkscreen |
| `ESP32RoboticsController-F_Mask.gbr` | Front solder mask |
| `ESP32RoboticsController-B_Mask.gbr` | Back solder mask |
| `ESP32RoboticsController-F_Paste.gbr` | Front solder paste |
| `ESP32RoboticsController-Edge_Cuts.gbr` | Board outline |
| `ESP32RoboticsController.drl` | Drill file (excellon) |

## Generating from KiCad 7

1. Open PCB in Pcbnew
2. File → Fabrication Outputs → Gerbers
3. Select output directory: `hardware/gerbers/`
4. Enable all required layers above
5. Click "Plot", then "Generate Drill Files"
