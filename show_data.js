const fs = require('fs');
const path = require('path');
const express = require('express');
const http = require('http');
const socketIo = require('socket.io');

// Create an express app
const app = express();
const server = http.createServer(app);
const io = socketIo(server);

// Path to the CSV file
const logFilePath = path.join(__dirname, 'cat_data.csv');

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

// Emit data every 5 seconds (adjust the interval as necessary)
setInterval(readAndEmitData, 5000);

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
