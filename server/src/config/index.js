import "dotenv/config";

// Validate that the configured port is a valid TCP port number
function parsePort(value) {
	const port = Number(value);

	if (!Number.isInteger(port) || port < 1 || port > 65535) {
		throw new Error("Invalid server port configuration.");
	}

	return port;
}

// Prevent accidental modification of the configuration object at runtime
const config = Object.freeze({
	nodeEnv: process.env.NODE_ENV ?? "development",
	host: process.env.HOST ?? "127.0.0.1",
	port: parsePort(process.env.PORT ?? "3000")
});

export default config;