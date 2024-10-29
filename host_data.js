// server.js

const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const fs = require('fs');
const path = require('path');
const WebSocket = require('ws');
const app = express();
const server = http.createServer(app);
const io = socketIo(server);

// Serve static files from the current directory
app.use(express.static(__dirname));

// Route for video page
app.get('/video', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'video.html'));
});

// Route for chart page
app.get('/chart', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'chart.html'));
});

// Function to parse duration string into seconds
function parseDuration(durationStr) {
    const parts = durationStr.split(':').map(Number);
    let seconds = 0;
    if (parts.length === 3) {
        // HH:MM:SS
        seconds = parts[0] * 3600 + parts[1] * 60 + parts[2];
    } else if (parts.length === 2) {
        // MM:SS
        seconds = parts[0] * 60 + parts[1];
    } else if (parts.length === 1) {
        // SS
        seconds = parts[0];
    }
    return seconds;
}

// Function to compute the leader ID based on "Wander Time" and "Moonwalk Time"
function computeLeaderId(callback) {
    fs.readFile(path.join(__dirname, 'cat_status_log.txt'), 'utf8', (err, data) => {
        if (err) {
            console.error('Error reading cat_status_log.txt:', err);
            callback(null);
            return;
        }

        const lines = data.trim().split('\n');

        // Initialize data structure
        const catDurations = {
            '1': 0, // For cat on port 3333
            '2': 0, // For cat on port 3334
            '3': 0  // For cat on port 3335
        };

        // Process each line
        lines.forEach(line => {
            if (line.trim() === '') return;

            // Parse line
            const parts = line.split(' | ');
            let port = null;
            let content = null;

            parts.forEach(part => {
                if (part.startsWith('Port ')) {
                    port = part.substring('Port '.length).trim();
                } else if (part.startsWith('Message: ')) {
                    content = part.substring('Message: '.length).trim();
                }
            });

            if (port && content) {
                // Determine cat ID based on port
                let catId = null;
                if (port === '3333') {
                    catId = '1';
                } else if (port === '3334') {
                    catId = '2';
                } else if (port === '3335') {
                    catId = '3';
                } else {
                    console.error('Unknown port:', port);
                    return;
                }

                // Parse content to get duration and state
                let [durationStr, stateStr] = content.split(', Cat state:');
                const durationInSeconds = parseDuration(durationStr.trim());
                const state = stateStr ? stateStr.trim() : '';

                // Accumulate if state is "Wander Time" or "Moonwalk Time"
                if (state === 'Wander Time' || state === 'Moonwalk Time') {
                    catDurations[catId] += durationInSeconds;
                }
            } else {
                console.error('Invalid line format:', line);
            }
        });

        // Find the cat with the maximum duration
        let leaderId = null;
        let maxDuration = -1;
        for (let catId in catDurations) {
            if (catDurations[catId] > maxDuration) {
                maxDuration = catDurations[catId];
                leaderId = catId;
            }
        }

        callback(leaderId);
    });
}

// Set up the WebSocket server on the same HTTP server, listening on '/buzz'
const wss = new WebSocket.Server({
    server,
    path: '/buzz',
    perMessageDeflate: false // Disable permessage-deflate compression
});

wss.on('connection', (ws) => {
    console.log('A new client connected to /buzz!');

    // Handle messages received from the ESP32 client
    ws.on('message', (message) => {
        console.log('Received from ESP32:', message);

        // Process the message as needed
        // For example, you can save it to a file or broadcast to other clients
    });

    // Optionally send a welcome message to the client
    ws.send('Welcome to the /buzz WebSocket server!');
});

// Periodically compute and send the leader ID to connected clients
setInterval(() => {
    computeLeaderId((leaderId) => {
        if (leaderId) {
            console.log('Current leader ID:', leaderId);
            // Send leader ID to all connected clients in '/buzz'
            wss.clients.forEach((client) => {
                if (client.readyState === WebSocket.OPEN) {
                    client.send(leaderId);
                }
            });
        } else {
            console.error('Failed to compute leader ID');
        }
    });
}, 5000); // Send every 5 seconds
// Namespace for chart data
const chartNamespace = io.of('/chart');

chartNamespace.on('connection', (socket) => {
    console.log('New client connected to chart namespace');
    fs.readFile(path.join(__dirname, 'cat_status_log.txt'), 'utf8', (err, data) => {
        if (err) {
            console.error('Error reading cat_status_log.txt:', err);
            socket.emit('data', { error: 'Failed to load data.' });
            return;
        }

        const lines = data.trim().split('\n');

        // Initialize data structure
        const catStates = {
            '1': {}, // For cat on port 3333
            '2': {}, // For cat on port 3334
            '3': {}  // For cat on port 3335
        };

        // Process each line
        lines.forEach(line => {
            if (line.trim() === '') return;

            // Parse line
            // Expected format:
            // Port 3334 | ID 1729875494654 | Message: 00:03:01, Cat state: Wander Time

            const parts = line.split(' | ');
            let port = null;
            let id = null;
            let content = null;

            parts.forEach(part => {
                if (part.startsWith('Port ')) {
                    port = part.substring('Port '.length).trim();
                } else if (part.startsWith('ID ')) {
                    id = part.substring('ID '.length).trim();
                } else if (part.startsWith('Message: ')) {
                    content = part.substring('Message: '.length).trim();
                }
            });

            if (port && content) {
                // Determine cat ID based on port
                let catId = null;
                if (port === '3333') {
                    catId = '1';
                } else if (port === '3334') {
                    catId = '2';
                } else if (port === '3335') {
                    catId = '3';
                } else {
                    console.error('Unknown port:', port);
                    return;
                }
                // Parse content to get duration and state
                let [durationStr, stateStr] = content.split(', Cat state: ');
                const durationInSeconds = parseDuration(durationStr.trim());
                const state = stateStr ? stateStr.trim() : '';

                // Accumulate duration per state per cat
                if (!catStates[catId][state]) {
                    catStates[catId][state] = 0;
                }
                catStates[catId][state] += durationInSeconds;
            } else {
                console.error('Invalid line format:', line);
            }
        });

        // Emit the data to the client
        socket.emit('data', catStates);
    });

    socket.on('disconnect', () => {
        console.log('Client disconnected from chart namespace');
    });
});

// Namespace for video (if needed)
const videoNamespace = io.of('/video');

videoNamespace.on('connection', (socket) => {
    console.log('Client connected to video namespace');
    // Add any video-related Socket.io events if needed

    socket.on('disconnect', () => {
        console.log('Client disconnected from video namespace');
    });
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, '0.0.0.0', () => {
    console.log(`Server running on port ${PORT}`);
});

