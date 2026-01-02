# IoT Experimental Framework

This repository provides an end-to-end experimental framework for evaluating data transmission strategies in satellite-inspired IoT conditions. The system emulates a low-connectivity sensing environment where an ESP32-based sensor node transmits measurements via a BLE + Ethernet gateway to a remote server connected through a Starlink backhaul.

The project was developed as part of a Bachelor's thesis titled **"Optimization of Energy Consumption and Reliability in an IoT Environment"** (in Slovak: *"Optimalizácia spotreby a spoľahlivosti v IoT prostredí*), written in Slovak at the Faculty of Informatics and Information Technologies, Slovak University of Technology in Bratislava.

The entire workflow, including configuration, firmware, experiment control, and data collection, is unified under a single, editable TOML configuration file.

---

## System Components

The project consists of four main components:

- **sensor_node/** – ESP32 firmware for the sensing device.
    
- **BLE_gateway/** – minimal ESP32 firmware for relaying packets between BLE and Ethernet.
    
- **control_station/** – Python script running on the operator machine, responsible for experiment control and data logging.
    
- **server/** – Python script running on a remote machine, responsible for receiving packets through Starlink and replying with acknowledgements.
    
- **config/** – global configuration folder with a single editable `config.toml` file.
    

Both firmware modules and both Python scripts consume the same shared configuration.

---

## Repository Structure

```
Satellite_IoT/
├── sensor_node/          # Sensor firmware (PlatformIO)
├── BLE_gateway/          # Gateway firmware (PlatformIO)
├── control_station/      # Local Python experiment controller
├── server/               # Remote Python packet receiver
├── config/               # Global TOML config, generator, workflow scripts
|	└── edit-config.sh    # Workflow script for editing project configuration
└── README.md
```

---

## Unified Configuration Workflow

All experiment parameters (timings, bucket definitions, agent selection, energy model, BLE parameters, etc.) are defined in:

```
config/config.toml
```

To edit the configuration and regenerate all outputs, use:

```
./config/edit-config.sh
```

This script performs two actions:

1. Opens `config.toml` using the user's preferred editor.
    
2. When the editor is closed, the script automatically runs `generate_config.py`.
    

The generator:

- Creates/updates `sensor_node/include/config.h`.
    
- Creates/updates `BLE_gateway/include/config.h`.
    
- Embeds configuration constants directly into `control_station.py`.
    
- Embeds config constants into `server.py`.
    

There are **no external config files** for Python scripts – both contain their configuration at the top, inside a protected auto-generated block.

### Dataset configuration

The control station uses a default synthetic dataset located at:
```
control_station/synthetic_light.csv
```

This file defines the input data used during the experiment.  If the user wishes to use a different dataset, the file can be replaced or modified directly, provided that the original CSV format and column structure are preserved.

---

## Hardware Requirements

- FIIT ESP32-S3 sensing kit (sensor node)
    
- ESP32 BLE + Ethernet gateway
    
- USB cable for flashing firmware
    
- Ethernet cable and network switch/router
    
- Starlink terminal (remote uplink)
    
- Local Linux machine (tested on Ubuntu 24.04 LTS)
    
- Remote Linux machine (physical or virtual)
    

---

## Software Requirements

- Python **3.12+**
    
- PlatformIO Core **6.1.18**
    
- Arduino framework for ESP32
    
- VS Code (recommended)
    
- Python packages listed in:
    
    - `control_station/requirements.txt`
        
    - `server/requirements.txt`
        
    - `config/requirements.txt`

---

## Building and Uploading Firmware

From `sensor_node/` or `BLE_gateway/`:

```
pio run --target upload
```

Ensure the ESP32 is connected over USB.

---

## Running the Python Components

Install dependencies:

```
pip install -r control_station/requirements.txt
pip install -r server/requirements.txt
```

Run both components:

```
python3 control_station/control_station.py
python3 server/server.py
```

Both scripts automatically use embedded configuration constants generated from `config.toml`.

---

## Agents

Agent implementations live in:

```
sensor_node/src/agents/
```

Supported policies include:

- Baseline heuristic agents
    
- Randomized agents
    
- Sleep-transmit agents
    
- Q-learning agents
    
- FORSEG agent
    

---

## Running an Experiment

1. Flash sensor and gateway firmware.
    
2. Start `server/server.py` on the remote machine.
    
3. Connect BLE gateway to Starlink uplink via Ethernet and power it on.
    
4. Connect sensor node to the local control machine.
    
5. Start `control_station/control_station.py`.
    
6. Observe incoming logs and generated CSV files.
    

---

## Output Format

All experiment outputs are saved to:

```
control_station/<experiment_name>/
```

where <experimet_name> can be specified in configuration.

The CSV files include:

- `AoII['<dimension_name>'].csv`
    
- `AoIrx['<dimension_name>'].csv`
    
- `AoItx['<dimension_name>'].csv`
    
- `energy_cost.csv`
    
- `Xproc['<dimension_name>'].csv`
    
- `Xtx['<dimension_name>'].csv`
    
- `Xrx['<dimension_name>'].csv`
    
where `<dimension_name>` is replaced by each dimension defined in `config.toml`.

---

## Reproducibility Notes

- Firmware determinism is ensured by `platformio.ini` + auto-generated `config.h`.
    
- Python reproducibility is maintained by pinned package versions.
    
- All configuration is centralized in `config.toml`.
    
- Wireless communication introduces inherent nondeterminism.
    

---

## Summary

This framework provides a fully integrated toolchain for satellite-inspired IoT experimentation. A single TOML file controls the behavior of all system components, ensuring consistency, reproducibility, and maintainability across the entire experimental stack. The workflow ensures consistency, reproducibility, and maintainability across all system components.
