const dgram = require('dgram');
const express = require('express');
const socketIo = require('socket.io');
const path = require('path');

const PORT = 3333; // Same port as used in the C code
const HOST = '192.168.1.103'; // Raspberry Pi's IP address

// Create UDP socket
const udpServer = dgram.createSocket('udp4');

// Create HTTP server with Express
const app = express();
const httpServer = app.listen(3000, () => {
    console.log('HTTP server running on http://localhost:3000');
});
const io = socketIo(httpServer);

// Serve index.html
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'index.html'));
});

// Handle incoming UDP messages
udpServer.on('message', (msg, rinfo) => {
    console.log(`Received message: ${msg} from ${rinfo.address}:${rinfo.port}`);

    // Broadcast the received message to all connected WebSocket clients
    io.emit('data', { message: msg.toString() });
});

// Start listening for UDP messages
udpServer.bind(PORT, HOST, () => {
    console.log(`UDP server listening on ${HOST}:${PORT}`);
});

// Handle any UDP errors
udpServer.on('error', (err) => {
    console.error(`UDP server error:\n${err.stack}`);
    udpServer.close();
});
