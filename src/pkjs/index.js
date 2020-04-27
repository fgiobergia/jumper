Pebble.addEventListener("ready", function(e) {
	console.log("Connected!");
});

Pebble.addEventListener("appmessage", function(e) {
	console.log("Message received");
	console.log(e.payload);
});
