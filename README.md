# HeatEx - Heat Exchanger Monitoring System

Qt6-based monitoring application with BACnet support for Raspberry Pi deployment.

## Prerequisites

- CMake 3.16 or higher
- Qt6 (Widgets, Charts, Svg)
- C++17 compatible compiler
- libqrencode-dev

### Ubuntu/Debian:
```bash
sudo apt install build-essential cmake qt6-base-dev qt6-charts-dev qt6-svg-dev libqrencode-dev
```

## Building

1. **Build BACnet Stack** (first time only):
```bash
cd bacnet-stack
mkdir -p build
cd build
cmake ..
make
cd ../..
```

2. **Build HeatEx**:
```bash
mkdir build
cd build
cmake ..
make
```

3. **Run**:
```bash
./bin/heatex
```

## For Raspberry Pi Embedded Deployment

See [EMBEDDED.md](EMBEDDED.md) for instructions on deploying to Raspberry Pi as a standalone embedded system.
