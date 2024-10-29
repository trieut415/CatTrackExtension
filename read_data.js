const net = require('net');
const fs = require('fs');
const path = require('path');
const express = require('express');
const http = require('http');
const socketIo = require('socket.io');

// Define the IP addresses and ports of the ESP32 devices
const devices = [
    { host: '192.168.1.102', port: 3333 }, // ESP32 1
    { host: '192.168.1.101', port: 3334 }, // ESP32 2
    { host: '192.168.1.102', port: 3335 }, // ESP32 3
];

// Create an express app
const app = express();
const server = http.createServer(app);
const io = socketIo(server);

// Path to the CSV file
const logFilePath = path.join(__dirname, 'cat_data.csv');

// Function to log data to CSV
function logDataToFile(logEntry) {
    fs.appendFile(logFilePath, logEntry, (err) => {
        if (err) {
            console.error('Error writing to file', err);
        } else {
            console.log('Data written:', logEntry.trim());

            // Emit the new data immediately to connected clients
            readAndEmitData(); // Emit data after logging it
        }
    });
}

// Function to connect to an ESP32
function connectToDevice(device) {
    const client = new net.Socket();

    // Connect to the ESP32
    client.connect(device.port, device.host, () => {
        console.log(`Connected to ESP32 at ${device.host}:${device.port}`);
    });

    // Listen for data from the ESP32
    client.on('data', (data) => {
        const currentTime = new Date().toISOString();
        const logEntry = `${currentTime}, ${device.host}:${device.port}, ${data.toString().trim()}\n`; // Format log entry

        // Log to console and append to file
        console.log(`Received from ${device.host}:${device.port}: ${data.toString().trim()}`);
        logDataToFile(logEntry);
    });

    // Handle errors
    client.on('error', (err) => {
        console.error(`Connection error with ${device.host}:${device.port} - ${err.message}`);
    });

    // Handle connection close
    client.on('close', () => {
        console.log(`Connection closed to ${device.host}:${device.port}`);
    });
}

// Connect to all ESP32 devices
devices.forEach(device => connectToDevice(device));

// Function to read and emit data from the CSV file
const readAndEmitData = () => {
    fs.readFile(logFilePath, 'utf8', (err, data) => {
        if (err) {
            console.error('Error reading cat_data.csv:', err);
            io.emit('data', { error: 'Failed to load data.' });
            return;
        }

        const lines = data.trim().split('\n');
        const records = lines.map(line => {
            const [time, source, state] = line.split(',').map(s => s.trim());
            return { time, source, state };
        });

        // Group data by source
        const groupedData = records.reduce((acc, record) => {
            if (!acc[record.source]) {
                acc[record.source] = [];
            }
            acc[record.source].push(record);
            return acc;
        }, {});

        // Emit the grouped data to connected clients
        io.emit('data', groupedData);
    });
};

// WebSocket connection
io.on('connection', (socket) => {
    console.log('New client connected to data server');
    readAndEmitData(); // Emit data immediately when a new client connects

    socket.on('disconnect', () => {
        console.log('Client disconnected from data server');
    });
});

// Serve the public directory for static assets
app.use(express.static(path.join(__dirname, 'public')));

// Start the server
const PORT = 3001;
server.listen(PORT, () => {
    console.log(`Data server running on port ${PORT}`);
});

// Keep the process alive
process.on('SIGINT', () => {
    console.log('Exiting...');
    process.exit();
});
