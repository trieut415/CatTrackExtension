# FitCat: A Networked Cat Activity Tracking System

**Date**: October 27, 2024  

---

## Overview

The **FitCat** project extends the functionality of the Smart Cat Collar by creating a centralized portal to aggregate and display data from multiple cat trackers in real time. This system collects activity data via Wi-Fi, identifies the most active cat using a rolling leaderboard, and provides live updates to users through a web interface. FitCat also features a live video stream from a camera for monitoring the cats’ activities.

This project combines IoT networking, sensor integration, and web development to create an innovative and interactive platform for tracking and comparing feline behavior.

---

## Key Features

1. **Cat Tracker Network**
   - Each Cat Tracker device transmits activity data (time, temperature, activity state) via Wi-Fi to a central server.
   - Devices are implemented using ESP32 microcontrollers.

2. **Leader Determination**
   - The system calculates the most active cat based on cumulative active and highly active time over a rolling 10-minute window.
   - Leader updates are sent back to all Cat Trackers, triggering a buzzer alert on leader change.

3. **Web Portal**
   - A Node.js-based server aggregates data from all Cat Trackers.
   - Displays activity data and leader status in real-time strip charts.
   - Accessible from any browser over the open internet.

4. **Live Video Stream**
   - Streams video from a Raspberry Pi camera module, allowing owners to monitor their cats in real-time.

5. **Event Notifications**
   - Buzzer alerts on all devices when the leader changes.
   - Alpha-numeric displays on Cat Trackers show the leader status and real-time activity data.

---

## System Design

### Architecture

The FitCat system consists of three core components:
1. **Cat Tracker Devices**
   - Each device collects activity data using an accelerometer and transmits it via Wi-Fi.
   - Displays activity states and leader updates on an alphanumeric display.
   - Provides buzzer feedback for leader changes.

2. **Central Server**
   - A Node.js server aggregates data from all Cat Trackers.
   - Calculates leader status and broadcasts updates back to devices.
   - Hosts a web portal accessible on the open internet.

3. **Web Portal**
   - Displays live leaderboards and activity data in strip charts.
   - Streams live video from a Raspberry Pi camera.

---

### Core Tasks

#### 1. **Cat Tracker Devices**
- **Data Acquisition**
  - Reads acceleration data from an ADXL343 accelerometer to classify activity as:
    - **Highly Active**: Intense movement.
    - **Active**: Moderate movement.
    - **Inactive**: Minimal or no movement.
  - Tracks temperature and timestamps each state.
- **Wi-Fi Communication**
  - Sends activity data to the central server via UDP/WebSocket.
  - Receives leader updates from the server.
- **Display Updates**
  - Alpha-numeric display shows real-time activity states, temperature, and leader status.
- **Buzzer Alerts**
  - Continuous buzzing indicates the device’s cat is the current leader.
  - A single buzz alerts to a leader change.

#### 2. **Central Server**
- **Data Aggregation**
  - Collects activity data from all Cat Trackers and calculates leader status.
- **Web Portal**
  - Displays:
    - Leaderboards for activity levels.
    - Real-time strip charts for time, temperature, and activity states.
  - Streams video from the Raspberry Pi camera.
- **Leader Updates**
  - Broadcasts leader status to all connected Cat Trackers.

#### 3. **Web Portal**
- Built on Node.js with WebSocket and HTTP servers.
- Features:
  - **Live Leaderboard**: Updates every 10 minutes.
  - **Strip Charts**: Real-time plots for activity, temperature, and timestamps.
  - **Live Video Feed**: Streams directly from a Raspberry Pi camera module.

---

### Networking and Communication

1. **ESP32 Setup**
   - Configured as Wi-Fi stations using static IPs.
   - Communicate with the server via UDP/WebSocket protocols.

2. **Node.js Server**
   - Runs on a Raspberry Pi with a static IP assigned by the router.
   - Utilizes DDNS and port forwarding to ensure accessibility from external networks.

3. **Camera Stream**
   - The Raspberry Pi streams video using a dedicated HTTP server.

---

## Results and Achievements

### Achievements
- Successfully established Wi-Fi communication between multiple Cat Trackers and a central server.
- Real-time aggregation and visualization of activity data on a web portal.
- Accurate leader determination using cumulative activity data.
- Seamless integration of live video streaming into the portal.

### Challenges
1. **Network Configuration**
   - Setting up static IPs and port forwarding across multiple devices required careful debugging.
2. **Concurrency Management**
   - Managing multiple UDP and WebSocket connections without data loss.
3. **Leader Calculation**
   - Tuning thresholds and rolling window calculations to ensure accurate and fair leader determination.

---

## Demonstrations

### Videos
- [System Demo](https://drive.google.com/file/d/1tmxS8HGcO_dwnGMNcW7fP4neTti4bQX-/view?usp=sharing)  
- [Design Walkthrough](https://drive.google.com/file/d/1KdEOFxNXSFyghgzOxwIRIRRMS36AR_G7/view?usp=sharing)  

### Visuals
#### System Architecture Diagram
![Architecture Diagram](https://github.com/trieut415/CatTrackExtension/blob/main/FitCatArchitecture.jpg)

---

## Potential Improvements

1. **Hardware Optimization**
   - Replace jumper wires with a PCB for improved durability.
   - Add power-saving features for battery-operated Cat Trackers.

2. **Data Visualization**
   - Enhance the web portal with interactive charts and filtering options.

3. **Scalability**
   - Optimize the server to handle more Cat Trackers simultaneously.

4. **Real-Time Alerts**
   - Add push notifications or email alerts for significant activity changes.

---

## Conclusion

The **FitCat** system demonstrates the potential of IoT and cloud-connected devices for real-time activity monitoring and data visualization. By integrating sensors, Wi-Fi networking, and a web-based interface, this project successfully tracks and compares cat activity in a dynamic and engaging way. Future improvements will focus on scalability, user experience, and hardware optimization, making FitCat a robust platform for animal behavior analysis.
