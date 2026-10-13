// Initialize arrays to store graphs titles and graphs objects
let graphs_titles = [];
let graphs = [];
let dbc_json;

// Initialize websocket connection to the server
const gateway = `ws://localhost:8765`;
let websocket;
window.addEventListener('load', onLoad);

/**
 * @brief Function to open WS connection to the server and register event handlers
 */
function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
    websocket.binaryType = "arraybuffer";

    // Close WebSocket when the page is refreshed or closed
    window.addEventListener("beforeunload", function () {
        if (websocket.readyState === WebSocket.OPEN) {
            websocket.close();
        }
    });
}

/**
 * @brief Function to log to console when WS connection is opened
 *
 * @param {*} event data of the event registered
 */
function onOpen(event) {
    console.log('Connection opened');
}

/**
 * @brief Function to handle closing WS connection. Restarts WS connection.
 *
 * @param {*} event data of the event registered
 */
function onClose(event) {
    console.log('Connection closed. Now reopening it...');
    initWebSocket();
}

/**
 * @brief Function to handle loading web page.
 *
 * @param {*} event data of the event registered
 */
function onLoad(event) {
    initWebSocket();
    loadJsonDBC();
    // setInterval(simulateServerMessage, 1000); // Testing purposes
}

async function fetchJsonDBC() {
    try {
        const response = await fetch('/VEHICLE_CAN.json');
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return await response.json();  // Parse and return JSON
    } catch (error) {
        console.error('Failed to fetch data:', error);
        return null;  // Return null in case of an error
    }
}

async function loadJsonDBC() {
    const data = await fetchJsonDBC();
    if (data) {
        dbc_json = data ? data.messages : null;
    }
}

// Function to find a message by its ID
function getMessageById(messages, id) {
    for (let message of messages) {
        if (message.id === id) {
            return message;  // Found the matching message
        }
    }
    return null;  // No match found
}

function multiplyBigIntSmart(bigIntValue, floatValue) {
    if (bigIntValue <= Number.MAX_SAFE_INTEGER && bigIntValue >= Number.MIN_SAFE_INTEGER) {
        return Number(bigIntValue) * floatValue; // Use normal multiplication
    } else {
        let precisionFactor = 1_000_000n; // Define precision factor (e.g., 1 million)
        let scaledFloat = BigInt(Math.round(floatValue * Number(precisionFactor)));
        return (bigIntValue * scaledFloat) / precisionFactor; // Scale down
    }
}

// Function to extract bits from the received 8-byte CAN message
function extractBits(data, startBit, bitLength, isBigEndian, isSigned) {
    let number = 0n; // Ensure this is a BigInt
    let mask = (BigInt(1) << BigInt(bitLength)) - BigInt(1);
    let shifted = 0n;

    if (isBigEndian) {
        for (let i = 0; i < 8; i++) {
            number |= BigInt(data[i]) << BigInt(8 * (7 - i));
        }
        let byte_num = (startBit + 1) >> 3;
        let offset = (startBit + 1) % 8;
        let shift = ((7 - byte_num) * 8 + offset);

        if (shift >= 64) {
            shifted = 0n;
        } else {
            shifted = (number >> BigInt(shift)) & mask;
        }
    } else {
        for (let i = 0; i < 8; i++) {
            number |= BigInt(data[i]) << BigInt(8 * i);
        }

        if (startBit >= 64) {
            shifted = 0n;
        } else {
            shifted = (number >> BigInt(startBit)) & mask;
        }
    }

    // Handle signed numbers (2's complement)
    if (isSigned && (shifted & (BigInt(1) << BigInt(bitLength - 1))) !== 0n) {
        shifted -= BigInt(1) << BigInt(bitLength);
    }

    return shifted;
}

function parseCanData(canMessage, canDataBuffer) {
    let parsedSignals = {};

    canMessage.signals.forEach(signal => {
        let {name, startbit, bitlength, endianess, scaling, offset, signed} = signal;

        // Extract raw value as BigInt
        let rawValue = extractBits(canDataBuffer, startbit, bitlength, endianess === "motorola", signed);

        // Apply scaling safely using multiplyBigIntSmart
        let scaledValue = multiplyBigIntSmart(rawValue, scaling);

        // Ensure `offset` has the same type as `scaledValue`
        if (typeof scaledValue === "bigint") {
            offset = BigInt(Math.round(offset)); // Convert to BigInt
        }

        parsedSignals[name] = scaledValue + offset; // Now safe to add
    });

    return parsedSignals;
}

// Function to handle the received TCP message (main entry point)
function onMessage(event) {
    const canMessages = dbc_json;
    if (!canMessages) {
        console.error("Failed to load CAN messages.");
        return;
    }

    // Assuming `event.data` contains the TCP message
    const tcpPacket = new Uint8Array(event.data);

    // The first 2 bytes are the CAN ID (big-endian, 16 bits)
    const canId = (tcpPacket[1] << 8) | tcpPacket[0]; // Extract CAN ID
    const canDlc = tcpPacket[2];

    // The remaining 8 bytes are the CAN message data
    const canDataBuffer = new Uint8Array(8); // all elements initialized to 0

    // Copy the data from tcpPacket (assumed to be a Uint8Array)
    for (let i = 0; i < canDlc; i++) {
        canDataBuffer[i] = tcpPacket[3 + i];
    }

    const time = (tcpPacket[canDlc + 3] << 24) | (tcpPacket[canDlc + 4] << 16) | (tcpPacket[canDlc + 5] << 8) | tcpPacket[canDlc + 6];

    // Find the corresponding CAN message in the JSON
    const canMessage = getMessageById(canMessages, canId);
    if (!canMessage) {
        console.log(`CAN message with ID ${canId} not found!`);
        return;
    }

    // Parse the CAN data and extract signal values
    const parsedData = parseCanData(canMessage, canDataBuffer);

    addDataToGraph(parsedData, time);
}

function msToTime(ms) {
    const totalSeconds = Math.floor(ms / 1000);
    const milliseconds = ms % 1000;
    const seconds = totalSeconds % 60;
    const minutes = Math.floor((totalSeconds / 60) % 60);
    const hours = Math.floor(totalSeconds / 3600);

    // Pad with zeroes if needed
    const pad = (n, z = 2) => ('00' + n).slice(-z);

    return `${pad(hours)}:${pad(minutes)}:${pad(seconds)}.${pad(milliseconds, 3)}`;
}

function addDataToGraph(jsonData, timeMs) {
    const tableBody = document.querySelector("#data-table tbody");
    let timeStamp = msToTime(timeMs);

    // Iterate through the JSON objects to register the datapoints
    for (let key in jsonData) {
        // If the graph for the current ID does not exist create one
        if (graphs_titles.indexOf(key) == -1) {
            addNewGraph(key);
        }

        // Find the corresponding graph of the JSON object
        let graph_index = graphs_titles.indexOf(key);

        const chartContainer = document.querySelector(`#${key}`).parentElement;
        // Check if graph exists (backup in case there are uncaught errors)
        if ((graph_index != -1) && (chartContainer.style.display === 'flex')) {
            // Only display the most recent 250 data points
            if (graphs[graph_index].data.datasets[0].data.length > 250) {
                graphs[graph_index].data.labels.shift();
                graphs[graph_index].data.datasets[0].data.shift();
            }
            // Add data point to the array storing data of the Chart.js object
            graphs[graph_index].data.labels.push(timeStamp);
            graphs[graph_index].data.datasets[0].data.push(jsonData[key]);
            // Update graph contents
            graphs[graph_index].update();
            
            let signalUnits = getSignalUnitsByName(dbc_json, key);
            key_units = key;
            if (signalUnits) {
                signalUnits = ` (${signalUnits})`;
                key_units += signalUnits;
            }
            // Update or create a table row for the key
            let existingRow = document.querySelector(`#data-table tbody tr[data-key="${key_units}"]`);
            let roundedVal = jsonData[key].toFixed(4)
            if (existingRow) {
                existingRow.querySelector(".timestamp").textContent = timeStamp;
                existingRow.querySelector(".value").textContent = roundedVal;
            } else {
                const newRow = document.createElement("tr");
                newRow.setAttribute("data-key", key_units);
                newRow.innerHTML = `
                    <td class="timestamp">${timeStamp}</td>
                    <td class="key">${key_units}</td>
                    <td class="value">${roundedVal}</td>
                `;
                tableBody.appendChild(newRow);
            }
        }
    }
}

/**
 * @brief Function to get the units of a signal by its name
 * 
 * @param {*} json json object containing the messages
 * @param {*} signalName signal name to search for
 * @returns units of the signal if found, null otherwise
*/
function getSignalUnitsByName(messages, signalName) {
    for (const message of messages) {
        for (const signal of message.signals) {
            if (signal.name === signalName) {
                return signal.units;
            }
        }
    }
    return null;
}

/**
 * @brief Function to add new graph to the WS client web page
 *
 * @param {*} id The name of the data being collected, to generate a graph title
 */
function addNewGraph(id) {
    // Update the array containing the graphs titles
    graphs_titles.push(id);

    // Add a new checkbox to the dropdown menu
    const dropdownContent = document.querySelector('.dropdown-content');
    const label = document.createElement('label');
    label.innerHTML = `<input type="checkbox" value="${id}"> ${id}`;
    dropdownContent.appendChild(label);

    // Add a new graph container
    const graphsContainer = document.querySelector('.graphs');
    const chartContainer = document.createElement('div');
    chartContainer.className = 'chart-container';
    const canvas = document.createElement('canvas');
    canvas.id = id;
    chartContainer.appendChild(canvas);
    graphsContainer.appendChild(chartContainer);

    // Get the corresponding index in the graph_titles array
    const graph_index = graphs_titles.indexOf(id);

    let signalUnits = getSignalUnitsByName(dbc_json, id);
    if (signalUnits) {
        signalUnits = ` (${signalUnits})`;
    }

    // Create the Chart.js object
    const ctx = document.getElementById(graphs_titles[graph_index]).getContext('2d');
    new_graph = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: graphs_titles[graph_index] + signalUnits,
                backgroundColor: 'rgba(75, 192, 192, 0.2)',
                borderColor: 'rgba(75, 192, 192, 1)',
                borderWidth: 1,
                data: []
            }]
        },
        options: {
            scales: {
                xAxes: [{
                    type: 'time',
                    time: {
                        displayFormats: {
                            millisecond: 'h:mm:ss.SSS a'
                        }
                    },
                    scaleLabel: {
                        display: true,
                        labelString: 'Time'
                    }
                }],
                yAxes: [{
                    scaleLabel: {
                        display: true,
                        labelString: 'Value'
                    }
                }]
            },
            animation: {
                duration: 0
            }
        }
    });

    // Add the new graph to the graphs array
    graphs.push(new_graph);

    // Add event listener to the new checkbox
    label.querySelector('input[type="checkbox"]').addEventListener('change', function () {

        const key = this.value;
        let signalUnits = getSignalUnitsByName(dbc_json, key);
        key_units = key;
        if (signalUnits) {
            signalUnits = ` (${signalUnits})`;
            key_units += signalUnits;
        }
        // Show/hide the corresponding graph
        const chartContainer = document.querySelector(`#${key}`).parentElement;
        chartContainer.style.display = this.checked ? 'flex' : 'none';

        // Remove table row if graph is unchecked
        const tableRow = document.querySelector(`#data-table tbody tr[data-key="${key_units}"]`);
        if (!this.checked && tableRow) {
            tableRow.remove();
        }


        if (this.checked) {
            chartContainer.style.display = 'flex';
        } else {
            chartContainer.style.display = 'none';
        }
    });
}

/**
 * @brief Function to simulate a WS message from server for testing purposes
 */
function simulateServerMessage() {
    const exampleEvent = {
        data: new Uint8Array([0x00, 0xB0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0xd0, 0x07]) // Example event data (TCP packet)
    };
    websocket.onmessage(exampleEvent);
}

document.getElementById('graphSearch').addEventListener('keyup', function() {
    let filter = this.value.toLowerCase();
    let labels = this.parentElement.querySelectorAll('label');
    labels.forEach(label => {
        if (label.textContent.toLowerCase().includes(filter)) {
            label.style.display = '';
        } else {
            label.style.display = 'none';
        }
    });
});

/**
 * @brief Event listener function for dropdown menu
 */
document.addEventListener('DOMContentLoaded', function () {
    // Retrieve containers from HTML file
    const dropdown = document.querySelector('.dropdown');
    const dropbtn = document.querySelector('.dropbtn');
    const checkboxes = document.querySelectorAll('.dropdown-content input[type="checkbox"]');
    const graphContainers = document.querySelectorAll('.chart-container');
    const dropdownContent = document.querySelector('.dropdown-content'); // <-- Add this line

    // Toggle dropdown when clicking the button
    dropdown.addEventListener('click', function (event) {
        event.stopPropagation(); // Prevents immediate closing
        dropdown.classList.toggle('show');
    });

    // Close dropdown when clicking outside
    document.addEventListener('click', function (event) {
        if (!dropdown.contains(event.target)) {
            dropdown.classList.remove('show');
        }
    });

    // Ensure tapping inside the dropdown (on touchscreens) doesn't close it
    dropdownContent.addEventListener('click', function (event) {
        event.stopPropagation(); // Keeps dropdown open when clicking inside
    });

    // Display the graph for the checkboxes clicked
    checkboxes.forEach(checkbox => {
        checkbox.addEventListener('change', function () {

            const key = this.value;

            // Show/hide the corresponding graph
            const chartContainer = document.querySelector(`#${key}`).parentElement;
            chartContainer.style.display = this.checked ? 'flex' : 'none';

            // Remove table row if graph is unchecked
            const tableRow = document.querySelector(`#data-table tbody tr[data-key="${key}"]`);
            if (!this.checked && tableRow) {
                tableRow.remove();
            }


            const selectedGraphs = Array.from(checkboxes)
                .filter(checkbox => checkbox.checked)
                .map(checkbox => checkbox.value);

            graphContainers.forEach(container => container.style.display = 'none');

            const selectedGraphContainers = Array.from(graphContainers).filter(container =>
                selectedGraphs.includes(container.querySelector('canvas').id)
            );

            console.log(selectedGraphContainers);

            selectedGraphContainers.forEach(container => {
                container.style.display = 'flex';
            });
        });
    });
});
