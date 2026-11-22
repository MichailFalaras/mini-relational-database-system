import { describe, it, expect, afterEach } from "vitest"
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { DBMSProcessManager } from "../../src/dbms/DBMSProcessManager.js";

// Locate the compiled C fixture independently of the working directory
const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

const MOCK_EXECUTABLE = resolve(
	__dirname, 
	"../fixtures/mock_dbms",
	process.platform === "win32" ? "mock_dbms.exe" : "mock_dbms"
);

// Query creation helpers
const createRequest = (id, type, payload) => ({
	version: 1,
	id,
	type,
	payload
}); 

const createQuery = (id, sql) => 
	createRequest(id, "query", { sql });

// DBMS process manager helpers
let managers = [];

function createManager(executablePath = MOCK_EXECUTABLE) {
	const manager = new DBMSProcessManager(executablePath);
	managers.push(manager);
	return manager;
}

async function cleanupManager(manager) {
	const child = manager.child;

	if (child === null) return;

	const closed = new Promise((resolve) => {
		child.once("close", resolve);
	});

	// Terminate the process
    child.kill();

    await closed;
}

describe("DBMSProcessManager", () => {
	// Reset all DBMS managers after every test
	afterEach(async () => {
		await Promise.all(managers.map(cleanupManager));
		managers = [];
	});

	/* ---------- PROCESS STARTUP ---------- */

	it("Starts the mock and receives READY", async () => {
		const manager = createManager();
		
		await manager.start();

		expect(manager.state).toBe("ready");
		expect(manager.child).not.toBeNull();
		expect(manager.pendingStartup).toBeNull();
	});

	it("READY messages split across chunks", async() => {
		const manager = createManager(
			resolve(__dirname, "../fixtures/mock_dbms", "mock_dbms_split_ready.exe")
		);

		await manager.start();

		expect(manager.state).toBe("ready");
		expect(manager.child).not.toBeNull();
		expect(manager.pendingStartup).toBeNull();
	});

	it("Rejects duplicate startup", async () => {
		const manager = createManager();
		
		await manager.start();

		await expect(manager.start()).rejects.toThrow("DBMS process already exists");

		expect(manager.state).toBe("ready");
	});

	it("Clears pendingStartup after startup failure", async () => {
		const manager = createManager(
			resolve(__dirname, "../fixtures/mock_dbms", "mock_dbms_invalid_ready.exe")
		);

		await expect(manager.start()).rejects.toThrow("Invalid READY message");

		expect(manager.state).toBe("failed");
		expect(manager.pendingStartup).toBeNull();
		expect(manager.pendingRequest).toBeNull();
	});

	it("Rejects a non-existent executable", async () => {
		const manager = createManager(resolve(__dirname, "nonexistent-dbms.exe"));

		await expect(manager.start()).rejects.toThrow();

		expect(manager.state).toBe("failed");
	});

	it("Invalid READY message", async () => {
		const manager = createManager(
			resolve(__dirname, "../fixtures/mock_dbms", "mock_dbms_invalid_ready.exe")
		);

		await expect(manager.start()).rejects.toThrow("Invalid READY message");

		expect(manager.state).toBe("failed");
	});

	it("Child process exits before READY", async () => {
		const manager = createManager(
			resolve(__dirname, "../fixtures/mock_dbms", "mock_dbms_exit_before_ready.exe")
		);

		await expect(manager.start())
			.rejects
			.toThrow("DBMS process closed unexpectedly: 1 null");

		expect(manager.state).toBe("failed");
	});

	it("Rejects startup when READY is not received", async () => {
		const manager = createManager(
			resolve(__dirname, "../fixtures/mock_dbms", "mock_dbms_startup_timeout.exe")
		);

		await expect(manager.start()).rejects.toThrow("DBMS startup timed out");

		expect(manager.state).toBe("failed");
		expect(manager.pendingStartup).toBeNull();

	}, 10000);

	/* ---------- REQUEST-RESPONSE COMMUNICATION ---------- */

	it("Receives a valid schema response", async () => {
		const manager = createManager();

		await manager.start();

		expect(manager.state).toBe("ready");
		expect(manager.child).not.toBeNull();
		expect(manager.pendingStartup).toBeNull();

		const response = await manager.sendRequest(createRequest("req-001", "schema", {}));

		expect(response.version).toBe(1);
		expect(response.id).toBe("req-001");
		expect(response.status).toBe("success");
		expect(response.result.kind).toBe("schema");
		expect(response.result.tables).toEqual([]);
		expect(response.result.indexes).toEqual([]);
		
		expect(manager.pendingRequest).toBeNull();
		expect(manager.state).toBe("ready");
	});

	it("Receives a valid rows response", async () => {
		const manager = createManager();

		await manager.start();

		expect(manager.state).toBe("ready");
		expect(manager.child).not.toBeNull();
		expect(manager.pendingStartup).toBeNull();

		const response = await manager.sendRequest(createQuery("req-002", "SELECT * FROM users;"));

		expect(response.version).toBe(1);
		expect(response.id).toBe("req-002");
		expect(response.status).toBe("success");
		
		expect(response.result.kind).toBe("rows");
		expect(Array.isArray(response.result.columns)).toBe(true);
		expect(Array.isArray(response.result.rows)).toBe(true);
		expect(response.result.rowCount).toBe(response.result.rows.length);

		expect(manager.pendingRequest).toBeNull();
		expect(manager.state).toBe("ready");
	});

	it("Handles sequential requests", async () => {
		const manager = createManager();

		await manager.start();

		expect(manager.state).toBe("ready");
		expect(manager.child).not.toBeNull();
		expect(manager.pendingStartup).toBeNull();

		const first = await manager.sendRequest(createRequest("req-001", "schema", {}));
		const second = await manager.sendRequest(createQuery("req-002", "SELECT * FROM users;"));

		expect(first.id).toBe("req-001");
		expect(second.id).toBe("req-002");

		expect(manager.pendingRequest).toBeNull();
		expect(manager.state).toBe("ready");
	});

	it("Resolves a structured DBMS error response", async () => {
		const manager = createManager();

		await manager.start();

		expect(manager.state).toBe("ready");
		expect(manager.child).not.toBeNull();
		expect(manager.pendingStartup).toBeNull();

		const response = await manager.sendRequest(createQuery("req-003", "MOCK_ERROR"));

		expect(response.id).toBe("req-003");
		expect(response.status).toBe("error");
		expect(response.error.code).toBe("TABLE_NOT_FOUND");
		expect(response.error.category).toBe("semantic");

		// A database error must not terminate the process
		expect(manager.pendingRequest).toBeNull();
		expect(manager.state).toBe("ready");
	});

	it("Reassembles a response split across stdout chunks", async () => {
		const manager = createManager();

		await manager.start();

		const response = await manager.sendRequest(createQuery("req-001", "MOCK_SPLIT_RESPONSE"));

		expect(response.id).toBe("req-001");
		expect(response.status).toBe("success");
		expect(manager.state).toBe("ready");
		expect(manager.pendingRequest).toBeNull();
	});

	it("Accepts split UTF-8 JSONL responses", async () => {
		const manager = createManager();

		await manager.start();

		const response = await manager.sendRequest(createQuery("req-001", "MOCK_SPLIT_UTF8"));

		expect(response.status).toBe("success");
		expect(manager.state).toBe("ready");
	});

	it("Accepts CRLF-terminated JSONL responses", async () => {
		const manager = createManager();

		await manager.start();

		const response = await manager.sendRequest(createQuery("req-001", "MOCK_CRLF_RESPONSE"));

		expect(response.status).toBe("success");
		expect(manager.state).toBe("ready");
	});

	it("Accepts a response frame of exactly 8 MiB", async () => {
		const manager = createManager();

		await manager.start();

		const response = await manager.sendRequest(
			createQuery("req-001", "MOCK_MAX_SIZE_RESPONSE")
		);

		expect(response.id).toBe("req-001");
		expect(response.status).toBe("success");
		expect(response.result.kind).toBe("rows");

		expect(manager.state).toBe("ready");
		expect(manager.pendingRequest).toBeNull();
	}, 15000);

	/* ---------- REQUEST REJECTION ---------- */

	it("Rejects requests before startup", async () => {
		const manager = createManager();

		await expect(manager.sendRequest(createRequest("req-001", "schema", {})))
			.rejects
			.toThrow("DBMS process is not available");
	});

	it("Rejects invalid requests without failing the process", async () => {
		const manager = createManager();

		await manager.start();

		const invalidRequest = createQuery("req-001", "");

		await expect(manager.sendRequest(invalidRequest)).rejects.toThrow("Invalid DBMS request");

		expect(manager.pendingRequest).toBeNull();
		expect(manager.state).toBe("ready");
	});

	it("Rejects a second request while one is pending", async () => {
		const manager = createManager();

		await manager.start();

		// The mock intentionally does not respond
		const firstPromise = manager.sendRequest(createQuery("req-001", "MOCK_NO_RESPONSE"));

		const firstRejection = expect(firstPromise).rejects.toThrow();

		await expect(manager.sendRequest(createRequest("req-002", "schema", {})))
			.rejects
			.toThrow("Another DBMS request is pending");

		expect(manager.pendingRequest?.id).toBe("req-001");
	
		// Force process failure to settle the first request.
		manager.failProcess(new Error("Test cleanup"));
		manager.terminateFailedProcess(manager.child);
		await firstRejection;
	});

	it("Rejects a request after the manager enter the FAILED state", async () => {
		const manager = createManager();

		await manager.start();

		await expect(manager.sendRequest(createQuery("req-001", "MOCK_BAD_JSON")))
			.rejects
			.toThrow("Invalid JSON received from MiniDB");

		expect(manager.state).toBe("failed");

		await expect(manager.sendRequest(createRequest("req-002", "schema", {})))
			.rejects
			.toThrow("DBMS process is not available");

		expect(manager.pendingRequest).toBeNull();
	});

	it("Accepts another request after a database error", async () => {
		const manager = createManager();

		await manager.start();

		const errorResponse = await manager.sendRequest(createQuery("req-001", "MOCK_ERROR"));

		expect(errorResponse.status).toBe("error");
		expect(errorResponse.error.code).toBe("TABLE_NOT_FOUND");

		expect(manager.state).toBe("ready");
		expect(manager.pendingRequest).toBeNull();

		const successResponse = await manager.sendRequest(createRequest("req-002", "schema", {}));

		expect(successResponse.status).toBe("success");
		expect(successResponse.id).toBe("req-002");
		expect(successResponse.result.kind).toBe("schema");

		expect(manager.state).toBe("ready");
		expect(manager.pendingRequest).toBeNull();
	});

	it("Rejects an outgoing request exceeding 8 MiB", async () => {
		const manager = createManager();

		await manager.start();

		const oversizedSQL = "A".repeat(8 * 1024 * 1024);

		const request = createQuery("req-001", oversizedSQL);

		await expect(manager.sendRequest(request)).rejects.toThrow("DBMS request exceeds 8MB");

		expect(manager.state).toBe("ready");
		expect(manager.pendingRequest).toBeNull();
	});

	/* ---------- PROTOCOL FAILURES ---------- */

	it("Rejects a response with a mismatched ID", async () => {
		const manager = createManager();

		await manager.start();

		await expect(manager.sendRequest(createQuery("req-001", "MOCK_WRONG_ID")))
			.rejects
			.toThrow("Invalid DBMS response");

		expect(manager.pendingRequest).toBeNull();
		expect(manager.state).toBe("failed");
	});

	it("Rejects malformed JSON from MiniDB", async () => {
		const manager = createManager();

		await manager.start();

		await expect(manager.sendRequest(createQuery("req-001", "MOCK_BAD_JSON")))
			.rejects
			.toThrow("Invalid JSON received from MiniDB");

		expect(manager.pendingRequest).toBeNull();
		expect(manager.state).toBe("failed");
	});

	it("Rejects invalid response structure", async () => {
		const manager = createManager();

		await manager.start();

		await expect(manager.sendRequest(createQuery("req-001", "MOCK_INVALID_RESPONSE")))
			.rejects
			.toThrow("Invalid DBMS response");
		
		expect(manager.pendingRequest).toBeNull();
		expect(manager.state).toBe("failed");
	});

	it("Rejects an oversized response frame", async () => {
		const manager = createManager();

		await manager.start();

		await expect(manager.sendRequest(createQuery("req-001", "MOCK_OVERSIZED_RESPONSE")))
			.rejects
			.toThrow("DBMS response exceeds 8MB");
		
		expect(manager.pendingRequest).toBeNull();
		expect(manager.state).toBe("failed");
	});

	it("Rejects an unsolicited response", async () => {
		const manager = createManager();

		await manager.start();

		const child = manager.child;

		const closed = new Promise(resolve => {
			child.once("close", resolve);
		});

		const response = await manager.sendRequest(
			createQuery("req-001", "MOCK_UNSOLICITED_RESPONSE")
		);

		expect(response.status).toBe("success");

		await closed;

		expect(manager.pendingRequest).toBeNull();
		expect(manager.state).toBe("failed");
		expect(manager.child).toBeNull();
	});

	it("Rejects invalid UTF-8 received from MiniDB", async () => {
		const manager = createManager();

		await manager.start();

		await expect(manager.sendRequest(createQuery("req-001", "MOCK_INVALID_UTF8")))
			.rejects
			.toThrow("Invalid UTF-8 received from MiniDB");

		expect(manager.state).toBe("failed");
		expect(manager.pendingRequest).toBeNull();
	});

	it("Rejects an oversized incomplete JSONL frame", async () => {
		const manager = createManager();

		await manager.start();

		await expect(manager.sendRequest(createQuery("req-001", "MOCK_OVERSIZED_INCOMPLETE_FRAME")))
			.rejects
			.toThrow("DBMS response exceeds 8MB");

		expect(manager.state).toBe("failed");
		expect(manager.pendingRequest).toBeNull();
	});

	/* ---------- CHILD PROCESS FAILURES ---------- */

	it("Rejects a pending request when the child process exits", async () => {
		const manager = createManager();

		await manager.start();

		await expect(manager.sendRequest(createQuery("req-001", "MOCK_EXIT"))).rejects.toThrow();

		expect(manager.state).toBe("failed");
		expect(manager.pendingRequest).toBeNull();
	});

	it("Detects unexpected exit after READY without a pending request", async () => {
		const manager = createManager();

		await manager.start();

		expect(manager.state).toBe("ready");
		expect(manager.pendingRequest).toBeNull();

		const child = manager.child;

		const closed = new Promise(resolve => {
			child.once("close", resolve);
		});

		expect(child.kill()).toBe(true);

		await closed;

		expect(manager.state).toBe("failed");
		expect(manager.child).toBeNull();
		expect(manager.pendingRequest).toBeNull();
	});

	it("Incomplete JSONL frame at EOF", async () => {
		const manager = createManager();

		await manager.start();

		expect(manager.state).toBe("ready");

		await expect(manager.sendRequest(createQuery("req-001", "MOCK_INCOMPLETE_FRAME")))
			.rejects
			.toThrow("Incomplete DBMS JSONL frame at EOF");

		expect(manager.state).toBe("failed");
		expect(manager.pendingRequest).toBeNull();
	});

	it("Rejects a request when MiniDB does not respond", async () => {
		const manager = createManager();

		await manager.start();

		await expect(manager.sendRequest(createQuery("req-001", "MOCK_NO_RESPONSE")))
			.rejects
			.toThrow("DBMS request timed out");

		expect(manager.state).toBe("failed");
		expect(manager.pendingRequest).toBeNull();
	}, 15000);
});