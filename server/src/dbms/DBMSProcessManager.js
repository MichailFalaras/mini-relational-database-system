import { spawn } from "node:child_process";
import { Buffer } from "node:buffer";
import { isValidResponse, isValidReadyMessage, isValidRequest } from "./dbmsProtocol.js";


// All states the C DBMS Process Manager can be in
const DBMS_STATES = Object.freeze({
	STOPPED: "stopped",
	STARTING: "starting",
	READY: "ready",
	STOPPING: "stopping",
	FAILED: "failed"
});

const MAX_FRAME_SIZE = 8 * 1024 * 1024;

// Process Manager Definition
export class DBMSProcessManager {

	constructor(executablePath) {
		this.executablePath = executablePath;
		this.child = null;
		this.state = DBMS_STATES.STOPPED;
		this.pendingStartup = null;
		this.pendingRequest = null;
		this.pendingShutdown = null;
	}

	// Initiate DBMS child process
	async start() {
		if (this.child !== null) {
			throw new Error("DBMS process already exists");
		}

		const child = spawn(this.executablePath, [], {
			stdio: ["pipe", "pipe", "pipe"],
			windowsHide: true
		});

		this.child = child;
		this.state = DBMS_STATES.STARTING;

		const startupPromise = new Promise((resolve, reject) => {
			this.pendingStartup = {
				resolve,
				reject,
				timeout: null
			};
		});

		// The protocol specifies MiniDB has 5sec to send the 'ready' message upon startup, or fail
		this.pendingStartup.timeout = setTimeout(() => {
			if (this.state === DBMS_STATES.STARTING) {
				this.failProcess(new Error("DBMS startup timed out"));
				this.terminateFailedProcess(child);
			}
		}, 5000);


		// Node.js process receives output on the child process' stdout stream
		let stdoutBuffer = Buffer.alloc(0);

		const decoder = new TextDecoder("utf-8", {
			fatal: true
		});

		child.stdout.on("data", (chunk) => {
			stdoutBuffer = Buffer.concat([stdoutBuffer, chunk]);
			let newlineIndex;

			while ((newlineIndex = stdoutBuffer.indexOf(0x0A)) !== -1) {
				let frame = stdoutBuffer.subarray(0, newlineIndex);

				stdoutBuffer = stdoutBuffer.subarray(newlineIndex + 1);

				// Validate JSONL line length < 8MB
				if (frame.length > MAX_FRAME_SIZE) {
					this.failProcess(new Error("DBMS response exceeds 8MB"));
					this.terminateFailedProcess(child);
					return;
				}

				// Support optional CRLF (\r\n) line endings 
				if (frame.at(-1) === 0x0D) {
					frame = frame.subarray(0, frame.length - 1);
				}

				let line;
				let message;

				try {
					line = decoder.decode(frame);
				} catch (error) {
					this.failProcess(new Error("Invalid UTF-8 received from MiniDB"));
					this.terminateFailedProcess(child);
					return;
				}

				try {
					message = JSON.parse(line);
				} catch (error) {
					this.failProcess(new Error("Invalid JSON received from MiniDB"));
					this.terminateFailedProcess(child);
					return;
				}

				if (message === null || typeof message !== "object" || Array.isArray(message)) {
					this.failProcess(new Error("Invalid DBMS protocol message"));
					this.terminateFailedProcess(child);
					return;
				}

				console.log("DBMS stdout:", message);

				if (this.state === DBMS_STATES.STARTING) {
					if (!isValidReadyMessage(message)) {
						this.failProcess(new Error("Invalid READY message"));
						this.terminateFailedProcess(child);
						return;
					}

					this.state = DBMS_STATES.READY;
					this.resolveStartup();
				}
				else if (this.state === DBMS_STATES.READY) {
					const pending = this.pendingRequest;

					if (pending === null || !isValidResponse(message, pending.id)) {
						this.failProcess(new Error("Invalid DBMS response"));
						this.terminateFailedProcess(child);
						return;
					}

					this.pendingRequest = null;
					clearTimeout(pending.timeout);
					pending.resolve(message);
				}
			}

			// Validate buffer length < 8MB, when no '\n' is received yet
			if (stdoutBuffer.length > MAX_FRAME_SIZE) {
				this.failProcess(new Error("DBMS response exceeds 8MB"));
				this.terminateFailedProcess(child);
				return;
			}
		});

		// Node.js process receives output on the child process' stderr stream
		child.stderr.on("data", (chunk) => {
			console.error("DBMS stderr:", chunk.toString());
		});

		// Listen for errors on the child process' input stream
		child.stdin.on("error", (error) => {
			if (this.child !== child) return;

			this.failProcess(error);
			this.terminateFailedProcess(child);
		})

		// Process event handlers

		// An error occurred during a child process operation
		child.on("error", (error) => {
			if (this.child !== child) return;

			this.failProcess(error);
		});

		// The child process has terminated
		child.on("exit", (code, signal) => {
			if (this.child !== child) return;

			console.log("DBMS exited:", { code, signal });
		});

		// Cleanup - The child process' has terminated (or failed to start) 
		// and its standard I/O streams have closed
		child.on("close", (code, signal) => {
			if (this.child !== child) return;

			// Any remaining bytes without a '\n' form an incomplete JSONL frame
			const hasIncompleteFrame = stdoutBuffer.length > 0;

			if (hasIncompleteFrame) {
				this.failProcess(new Error("Incomplete DBMS JSONL frame at EOF"));
			}
			else if (this.state === DBMS_STATES.STOPPING && code === 0) {
				this.child = null;
				this.state = DBMS_STATES.STOPPED;
				this.resolveShutdown();
			}
			else if (this.state !== DBMS_STATES.FAILED) {
				this.failProcess(new Error(`DBMS process closed unexpectedly: ${code} ${signal}`));
			}

			this.child = null;
		});

		return startupPromise;
	}

	resolveStartup() {
		const pending = this.pendingStartup;

		if (pending === null) return;

		this.pendingStartup = null;
		clearTimeout(pending.timeout);
		pending.resolve();
	}

	// Send request to the DBMS child process
	async sendRequest(request) {
		if (this.state !== DBMS_STATES.READY || this.child === null) {
			return Promise.reject(new Error("DBMS process is not available"));
		}

		if (this.pendingRequest !== null) {
			return Promise.reject(new Error("Another DBMS request is pending"));
		}

		if (!isValidRequest(request)) {
			return Promise.reject(new Error("Invalid DBMS request"));
		}

		// Validate input JSONL is < 8MB
		let serialized;

		try {
			serialized = JSON.stringify(request);
		} catch (error) {
			throw new Error("Failed to serialize DBMS request", { cause: error });
		}

		if (Buffer.byteLength(serialized, "utf8") > MAX_FRAME_SIZE) {
			return Promise.reject(new Error("DBMS request exceeds 8MB"));
		}

		return new Promise((resolve, reject) => {
			const child = this.child;

			this.pendingRequest = {
				id: request.id,
				resolve,
				reject,
				timeout: null
			};

			this.pendingRequest.timeout = setTimeout(() => {
				if (this.pendingRequest !== null && this.pendingRequest.id === request.id) {
					this.failProcess(new Error("DBMS request timed out."));
					this.terminateFailedProcess(child);
				}
			}, 10000);

			try {
				child.stdin.write(serialized + "\n", (error) => {
					if (error && this.child === child) {
						this.failProcess(error);
						this.terminateFailedProcess(child);
					}
				});
			} catch (error) {
				this.failProcess(error);
				this.terminateFailedProcess(child);
			}
		});
	}

	async shutdown() {
		// Already stopped DMBS child process
		if (this.state === DBMS_STATES.STOPPED && this.child === null) {
			return;
		}

		// Child process actively stopping
		if (this.state === DBMS_STATES.STOPPING) {
			return this.pendingShutdown.promise;
		}

		// Invalid shutdown non-READY state
		if (this.state !== DBMS_STATES.READY) {
			return Promise.reject(new Error(`Cannot gracefully shut down DBMS in state: ${this.state}`));
		}

		// Invalid shutdown for currently pending request
		if (this.pendingRequest !== null) {
			return Promise.reject(new Error("Cannot shut down DBMS while a request is pending"));
		}

		const child = this.child;

		this.state = DBMS_STATES.STOPPING;

		const promise = new Promise((resolve, reject) => {
			this.pendingShutdown = {
				resolve,
				reject,
				promise: null,
				timeout: null
			};
		});

		this.pendingShutdown.promise = promise;

		this.pendingShutdown.timeout = setTimeout(() => {
			if (this.child === child && this.state === DBMS_STATES.STOPPING) {
				this.failProcess(new Error("DBMS shutdown timed out"));
				this.terminateFailedProcess(child);
			}
		}, 10000);

		// Close child process' stdin stream 
		try {
			child.stdin.end();
		} catch(error) {
			this.failProcess(error);
			this.terminateFailedProcess(child);
		}

		return promise;
	}

	// Successful shutdown resolution helper
	resolveShutdown() {
		const pending = this.pendingShutdown;

		if (pending === null) {
			return;
		}

		this.pendingShutdown = null;
		clearTimeout(pending.timeout);
		pending.resolve();
	}

	// Unuccessful shutdown rejection helper
	rejectShutdown(error) {
		const pending = this.pendingShutdown;

		if (pending === null) {
			return;
		}

		this.pendingShutdown = null;
		clearTimeout(pending.timeout);
		pending.reject(error);
	}

	// Centralized error handling that handles potential startup failures
	// and failures during pending requests
	failProcess(error) {
		this.state = DBMS_STATES.FAILED;

		// Reject pending startup
		if (this.pendingStartup !== null) {
			const pending = this.pendingStartup;
			this.pendingStartup = null;

			clearTimeout(pending.timeout);
			pending.reject(error);
		}

		// Reject pending request
		if (this.pendingRequest !== null) {
			const pending = this.pendingRequest;
			this.pendingRequest = null;

			clearTimeout(pending.timeout);
			pending.reject(error);
		}

		// Prevent shutdown timeout from remaining active after failure
		this.rejectShutdown(error);
	}

	terminateFailedProcess(child) {
		if (this.child !== child) return;

		if (child.exitCode === null && child.signalCode === null) {
			child.kill();
		}
	}
}