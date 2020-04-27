Pebble.addEventListener("ready", function(e) {
	console.log("Connected!");
	Pebble.sendAppMessage({ "JSReady": 1 });
});

Pebble.addEventListener("appmessage", function(e) {
	console.log("Message received");
	const timestampPrefix = e.payload.TimestampPrefix;
	const keys = Object.keys(e.payload).filter(k => { // filter out TimestampPrefix (and other possible future keys)
		const key = parseInt(k);
		return !isNaN(key);
	});
	let dict = {};
	for (const key of keys) {
		dict[BigInt(key) | (BigInt(timestampPrefix) << BigInt(32))] = e.payload[key].split(",").map(k => parseInt(k));
	}

	const req = new XMLHttpRequest();
	req.open("POST", "http://192.168.43.65:5000/api/v1/accel/reading");
	req.onload = function() {
		console.log(this.status);
	};
	req.setRequestHeader("Content-Type", "application/json");
	req.send(JSON.stringify(dict));


});
