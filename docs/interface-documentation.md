# Interface Documentation

Wokwi simulation project: https://wokwi.com/projects/460815309855625217

## Direct Board Connections

The following Arduino interfaces are used by the project.

### Power Pins

All modules listed in this section are connected to the development board through a breadboard.

| Board Pin | Module | Module Pin |
|:---:|:---:|:---:|
| 5V | Relay module | VCC |
| 5V | Relay module | COM |
| 5V | OLED display | VCC |
| 5V | Soil moisture sensor | VCC |
| 5V | Light sensor | VCC |
| 5V | Water level sensor | + |
| 5V | Buzzer module | VCC |
| GND | Soil moisture sensor | GND |
| GND | Light sensor | GND |
| GND | Water level sensor | - |
| GND | Buzzer module | GND |
| GND | LED with resistor | Cathode |
| GND | Button 1 | Bottom-right terminal |
| GND | Button 2 | Bottom-right terminal |
| GND | OLED display | GND |
| GND | Water pump | Negative terminal |
| GND | Relay module | GND |
| GND | Independent pump power supply | Negative terminal |

### Analog Signal Pins

| Board Pin | Module | Module Pin |
|:---:|:---:|:---:|
| A0 | Light sensor | AO |
| A1 | Soil moisture sensor | AOUT |
| A2 | Water level sensor | S |
| A4 | OLED display | SDA |
| A5 | OLED display | SCL |

### Digital Signal Pins

| Board Pin | Module | Module Pin |
|:---:|:---:|:---:|
| 2 | Button 1 | Top-left terminal |
| ~3 | Button 2 | Top-left terminal |
| 4 | LED | Anode |
| ~5 | Buzzer module | I/O |
| ~6 | Relay module | IN |

---

## Relay Pins

All modules listed in this section are connected to the development board through a breadboard.

### Control Circuit Side

| Pin | Connected Device | Connected Pin |
|:---:|:---:|:---:|
| VCC | Development board | 5V power |
| GND | Development board | GND |
| IN | Development board | Digital signal pin ~6 |

### Switched Circuit Side

| Pin | Connected Device | Connected Pin |
|:---:|:---:|:---:|
| NC | Not connected | / |
| COM | Development board | 5V power |
| NO | Water pump | Positive terminal |

**Translate By ChatGPT**