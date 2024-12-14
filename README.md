# Smart Cat Collar with Activity Monitoring

**Date**: 2024-10-27  

## Overview

This project focuses on designing and building a smart cat collar that monitors and transmits real-time activity data. The collar uses sensors to detect activity states (e.g., "Sleepy Time", "Wander Time", "Moonwalk Time") and displays the results on an alphanumeric display. It also communicates with a central server for leader determination among multiple cats. This is a proof of concept, implemented on the ESP32, on the ESP-IDF framework.

---

## Features

### Activity Detection
- An ADXL343 accelerometer detects the cat's activity state: sleeping, wandering, or performing a "moonwalk."

### State Reporting
- The current activity state is displayed on a 14-segment alphanumeric display.  
- States and timestamps are transmitted to a central server via Wi-Fi.

### Leader Determination
- The system tracks active time over a rolling 10-minute window and designates the most active cat as the leader.  
- Leader updates are received from the server, and changes are indicated via a buzzer.

### Button Interface
- A button allows switching between display modes:  
  - **Mode 0**: Default message ("Boots and Cats").  
  - **Mode 1**: Current activity state.  
  - **Mode 2**: Elapsed time in the current state.

### Buzzing Mechanism
- The buzzer activates continuously when the cat is the leader.  
- A single buzz indicates a leader change.

---

## System Design

### Core Components
1. **Sensor Integration**  
   - ADXL343 accelerometer used to determine activity states.  

2. **Display Interface**  
   - A 14-segment alphanumeric display shows real-time activity updates.  

3. **Network Communication**  
   - Uses Wi-Fi for WebSocket and UDP communication with a central server.  

4. **Concurrency Management**  
   - FreeRTOS tasks handle sensor reading, display updates, button presses, and network communication.  
   - Mutexes ensure safe access to shared data.  

---

### Task Breakdown
1. **Initialization (`app_main`)**  
   - Sets up mutexes, I2C, UART, GPIOs (button, buzzer), Wi-Fi, and the accelerometer.  

2. **FreeRTOS Tasks**
   - **Sensor Task (`test_adxl343`)**  
     - Reads accelerometer data to calculate roll, pitch, and acceleration.  
     - Classifies activity states using `getCatState` and updates the shared state.  

   - **Button Task (`task_button_presses`)**  
     - Monitors button presses to cycle through display modes.  

   - **Display Task (`test_alpha_display`)**  
     - Updates the display based on the current mode:  
       - **Mode 0**: Default message.  
       - **Mode 1**: Current activity state.  
       - **Mode 2**: Elapsed time in the state.  

   - **Network Listener Task (`network_listener_task`)**  
     - Receives leader updates from the server via UDP.  
     - Updates the leader ID and triggers appropriate buzzer actions.

3. **WebSocket Event Handling**  
   - Listens for incoming leader data.  
   - Updates the buzzer status based on the current leader.  

4. **Buzz Functionality (`buzz`)**  
   - Continuous buzzing if the cat is the leader.  
   - A single buzz indicates a leader change.

---

### Supporting Scripts
- **`read_data.js`**: Reads sensor data and generates a `cat_data.txt` file.  
- **`host_data.js`**: Hosts the data, accessible via endpoints (`rpi_IP:3000/video`, `rpi_IP:3000/chart`).  

---

## Visuals

### Circuit Diagram
![Circuit Diagram](https://github.com/trieut415/CatTrackExtension/blob/main/CatCollarDiagram.jpg)
---

## Potential Improvements
- **Hardware Interrupts**: Replace polling mechanisms with interrupts to improve efficiency.  
- **Circuit Optimization**: Reduce hardware complexity to minimize potential points of failure.  
- **Error Handling**: Enhance error detection and recovery for network and I2C operations.  
- **Power Management**: Implement power-saving features to extend battery life.  

---

## Results

The smart cat collar successfully detects and reports activity states, including sleeping, wandering, and performing a "moonwalk." It displays activity data on the alphanumeric display and communicates updates to a central server. The buzzer alerts changes in leader status, and the button interface allows mode switching.

---

## Challenges
1. **Concurrency Management**  
   - Ensuring thread-safe shared data access with mutexes across FreeRTOS tasks.  

2. **Network Communication**  
   - Real-time WebSocket and UDP operations required careful synchronization.  

3. **Sensor Calibration**  
   - Tuning activity state thresholds for accurate detection required significant effort.  

---

## Demonstrations

- [Video Demo: Report](https://drive.google.com/file/d/1tmxS8HGcO_dwnGMNcW7fP4neTti4bQX-/view?usp=sharing)  
- [Video Demo: Design](https://drive.google.com/file/d/1KdEOFxNXSFyghgzOxwIRIRRMS36AR_G7/view?usp=sharing)  
