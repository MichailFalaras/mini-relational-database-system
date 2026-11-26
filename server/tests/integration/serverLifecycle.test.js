import { describe, it, expect, afterEach } from "vitest";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { createServer } from "node:net";


const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

/* ---------- BACKEND PATHS ---------- */

const BACKEND_PATH = resolve(__dirname, "../..");
const SERVER_PATH = resolve(BACKEND_PATH, "src/server.js");
const FIXTURES_PATH = resolve(BACKEND_PATH, "tests/fixtures/mock_dbms");

/* ---------- MOCK EXECUTABLES PATHS ---------- */

const EXECUTABLE_EXTENSION = process.platform === "win32" ? ".exe" : "";
const MOCK_EXECUTABLE = resolve(FIXTURES_PATH, `mock_dbms${EXECUTABLE_EXTENSION}`);
const INVALID_READY_EXECUTABLE = resolve(FIXTURES_PATH, `mock_dbms_invalid_ready${EXECUTABLE_EXTENSION}`);
const STARTUP_TIMEOUT_EXECUTABLE = resolve(FIXTURES_PATH, `mock_dbms_startup_timeout${EXECUTABLE_EXTENSION}`);
const IGNORE_EOF_EXECUTABLE = resolve(FIXTURES_PATH, `mock_dbms_ignore_eof${EXECUTABLE_EXTENSION}`);


/* ---------- SERVER PORT-RELATED HELPERS ---------- */

// Create a server on a machine-selected available port
function occupyPort() {
	return new Promise((resolve, reject) => {
		const server = createServer();

		server.on("error", reject);

		server.listen(0, "127.0.0.1", () => {
			resolve({
				server,
				port: server.address().port
			});
		});
	});
}

// Release port by stopping the server
function releasePort(server) {
	return new Promise((resolve, reject) => {
		server.close((error) => {
			if (error && error.code !== "ERR_SERVER_NOT_RUNNING") {
				reject(error);
			}
			else {
				resolve();
			}
		});
	});
}

// Temporarily instantiate a server to retrieve its port number
async function getAvailablePort() {
	const { server, port } = await occupyPort();

	await releasePort(server);

	return port;
}

/* ---------- BACKEND-RELATED HELPERS ---------- */

let backends = [];

// Create a backend server
async function createBackend(executablePath = MOCK_EXECUTABLE, port = null) {
	const backendPort = port ?? await getAvailablePort();

	const child = spawn(process.execPath, [SERVER_PATH], {
		cwd: BACKEND_PATH,
		env: {
			...process.env,
			NODE_ENV: "test",
			HOST: "127.0.0.1",
			PORT: String(backendPort),
			DBMS_EXECUTABLE_PATH: executablePath
		},
		stdio: ["ignore", "pipe", "pipe", "ipc"],
		windowsHide: true
	});

	const backend = {
		child,
		port: backendPort,
		dbmsPid: null,
		stdout: "",
		stderr: "",
		messages: [],
		closed: false,
		exitPromise: null
	};

	// Make the output streams produce readable outputs 
	child.stdout.setEncoding("utf8");
	child.stderr.setEncoding("utf8");

	// Collect output chunks from the output streams
	child.stdout.on("data", (chunk) => {
		backend.stdout += chunk;
	});

	child.stderr.on("data", (chunk) => {
		backend.stderr += chunk;
	});

	// Receive IPC messages from the Node.js backend and capture the DBMS PID
	child.on("message", (message) => {
		backend.messages.push(message);

		if (message.type === "dbms-spawned") {
			backend.dbmsPid = message.pid;
		}
	});

	backend.exitPromise = new Promise((resolve) => {
		let spawnError = null;

		child.on("error", (error) => {
			spawnError = error;
		});

		child.once("close", (code, signal) => {
			backend.closed = true;

			resolve({
				code,
				signal,
				error: spawnError
			});
		});
	});

	backends.push(backend);

	return backend;
}

// Wait for an IPC message from the Node.js backend to the test process
function waitForMessage(backend, type, timeout = 5000) {
	const existingMessage = backend.messages.find((msg) => msg.type === type);

	// Message already there
	if (existingMessage) {
		return Promise.resolve(existingMessage);
	}

	// Backend closed before the message arrived
	if (backend.closed) {
		return Promise.reject(new Error(`Backend exited before sending ${type}`));
	}

	return new Promise((resolve, reject) => {
		function cleanup() {
			clearTimeout(timer);

			backend.child.off("message", onMessage);
			backend.child.off("close", onClose);
		}

		function onMessage(message) {
			if (message.type !== type) {
				return;
			}

			cleanup();
			resolve(message);
		}

		function onClose() {
			cleanup();

			reject(new Error(`Backend exited before sending ${type}\n` + backend.stderr));
		}

		const timer = setTimeout(() => {
			cleanup();

			reject(new Error(`Timed out waiting for ${type}`));
		}, timeout);

		backend.child.on("message", onMessage);
		backend.child.on("close", onClose);
	});
}

// Wait until the Node.js backend has terminated
// while preventing the test from waiting indefinitely
async function waitForBackendExit(backend, timeout = 5000) {
	let timer;

	try {
		return await Promise.race([
			backend.exitPromise,

			new Promise((_, reject) => {
				timer = setTimeout(() => {
					reject(new Error("Backend exit timed out"));
				}, timeout);
			})
		]);
	} finally {
		clearTimeout(timer);
	}
}

// Request that the backend initiates the normal shutdown workflow 
// without forcibly terminating the Node.js process
function requestBackendShutdown(backend) {
	return new Promise((resolve, reject) => {
		if (!backend.child.connected) {
			reject(new Error("Backend IPC channel is unavailable"));
			return;
		}
		
		backend.child.send({ type: "shutdown" }, (error) => {
			if (error) {
				reject(error);
			}
			else {
				resolve();
			}
		});
	});
}

// Verify whether an OS process with a given PID still exists
function isProcessRunning(pid) {
	if (!Number.isInteger(pid) || pid <= 0) {
		throw new Error("Invalid process PID");
	}

	try {
		// Signal 0 -> Process existence and permission check
		process.kill(pid, 0);

		return true;
	} catch (error) {
		// No such process
		if (error.code === "ESRCH") {
			return false;
		}

		// Process exists but permission denied
		if (error.code == "EPERM") {
			return true;
		}

		throw error;
	}
}

// Ensure test failures don't leave backend or DBMS processes running on the computer
async function cleanupBackend(backend) {
	try {
		if (!backend.closed) {
			try {
				await requestBackendShutdown(backend);
				await waitForBackendExit(backend, 15000);
			} catch {
				backend.child.kill("SIGKILL");

				await waitForBackendExit(backend, 5000);
			}
		}
	} finally {
		if (backend.dbmsPid !== null && isProcessRunning(backend.dbmsPid)) {
			try {
				process.kill(backend.dbmsPid, "SIGKILL");
			} catch (error) {
				if (error.code !== "ESRCH") {
					throw error;
				}
			}
		}
	}
}

describe("Backend Lifecycle", () => {
	afterEach(async () => {
		try {
			await Promise.all(backends.map(cleanupBackend));
		} finally {
			backends = [];
		}
	}, 30000);

    /* ---------- BACKEND STARTUP ---------- */

    it("Starts the backend after the DBMS becomes ready", async () => {
		const backend = await createBackend();

		const spawned = await waitForMessage(backend, "dbms-spawned");
		const ready = await waitForMessage(backend, "backend-ready");

		expect(spawned.pid).toBeGreaterThan(0);
		expect(isProcessRunning(spawned.pid)).toBe(true);

		expect(ready.port).toBe(backend.port);

		expect(backend.messages.map((msg) => msg.type)).toEqual(["dbms-spawned", "backend-ready"]);
		
		expect(backend.stdout).toContain("BaseQL DBMS is ready.");

		// Verify the test endpoint
		const response = await fetch(`http://127.0.0.1:${backend.port}/api/test`,
			{
				method: "POST",
				headers: {
					"Content-Type": "application/json"
				},
				body: JSON.stringify({ test: true }),
				signal: AbortSignal.timeout(3000)
			}
		);

		expect(response.status).toBe(200);

		const body = await response.json();

		expect(body).toEqual({
			message: "JSON received successfully.",
			data: {
				test: true
			}
		});
	}, 10000);

    it("Rejects backend startup when the executable does not exist", async () => {
		const backend = await createBackend(resolve(FIXTURES_PATH, "nonexistent-dbms.exe"));

		const result = await waitForBackendExit(backend);

		expect(result.error).toBeNull();
		expect(result.code).toBe(1);
		expect(result.signal).toBeNull();

		expect(backend.dbmsPid).toBeNull();

		expect(backend.messages.some((msg) => msg.type === "backend-ready")).toBe(false);

		expect(backend.stderr).toContain("Unable to start BaseQL backend:");
	}, 10000);

    it("Cleans up the DBMS when initialization fails", async () => {
		const backend = await createBackend(INVALID_READY_EXECUTABLE);

		const spawned = await waitForMessage(backend, "dbms-spawned");

    	const result = await waitForBackendExit(backend);

		expect(result.error).toBeNull();
		expect(result.code).toBe(1);
		expect(result.signal).toBeNull();

		expect(backend.messages.some((msg) => msg.type === "backend-ready")).toBe(false);

		expect(backend.stderr).toContain("Invalid READY message");

		expect(isProcessRunning(spawned.pid)).toBe(false);
	}, 10000);

    it("Cleans up the DBMS when HTTP server startup fails", async () => {
		const reservation = await occupyPort();

		try {
			const backend = await createBackend(MOCK_EXECUTABLE, reservation.port);

			const spawned = await waitForMessage(backend, "dbms-spawned");
			
			const result = await waitForBackendExit(backend, 15000);

			expect(result.error).toBeNull();
			expect(result.code).toBe(1);
			expect(result.signal).toBeNull();

			expect(backend.stdout).toContain("BaseQL DBMS is ready.");

			expect(backend.stderr).toContain("EADDRINUSE");
			
			expect(backend.stdout).toContain("BaseQL DBMS shut down gracefully.");

			expect(backend.messages.some((message) => message.type === "backend-ready")).toBe(false);

			expect(isProcessRunning(spawned.pid)).toBe(false);
		} finally {
			await releasePort(reservation.server);
		}
	}, 20000);


    /* ---------- BACKEND SHUTDOWN ---------- */

    it("Gracefully shuts down the backend and DBMS", async () => {
		const backend = await createBackend();

		await waitForMessage(backend, "backend-ready");

		const dbmsPid = backend.dbmsPid;

		expect(isProcessRunning(dbmsPid)).toBe(true);

		await requestBackendShutdown(backend);

		const result = await waitForBackendExit(backend, 15000);

		expect(result.error).toBeNull();
		expect(result.code).toBe(0);
		expect(result.signal).toBeNull();

		expect(backend.stdout).toContain("BaseQL DBMS shut down gracefully.");
		expect(backend.stdout).toContain("BaseQL backend shut down gracefully.");
		
		expect(isProcessRunning(dbmsPid)).toBe(false);
	}, 20000);

    it("Handles shutdown during DBMS startup", async () => {
		const backend = await createBackend(STARTUP_TIMEOUT_EXECUTABLE);

		const spawned = await waitForMessage(backend, "dbms-spawned");

		expect(isProcessRunning(spawned.pid)).toBe(true);

    	await requestBackendShutdown(backend);

    	const result = await waitForBackendExit(backend, 10000);

		expect(result.error).toBeNull();
		expect(result.code).toBe(1);
		expect(result.signal).toBeNull();

		expect(backend.messages.some((message) => message.type === "backend-ready")).toBe(false);

		expect(backend.stderr).not.toContain("ReferenceError");
		expect(backend.stderr).not.toContain("TypeError");

		expect(isProcessRunning(spawned.pid)).toBe(false);
	}, 15000);

    it("Handles multiple shutdown requests safely", async () => {
		const backend = await createBackend();

		await waitForMessage(backend, "backend-ready");

		const dbmsPid = backend.dbmsPid;

		await Promise.all([
			requestBackendShutdown(backend),
			requestBackendShutdown(backend),
			requestBackendShutdown(backend)
		]);

		const result = await waitForBackendExit(backend, 15000);

		expect(result.error).toBeNull();
		expect(result.code).toBe(0);
		expect(result.signal).toBeNull();

		const shutdownMessages = backend.stdout.match(/Shutting down BaseQL backend/g) ?? [];

		expect(shutdownMessages).toHaveLength(1);

		expect(isProcessRunning(dbmsPid)).toBe(false);
	}, 20000);


    /* ---------- FAILURE AND EMERGENCY CLEANUP ---------- */

    it("Forcibly terminates the DBMS when graceful shutdown times out", async () => {
		const backend = await createBackend(IGNORE_EOF_EXECUTABLE);

		await waitForMessage(backend, "backend-ready");

		const dbmsPid = backend.dbmsPid;

		expect(isProcessRunning(dbmsPid)).toBe(true);

		await requestBackendShutdown(backend);

		const result = await waitForBackendExit(backend, 20000);

		expect(result.error).toBeNull();
		expect(result.code).toBe(1);
		expect(result.signal).toBeNull();

		expect(backend.stderr).toContain("DBMS shutdown timed out");

		expect(isProcessRunning(dbmsPid)).toBe(false);
	}, 25000);

    it("Remains operational and shuts down safely after unexpected DBMS termination", async () => {
		const backend = await createBackend();

		await waitForMessage(
			backend,
			"backend-ready"
		);

		const dbmsPid = backend.dbmsPid;

		expect(isProcessRunning(dbmsPid)).toBe(true);

		// Simulate unexpected DBMS termination
		process.kill(dbmsPid, "SIGKILL");

		const closed = await waitForMessage(backend, "dbms-closed");

		expect(closed.state).toBe("failed");

		expect(isProcessRunning(dbmsPid)).toBe(false);

		expect(backend.closed).toBe(false);

		// Verify the HTTP backend remains operational
		const response = await fetch(
			`http://127.0.0.1:${backend.port}/api/test`,
			{
				method: "POST",
				headers: {
					"Content-Type": "application/json"
				},
				body: JSON.stringify({ test: true }),
				signal: AbortSignal.timeout(3000)
			}
		);

		expect(response.status).toBe(200);

		await requestBackendShutdown(backend);

		const result = await waitForBackendExit(backend, 10000);

		expect(result.error).toBeNull();
		expect(result.code).toBe(0);
		expect(result.signal).toBeNull();

		expect(isProcessRunning(dbmsPid)).toBe(false);
	}, 15000);
});