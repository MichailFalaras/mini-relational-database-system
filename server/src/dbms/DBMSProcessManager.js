import { spawn } from "node:child_process";

// All states the C DBMS Process Manager can be in
const DBMS_STATES = Object.freeze({
	STOPPED: "stopped",
	STARTING: "starting",
	READY: "ready",
	STOPPING: "stopping",
	FAILED: "failed"
});

// Process Manager Definition
export class DBMSProcessManager {
	constructor(executablePath) {
		this.executablePath = executablePath;
		this.child = null;
		this.state = DBMS_STATES.STOPPED;
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

		// Standard output streams event handers
		child.stdout.on("data", (chunk) => {
			console.log("DBMS stdout:", chunk.toString());
		});

		child.stderr.on("data", (chunk) => {
			console.error("DBMS stderr:", chunk.toString());
		});

		// Process event handlers
		// An error occurred during a child process operation (spawn, kill, communication)
		child.on("error", (error) => {
			this.state = DBMS_STATES.FAILED;
			console.error("DBMS process error:", error);
		});

		// The child process has terminated
		child.on("exit", (code, signal) => {
			console.log("DBMS exited:", { code, signal });

			if (this.child !== child) return;
			
			// Checking for abnormal termination (!= STOPPING)
			if (this.state !== DBMS_STATES.STOPPING) {
				this.state = DBMS_STATES.FAILED;
			}
		});

		// Cleanup - The child process' has terminated (or failed to start) 
		// and its standard I/O streams have closed
		child.on("close", (code, signal) => {
			if (this.child !== child) return;

			this.child = null;

			if (this.state === DBMS_STATES.STOPPING && code === 0) {
				this.state = DBMS_STATES.STOPPED;
			}
			else {
				this.state = DBMS_STATES.FAILED;
			}
		});
	}

	async sendRequest(request) {
		throw new Error("Not implemented yet");
	}

	async shutdown() {
		throw new Error("Not implemented yet");
	}
}