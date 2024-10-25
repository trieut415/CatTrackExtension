const dgram = require('dgram');
const fs = require('fs');

// Ports and IP for the server
const PORT_1 = 3333;
const PORT_2 = 3334;
const HOST = '192.168.1.103';

// Function to create and set up a server for a specific port
function createServer(port) {
    const server = dgram.createSocket('udp4');

    server.on('listening', () => {
        const address = server.address();
        console.log(`UDP Server listening on ${address.address}:${address.port}`);
    });

    // On message received
    server.on('message', (message, remote) => {
        console.log(`Received message from ${remote.address}:${remote.port} - ${message}`);

        // Construct a log entry with a unique ID for each port
        const logEntry = `Port ${port} | ID ${Date.now()} | Message: ${message}\n`;

        // Append the message to a text file
        fs.appendFile('cat_status_log.txt', logEntry, (err) => {
            if (err) {
                console.log('Error writing to file:', err);
            } else {
                console.log(`Message saved to cat_status_log.txt from Port ${port}`);
            }
        });
    });

    // Bind server to the specified port and IP
    server.bind(port, HOST);
}

// Create servers for both ports
createServer(PORT_1);
createServer(PORT_2);