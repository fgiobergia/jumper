const config = require("./config.json");

Pebble.addEventListener("ready", function(e) {
	console.log("Connected!");
	Pebble.sendAppMessage({ "JSReady": 1 });
});

let buffer = {};
Pebble.addEventListener("appmessage", function(e) {
	const timestampPrefix = e.payload.TimestampPrefix;

	// filter out TimestampPrefix (and other possible future keys)
	const keys = Object.keys(e.payload).filter(k => { 
		const key = parseInt(k);
		return !isNaN(key);
	});
	let dict = {};
	for (const key of keys) {
		dict[BigInt(key) | (BigInt(timestampPrefix) << BigInt(32))] = e.payload[key].split(",").map(k => parseInt(k));
	}

	buffer = Object.assign(buffer, dict);

	if (Object.keys(buffer).length >= config.bufferLength) {
        const req = new XMLHttpRequest();
        req.open("POST", config.apiEndpoint);
        req.onload = function() {
            console.log(this.status);
        };
        req.setRequestHeader("Content-Type", "application/json");
        req.send(JSON.stringify(buffer));
        buffer = {};
    }
});

