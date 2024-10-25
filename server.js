const dgram = require('dgram');
const fs = require('fs');

// Server IP and Port
const SERVER_PORT = 3333; // Port to receive messages from Cat Collars
const HOST = '192.168.1.103'; // Replace with your server's IP address

// Create a UDP socket
const server = dgram.createSocket('udp4');

// Store Cat Collar addresses
let catCollars = [];

// Function to send leader change notification
function sendLeaderChangeNotification() {
    const message = 'leader';
    const buffer = Buffer.from(message);

    catCollars.forEach(collar => {
        server.send(buffer, 0, buffer.length, collar.port, collar.address, (err) => {
            if (err) {
                console.log(`Error sending message to ${collar.address}:${collar.port} - ${err}`);
            } else {
                console.log(`Leader change notification sent to ${collar.address}:${collar.port}`);
            }
        });
    });
}

// On receiving messages from Cat Collars
server.on('message', (message, remote) => {
    console.log(`Received message from ${remote.address}:${remote.port} - ${message}`);

    // Add the Cat Collar to the list if not already present
    const existingCollar = catCollars.find(collar => collar.address === remote.address && collar.port === remote.port);
    if (!existingCollar) {
        catCollars.push({ address: remote.address, port: remote.port });
        console.log(`Added Cat Collar at ${remote.address}:${remote.port}`);
    }

    // Log the message
    const logEntry = `From ${remote.address}:${remote.port} - ${message}\n`;
    fs.appendFile('cat_status_log.txt', logEntry, (err) => {
        if (err) {
            console.log('Error writing to file:', err);
        } else {
            console.log(`Message saved to cat_status_log.txt`);
        }
    });

    // Trigger leader change notification (simplified)
    // Call this function whenever there's a leader change in your application logic
    sendLeaderChangeNotification();
});

// Start the server to listen for incoming messages
server.bind(SERVER_PORT, HOST, () => {
    console.log(`Server listening on ${HOST}:${SERVER_PORT}`);
});