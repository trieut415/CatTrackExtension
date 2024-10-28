# 1. Smart Cat Collar with Activity Monitoring

## 2. Authors
Group 6: Josh Bardwick, Noah Hathout, Cole Knutsen, Trieu Tran

## 3. Date
10 - 27 - 2024

## 4. Summary
The project involves designing and building a smart cat collar that captures, logs, and transmits activity data of cats in real-time. The primary focus is to read sensor data periodically, report it in meaningful states (e.g., "Sleepy Time", "Wander Time", "Moonwalk Time"), and display the results on an alphanumeric display as well as transmit data to a central server for leader determination among multiple cats.

### Desired Behaviors

1. **Activity Detection**
   - Use an accelerometer (ADXL343) to detect the cat's activity state: sleeping, wandering, or performing a "moonwalk".

2. **State Reporting**
   - Display the current activity state on a 14-segment alphanumeric display.
   - Report the state along with a timestamp to a central server via Wi-Fi.

3. **Leader Determination**
   - Participate in a system where the cat with the most active time over a rolling 10-minute window is designated as the leader.
   - Receive leader updates from the server and indicate changes via a buzzer.

4. **Button Interface**
   - Allow switching between different display modes using a button:
     - **Mode 0**: Display a default message ("Boots and Cats").
     - **Mode 1**: Display the current activity state.
     - **Mode 2**: Display the elapsed time in the current state.

5. **Buzzing Mechanism**
   - Buzz continuously if the cat is the leader.
   - Buzz once when there is a leader change.

### Solution Requirements

1. **Sensor Integration**
   - Utilize the ADXL343 accelerometer to determine the cat's activity state.

2. **Display Interface**
   - Use an alphanumeric display to show messages and activity states.

3. **Network Communication**
   - Connect to a Wi-Fi network and communicate with a central server via WebSocket and UDP.

4. **Concurrency**
   - Implement FreeRTOS tasks for handling sensor reading, display updates, button presses, and network communication.

5. **Synchronization**
   - Use mutexes to protect shared data among tasks.

## 5. Solution Design

1. **app_main**
   - Initialize Mutex.
   - Initialize I2C and UART.
   - Set up GPIOs for Button and Buzzer.
   - Connect to Wi-Fi.
   - Initialize Accelerometer.

2. **Create FreeRTOS Tasks**
   - **test_adxl343** (Sensor Task)
   - **task_button_presses** (Button Task)
   - **test_alpha_display** (Display Task)
   - **network_listener_task** (Network Listener Task)

3. **Initialize WebSocket Client**

4. **Task Reset Button (If Implemented)**
   - **Loop**: Poll Button.
   - If button pressed:
     - Reset activity timers and states.
     - Turn on an indicator LED (optional).
     - Log: "Reset button pressed."
   - Else: Continue polling.

5. **Task Tilt Button (If Implemented)**
   - **Loop**: Poll Tilt Sensor.
   - If tilt detected:
     - Toggle tilt orientation state.
   - Else: Continue polling.

6. **Sensor Task (test_adxl343)**
   - **Loop**:
     - Read accelerometer data.
     - Calculate roll, pitch, and acceleration values.
     - Determine the cat's activity state using `getCatState`.
     - Update the shared state and reset timers as needed.

7. **Button Task (task_button_presses)**
   - **Loop**:
     - Poll the button GPIO.
     - On button press, cycle through display modes (0, 1, 2).

8. **Display Task (test_alpha_display)**
   - **Loop**:
     - Based on the current display mode:
       - **Mode 0**: Display "Boots and Cats".
       - **Mode 1**: Display the current activity state.
       - **Mode 2**: Display the elapsed time in the current state.
     - Handle scrolling messages if the text exceeds display capacity.
     - Update the alphanumeric display via I2C.

9. **Network Listener Task (network_listener_task)**
   - **Loop**:
     - Listen for leader updates from the server via UDP.
     - On receiving a new leader ID:
       - Update the current leader ID.
       - Call the `buzz` function accordingly.

10. **WebSocket Event Handler (websocket_event_handler)**
    - **On Receiving Data**:
      - Parse the received leader ID.
      - Update `isBuzzing` based on whether this device is the leader.
      - Call the `buzz` function.

11. **Buzz Function (buzz)**
    - If `isBuzzing` is true:
      - Buzz continuously (on/off every 500ms).
    - Else If Leader Changed:
      - Buzz once to indicate leader change.
      - Update `current_leader_id` and `previous_leader_id`.
    - Else:
      - Ensure the buzzer is off.




## 6. Summary

### Potential Improvements
- **Hardware Interrupts**: Replace polling mechanisms with hardware interrupts for the button to reduce CPU usage.
- **Circuit Optimization**: Simplify the hardware setup to reduce the number of jumper wires and potential points of failure.
- **Error Handling**: Enhance error checking and handling, especially for network operations and I2C communication.
- **Power Management**: Implement power-saving features to extend battery life on the collar.

### Results
The smart cat collar successfully captures and reports activity data, including detecting when the cat is sleeping, wandering, or performing a "moonwalk." The activity state is displayed on the alphanumeric display, and data is transmitted to a central server. The device responds to leader updates by buzzing appropriately, and the button interface allows switching between display modes.

### Challenges
- **Concurrency Issues**: Ensuring thread-safe access to shared variables across multiple FreeRTOS tasks required careful use of mutexes.
- **Network Communication**: Managing WebSocket connections and handling leader updates in real-time presented challenges.
- **Sensor Calibration**: Fine-tuning the thresholds for activity states to accurately reflect the cat's behavior was time-consuming.

## 7. Artifacts
Everything that we needed for this quest was covered by the skills and we did not need to use external resources except code from ChatGPT that was present in our skills as well.

Link to report video:
- [Link to video demo](). 

Link to design demo:
- [Link to video demo]().

## 8. Self-assessment
## Rubric

| Objective Criterion | Rating | Max Value  | 
|---------------------------------------------|:-----------:|:---------:|
| Cat Trackers connected via WiFi | 1 |  1     | 
| Data from each (5) Cat Tracker sent to central server and aggregated | 1 |  1     | 
| Portal reports live leader status for each activity state for each cat and charts them on web site | 1 |  1     | 
| Central server reports live leader status back to Cat Tracker alpha displays and buzzer | 1 |  1     | 
| Portal accessible from open internet | N/A (Professor said it was ok due to double NAT) |  1     | 
| Web cam operational at the same client | 1 |  1     | 
| Node.js runs on pi | 1 |  1     | 

 ## 9. AI Code Assertions

All code is labeled appropriately "AI Generated".

