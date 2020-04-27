Pebble.addEventListener("ready", function(e) {
	console.log("Connected!");
	Pebble.sendAppMessage({ "JSReady": 1 });
});

Pebble.addEventListener("appmessage", function(e) {
	console.log("Message received");
	console.log(JSON.stringify(e.payload));
});
