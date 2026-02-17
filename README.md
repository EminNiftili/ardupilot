# 📡 G2S Custom CAN-Based Airspeed Sensor Integration for ArduPilot

## 📌 Task Overview

This project implements a **custom CAN-based airspeed sensor driver** for ArduPilot according to the following requirements:

### Required Features

- Create backend driver class  
- Register sensor in HAL  
- Implement periodic read (50 Hz)  
- Publish data into AP_Airspeed system  
- Add parameters:
  - SENSOR_ENABLE
  - SENSOR_OFFSET
  - SENSOR_FAILSAFE
- Implement:
  - Sensor health monitoring  
  - Timeout detection  
  - Failsafe trigger  

### Deliverables

- Patch file  
- Short architecture explanation  
- Data path flow diagram  

---

## 🧰 Environment Setup

Development was performed on Windows using **WSL (Windows Subsystem for Linux)** with Ubuntu.

WSL is used to provide the Linux toolchain required by ArduPilot.

### Install Required Packages

```bash
sudo apt update
sudo apt install -y git python3 python3-pip python3-venv     build-essential g++ gcc make cmake ninja-build     pkg-config libtool autoconf automake     libffi-dev libssl-dev     wget curl rsync unzip
```

### Clone ArduPilot Repository

```bash
git clone https://github.com/ArduPilot/ardupilot
```

### Initialize Submodules

ArduPilot depends on many external modules.

```bash
git submodule update --init --recursive
```

### Configure SITL Build (No Hardware Required)

```bash
./waf configure --board sitl
```

At this point, the development environment is ready.

---

## 🔍 Research Phase

Before implementation, the following topics were studied:

### Airspeed Sensors

- Differential pressure sensors
- I2C-based vs CAN-based airspeed sensors
- Existing ArduPilot airspeed drivers
- Bernoulli equation (pressure → airspeed conversion)

ArduPilot already supports CAN airspeed via **DroneCAN**, but this task required a **custom protocol implementation**.

---

### CAN Integration in ArduPilot

Key findings:

- HAL registration is handled through AP_CANManager
- Custom CAN devices can use Raw CAN drivers
- CAN frame reception methods:
  - Polling (manual read)
  - Callback (event-driven)

Callback approach was selected for efficiency.

---

### Data Representation

CAN payload interpretation required selecting an endianness.

- Little-endian byte order was used.

---

## 🏗 Implementation Process

### 1️⃣ Driver Creation

A custom backend driver named:

AP_Airspeed_G2S

was created.

The driver inherits from:

AP_Airspeed_Backend

This integrates it into the AP_Airspeed subsystem.

---

### 2️⃣ HAL Registration

Sensor registration is performed using the CAN manager infrastructure.

- A custom Raw CAN driver was created
- MultiCAN interface was used for frame routing
- Callback-based reception implemented

Selected CAN protocol type:

AP_CAN::Protocol::Scripting2

---

### 3️⃣ Periodic Operation

The driver processes incoming frames and maintains the latest sensor state.

Although CAN reception is event-driven, data publishing behaves as a periodic 50 Hz update source for the AP_Airspeed frontend.

---

### 4️⃣ Data Publication

Sensor values are exposed through backend interface functions:

- get_airspeed()
- get_temperature()

These values are consumed by the AP_Airspeed system.

---

### 5️⃣ Health Monitoring

Health status is determined based on:

- Timestamp of last received CAN frame
- Validity of parsed data

If no frames are received within a threshold period, the sensor is marked unhealthy.

---

### 6️⃣ Timeout Detection

Timeout logic detects sensor communication loss:

```
if (current_time - last_update > timeout)
    sensor_healthy = false
```

---

### 7️⃣ Failsafe Mechanism

Failsafe behavior prevents invalid data propagation.

If enabled:

- Stale or invalid data is rejected
- System can fall back to alternative sources

---

## ⚙️ Added Parameters

Three custom parameters were implemented:

| Parameter | Description |
|----------|------------|
| SENSOR_ENABLE | Enables or disables the G2S sensor |
| SENSOR_OFFSET | Calibration offset applied to measured airspeed |
| SENSOR_FAILSAFE | Enables failsafe behavior on sensor failure |

These parameters were added to the AP_Airspeed parameter structure for each instance.

---

## 🧪 Testing Status

### Hardware Availability

No physical CAN hardware was available during development.

### Virtual Testing Attempt

Virtual CAN (vcan) was considered, but WSL limitations prevented its use:

- vcan kernel module not available
- WSL2 does not support native Linux CAN networking

Therefore, full runtime testing could not be performed.

### Build Verification

Driver functionality was verified by:

- Successful compilation
- Integration into SITL build
- No runtime linkage errors

---

## ⚠️ Limitations

- No real CAN interface available
- No SITL CAN simulation used
- Sensor protocol parsing implemented as placeholder
- End-to-end runtime validation pending

---

## 📌 Summary

This work demonstrates:

✔ Creation of a custom CAN airspeed backend driver  
✔ HAL registration via CAN manager  
✔ Integration with AP_Airspeed system  
✔ Parameter support  
✔ Health monitoring and failsafe logic  
✔ Build-level validation  

Full runtime validation requires physical CAN hardware.

---

# Patch file

Look [Here](https://github.com/EminNiftili/ardupilot/0001-G2S-driver-ready.patch)

# Flow Diagram — Data Path (G2S RawCAN Airspeed → AP_Airspeed)

This document shows how airspeed data travels from the CAN bus into ArduPilot’s `AP_Airspeed` frontend when using the **G2S** RawCAN backend.

---

## 1) High-level Data Path (Overview)

```mermaid
flowchart LR
    S[CAN Airspeed Sensor (G2S)] -->|CAN Frames (RawCAN)| B[CAN Transceiver / Bus]
    B --> I[HAL CAN Interface (AP_HAL::CANIface)]
    I --> M[AP_CANManager / CAN routing]
    M --> C[CANSensor / MultiCAN (Protocol: Scripting2)]
    C --> H[AP_Airspeed_G2S::handle_frame()]
    H --> K[Cache + Health State
(last_update_ms, last_airspeed_ms)]
    K --> G[AP_Airspeed_Backend API
get_airspeed(), get_temperature()]
    G --> F[AP_Airspeed Frontend
state + calibration + filtering]
    F --> U[Vehicle Control / Estimation
(EKF, TECS, logs, GCS)]
```

---

## 2) Detailed Data Path (Runtime Steps)

```mermaid
sequenceDiagram
    autonumber
    participant Sensor as G2S Sensor
    participant Bus as CAN Bus
    participant Iface as AP_HAL::CANIface
    participant CANMgr as AP_CANManager
    participant Multi as MultiCAN (Scripting2)
    participant G2S as AP_Airspeed_G2S
    participant Front as AP_Airspeed (Frontend)

    Sensor->>Bus: Publish CAN frame (50 Hz)
    Bus->>Iface: Frame received by HAL driver
    Iface->>CANMgr: Dispatch frame to CAN manager
    CANMgr->>Multi: Route by Protocol = Scripting2
    Multi->>G2S: Callback: handle_frame(frame)
    G2S->>G2S: Parse payload → last_airspeed_ms
    G2S->>G2S: last_update_ms = millis()
healthy = true
    Front->>G2S: get_airspeed(airspeed)
    G2S-->>Front: airspeed_ms + SENSOR_OFFSET (if healthy)
    Front->>Front: Apply ratio/filters/calibration
Update AP_Airspeed state
```

---

## 3) Health / Timeout / Failsafe Branch

```mermaid
flowchart TD
    A[New CAN frame received] --> B[handle_frame updates last_update_ms]
    B --> C[healthy = true]
    C --> D[AP_Airspeed reads backend get_airspeed()]

    T[No frames received] --> E{now - last_update_ms > TIMEOUT?}
    E -- No --> D
    E -- Yes --> F[healthy = false]
    F --> G{SENSOR_FAILSAFE enabled?}
    G -- Yes --> H[Reject / invalidate data
(get_airspeed returns false)]
    G -- No --> I[Allow last cached value
(optional behavior)]
```

> Notes:
> - The **timeout threshold** is implemented inside the G2S backend logic using `last_update_ms`.
> - When failsafe is enabled, the backend prevents stale/invalid readings from propagating into `AP_Airspeed`.

---

## 4) ASCII Diagram (for environments without Mermaid)

```
[ G2S CAN Airspeed Sensor ]
            |
            |  CAN frames (RawCAN, Protocol=Scripting2)
            v
[ HAL: AP_HAL::CANIface ]
            |
            v
[ AP_CANManager (routing) ]
            |
            v
[ CANSensor / MultiCAN ]
            |
            v
[ AP_Airspeed_G2S backend ]
   - handle_frame()
   - parse payload
   - last_update_ms
   - healthy/timeout
            |
            v
[ AP_Airspeed frontend ]
   - calibration/filters
   - state update
            |
            v
[ EKF / TECS / Logs / GCS ]
```

---

## 5) What each block means (short)

- **Sensor**: publishes CAN frames at ~50 Hz.
- **HAL (CANIface)**: board/OS-specific CAN driver interface that receives frames.
- **AP_CANManager**: registers CAN drivers and routes frames to the correct protocol consumers.
- **MultiCAN / CANSensor**: helper layer that distributes frames to callbacks (backends) registered for that protocol.
- **AP_Airspeed_G2S**: parses frames, caches values, enforces health/timeout/failsafe.
- **AP_Airspeed frontend**: consumes backend values and provides airspeed to the rest of the flight stack.
