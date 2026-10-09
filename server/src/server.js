import app from "./app.js";
import config from "./config/index.js";

const server = app.listen(config.port, config.host, () => {
	console.log(`BaseQL backend running at http://${config.host}:${config.port}`);
});

server.on("error", (error) => {
	console.error("Unable to start BaseQL backend:", error);
	process.exitCode = 1;
});

let isShuttingDown = false;

// Gracefully shutdown the backend server
function shutdown(signal) {
	if (isShuttingDown) {
		return;
	}

	isShuttingDown = true;

	console.log(`Received ${signal}. Shutting down BaseQL backend...`);

	// Prevent shutdown from hanging indefinitely
	const shutdownTimeout = setTimeout(() => {
		console.error("Backend shutdown timed out.");
		process.exit(1);
	}, 10000);

	// Allow the process to exit before the Timeout's callback is invoked
	shutdownTimeout.unref();

	server.close((error) => {
		clearTimeout(shutdownTimeout);

		if (error) {
			console.error("Error during backend shutdown:", error);
			process.exitCode = 1;
		} else {
			console.log("BaseQL backend shut down gracefully.");
		}
	});
}

process.on("SIGINT", () => shutdown("SIGINT"));
process.on("SIGTERM", () => shutdown("SIGTERM"));