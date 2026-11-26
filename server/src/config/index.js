import "dotenv/config";
import { resolve } from "node:path";


// Validate that the configured port is a valid TCP port number
function parsePort(value) {
	const port = Number(value);

	if (!Number.isInteger(port) || port < 1 || port > 65535) {
		throw new Error("Invalid server port configuration.");
	}

	return port;
}

// Validate and resolve the configured DBMS executable path
function parseExecutablePath(value) {
	if (typeof value !== "string" || value.trim() === "") {
		throw new Error("Invalid DBMS executable path configuration.");
	}

	return resolve(value);
}

// Prevent accidental modification of the configuration object at runtime
const config = Object.freeze({
	nodeEnv: process.env.NODE_ENV ?? "development",
	host: process.env.HOST ?? "127.0.0.1",
	port: parsePort(process.env.PORT ?? "3000"),
	dbmsExecutablePath: parseExecutablePath(process.env.DBMS_EXECUTABLE_PATH)
});

export default config;