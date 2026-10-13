import { spawn } from "node:child_process";

// Process Manager Definition
export class DBMSProcessManager {
	constructor(executablePath) {
		this.executablePath = executablePath;
		this.child = null;
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

		// Standard output streams event handers
		child.stdout.on("data", (chunk) => {
			console.log("DBMS stdout:", chunk.toString());
		});

		child.stderr.on("data", (chunk) => {
			console.error("DBMS stderr:", chunk.toString());
		});

		// Process event handlers
		child.on("error", (error) => {
			console.error("DBMS process error:", error);
		});

		child.on("exit", (code, signal) => {
			console.log("DBMS exited:", { code, signal });
		});

		child.on("close", () => {
			if (this.child === child) {
				this.child = null;
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