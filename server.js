const dgram = require('dgram');
const fs = require('fs');

// Port and IP for the server
const PORT = 3333;
const HOST = '192.168.1.103';

// Create the UDP socket
const server = dgram.createSocket('udp4');

// Start listening on the specified port and IP
server.on('listening', () => {
    const address = server.address();
    console.log(`UDP Server listening on ${address.address}:${address.port}`);
});

// On message received
server.on('message', (message, remote) => {
    console.log(`Received message from ${remote.address}:${remote.port} - ${message}`);
    
    // Append the message to a text file
    fs.appendFile('cat_status_log.txt', message + '\n', (err) => {
        if (err) {
            console.log('Error writing to file:', err);
        } else {
            console.log('Message saved to cat_status_log.txt');
        }
    });
});

// Bind server to port and IP
server.bind(PORT, HOST);
