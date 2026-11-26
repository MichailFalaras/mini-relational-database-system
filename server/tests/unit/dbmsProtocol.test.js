import { describe, it, expect } from "vitest";
import { isValidReadyMessage, isValidRequest, isValidResponse} from "../../src/dbms/dbmsProtocol.js";

const VALID_READY = {
	version: 1,
	type: "ready",
	capabilities: ["query", "schema"]
};

const VALID_QUERY = {
	version: 1,
	id: "req-001",
	type: "query",
	payload: {
		sql: "SELECT * FROM users;"
	}
};

const VALID_ROWS_RESPONSE = {
	version: 1,
	id: "req-001",
	status: "success",
	result: {
		kind: "rows",
		columns: [
			{ name: "id", type: "INTEGER" }
		],
		rows: [[1], [2]],
		rowCount: 2
	}
};

const VALID_ERROR_RESPONSE = {
	version: 1,
	id: "req-001",
	status: "error",
	error: {
		code: "TABLE_NOT_FOUND",
		category: "semantic",
		message: "Table does not exist"
	}
};

describe("isValidReadyMessage", () => {

	/* ---------- VALID USE CASES ---------- */

	it("Accepts a valid READY message", () => {
		expect(isValidReadyMessage(VALID_READY)).toBe(true);
	});

	it("Accepts an empty capabilities array", () => {
		const message = {
			...VALID_READY,
			capabilities: []
		};

		expect(isValidReadyMessage(message)).toBe(true);
	});

	/* ---------- INVALID USE CASES ---------- */	

	it.each([
		null, 
		undefined, 
		42, 
		"ready"
	])("rejects a non-object READY message: %s", (message) => {
		expect(isValidReadyMessage(message)).toBe(false);
	});

	it("Rejects a READY message containing an ID", () => {
		const message = {
			...VALID_READY,
			id: "req-001"
		};

		expect(isValidReadyMessage(message)).toBe(false);
	});

	it("Missing/incorrect version", () => {
		// Missing version
		let message = { ...VALID_READY };

		delete message.version;
		expect(isValidReadyMessage(message)).toBe(false);

		// Incorrect version
		message = { 
			...VALID_READY, 
			version: 2 
		};

		expect(isValidReadyMessage(message)).toBe(false);
	});

	it("Missing/incorrect type", () => {
		// Missing version
		let message = { ...VALID_READY };

		delete message.type;
		expect(isValidReadyMessage(message)).toBe(false);

		// Incorrect version
		message = { 
			...VALID_READY,
			type: "rows"
		};

		expect(isValidReadyMessage(message)).toBe(false);
	});

	it("Missing/incorrect capabilities", () => {
		// Missing capabilities
		let message = { ...VALID_READY };

		delete message.capabilities;
		expect(isValidReadyMessage(message)).toBe(false);

		// Incorrect capabilities
		message = {
			...VALID_READY,
			capabilities: [1, 2]
		};

		expect(isValidReadyMessage(message)).toBe(false);
	});
});

describe("isValidRequest", () => {

	/* ---------- VALID USE CASES ---------- */

	it("Accepts a valid query request", () => {
		expect(isValidRequest(VALID_QUERY)).toBe(true);
	});

	it.each([
		"schema",
		"shutdown",
		"close_database",
	])("Accepts a valid %s request", (type) => {
		const request = {
			version: 1,
			id: "req-001",
			type,
			payload: {}
		};

		expect(isValidRequest(request)).toBe(true);
	});

	it.each([
		"open_database",
		"delete_database"
	])("Accepts a valid %s request", (type) => {
		const request = {
			version: 1,
			id: "req-001",
			type,
			payload: {
				path: "databases/test.db"
			}
		};

		expect(isValidRequest(request)).toBe(true);
	});

	it("Accepts a valid create_database request", () => {
		const request = {
			version: 1,
			id: "req-001",
			type: "create_database",
			payload: {
				path: "databases/test.db",
				name: "test_database"
			}
		}

		expect(isValidRequest(request)).toBe(true);
	});

	it("Accepts an ID of exactly 128 UTF-8 bytes", () => {
		const request = {
			...VALID_QUERY,
			id: "α".repeat(64)
		};

		expect(isValidRequest(request)).toBe(true);
	});

	/* ---------- INVALID USE CASES ---------- */	

	it("Rejects an ID exceeding 128 UTF-8 bytes", () => {
		const request = {
			...VALID_QUERY,
			id: "α".repeat(100)
		};

		expect(isValidRequest(request)).toBe(false);
	});

	it("Rejects unsupported protocol versions", () => {
		const request = {
			...VALID_QUERY,
			version: 2
		};

		expect(isValidRequest(request)).toBe(false);
	});

	it("Missing/invalid request id", () => {
		// Missing request id
		let request = { ...VALID_QUERY };

		delete request.id;
		expect(isValidRequest(request)).toBe(false);

		// empty request id
		request = {
			...VALID_QUERY,
			id: ""
		};

		expect(isValidRequest(request)).toBe(false);
	});

	it("Rejects a valid operation type with an incompatible payload", () => {
		const request = {
			...VALID_QUERY,
			type: "schema"	
		};

		expect(isValidRequest(request)).toBe(false);
	});

	it("Rejects an unsupported request type", () => {
		const request = {
			...VALID_QUERY,
			type: "unknown_operation"
		};

		expect(isValidRequest(request)).toBe(false);
	});

	it("Missing/invalid payload", () => {
		// Missing payload
		let request = { ...VALID_QUERY };

		delete request.payload;
		expect(isValidRequest(request)).toBe(false);

		// Invalid payload
		request = {
			...VALID_QUERY,
			payload: { query: "SELECT * FROM users;" }
		};

		expect(isValidRequest(request)).toBe(false);
	});

	it("Rejects an empty SQL string", () => {
		const request = {
			...VALID_QUERY,
			payload: { sql: "" }
		};

		expect(isValidRequest(request)).toBe(false);
	});

	it("Rejects additional fields in schema payload", () => {
		const request = {
			version: 1,
			id: "req-001",
			type: "schema",
			payload: { unexpected: "" }
		};

		expect(isValidRequest(request)).toBe(false);
	});

	it("Rejects invalid database paths", () => {
		// Empty database path
		let request = {
			version: 1,
			id: "req-001",
			type: "open_database",
			payload: { path: "" }
		};

		expect(isValidRequest(request)).toBe(false);

		// Invalid type path
		request = {
			version: 1,
			id: "req-001",
			type: "open_database",
			payload: { path: 100 }
		};

		expect(isValidRequest(request)).toBe(false);
	});

	it.each([
		"",
		100,
		null
	])("Rejects invalid database name: %s", (name) => {
		const request = {
			version: 1,
			id: "req-001",
			type: "create_database",
			payload: {
				path: "database/test.db",
				name
			}
		};

		expect(isValidRequest(request)).toBe(false);
	});
});

describe("isValidResponse", () => {

	/* ---------- VALID USE CASES ---------- */

	it("Accepts a valid rows response", () => {
		expect(isValidResponse(VALID_ROWS_RESPONSE, "req-001")).toBe(true);
	});

	it("Accepts an empty schema response", () => {
		const response = {
			version: 1,
			id: "req-001",
			status: "success",
			result: {
				kind: "schema",
				tables: [],
				indexes: []
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(true);
	});

	it("Accepts a valid command response", () => {
		const response = {
			version: 1,
			id: "req-001",
			status: "success",
			result: {
				kind: "command",
				command: "UPDATE",
				rowsAffected: 3
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(true);
	});

	it("Accepts a command response with null rowsAffected", () => {
		const response = {
			...VALID_ROWS_RESPONSE,
			result: {
				kind: "command",
				command: "CREATE TABLE",
				rowsAffected: null
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(true);
	});

	it("Accepts a valid database response", () => {
		const response = {
			version: 1,
			id: "req-001",
			status: "success",
			result: {
				kind: "database",
				name: "inventorydb",
				state: "opened"
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(true);
	});

	it("Accepts a valid shutdown response", () => {
		const response = {
			version: 1,
			id: "req-001",
			status: "success",
			result: {
				kind: "shutdown"
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(true);
	});

	it("Accepts a valid database error", () => {
		expect(isValidResponse(VALID_ERROR_RESPONSE, "req-001")).toBe(true);
	});

	it("Accepts valid execution metadata", () => {
		const response = {
			...VALID_ROWS_RESPONSE,
			execution: { durationMs: 2.5 }
		};

		expect(isValidResponse(response, "req-001")).toBe(true);
	});

            

	/* ---------- INVALID USE CASES ---------- */

	it.each([
    	null,
		undefined,
		42,
		"response",
		[]
	])("Rejects non-object response: %s", (response) => {
		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Rejects mismatched response IDs", () => {
		expect(isValidResponse(VALID_ROWS_RESPONSE, "req-999")).toBe(false);
	});

	it("Rejects unsupported protocol versions", () => {
		const response = {
			...VALID_ROWS_RESPONSE,
			version: 2
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Missing ID", () => {
		const response = { ...VALID_ROWS_RESPONSE };

		delete response.id;
		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Invalid status", () => {
		const response = { 
			...VALID_ROWS_RESPONSE,
			status: "pending"
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Missing version", () => {
		const response = { ...VALID_ROWS_RESPONSE };

		delete response.version;
		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Rejects inconsistent rowCount", () => {
		const response = {
			...VALID_ROWS_RESPONSE,
			result: {
				...VALID_ROWS_RESPONSE.result, rowCount: 5
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Rejects invalid columns array", () => {
		const response = {
			...VALID_ROWS_RESPONSE,
			result: {
				...VALID_ROWS_RESPONSE.result,
				columns: { name: "id" }
			}
		}

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Rejects invalid rows array", () => {
		const response = {
			...VALID_ROWS_RESPONSE,
			result: {
				...VALID_ROWS_RESPONSE.result,
				rows: { first: [1], second: [2] }
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it.each([
		{ command: null, rowsAffected: 3 },
		{ command: 123, rowsAffected: 3 },
		{ command: "UPDATE", rowsAffected: -1 },
		{ command: "UPDATE", rowsAffected: 2.5 },
		{ command: "UPDATE", rowsAffected: "3" },
		{ command: "UPDATE" },
		{ rowsAffected: 3 }
	])("Rejects invalid command result: %j", (result) => {
		const response = {
			...VALID_ROWS_RESPONSE,
			result: {
				kind: "command",
				...result
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it.each([
		{ tables: null, indexes: [] },
		{ tables: {}, indexes: [] },
		{ tables: [], indexes: null },
		{ tables: [], indexes: {} },
		{ tables: [{ name: "", columns: [] }], indexes: [] },
		{ tables: [{ name: 123, columns: [] }], indexes: [] },
		{ tables: [{ name: "users", columns: null }], indexes: [] },
		{ tables: [{ name: "users", columns: {} }], indexes: [] },
		{ tables: [null], indexes: [] },
		{ tables: ["users"], indexes: [] },
		{ tables: [] },
		{ indexes: [] }
	])("Rejects invalid schema result: %j", (result) => {
		const response = {
			...VALID_ROWS_RESPONSE,
			result: {
				kind: "schema",
				...result
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Rejects mismatched error codes and categories", () => {
		const response = {
			...VALID_ERROR_RESPONSE,
			error: {
				...VALID_ERROR_RESPONSE.error,
				category: "syntax"
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Rejects an unknown error code", () => {
		const response = {
			...VALID_ERROR_RESPONSE,
			error: {
				...VALID_ERROR_RESPONSE.error,
				code: "UNKNOWN_ERROR"
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it.each([
		{ category: "semantic", message: "Table does not exist" },
		{ code: "TABLE_NOT_FOUND", message: "Table does not exist" },
		{ code: "TABLE_NOT_FOUND", category: "semantic" }
	])("Rejects missing error fields: %j", (error) => {
		const response = {
			...VALID_ERROR_RESPONSE,
			error
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Rejects negative execution duration", () => {
		const response = {
			...VALID_ROWS_RESPONSE,
			execution: {
				durationMs: -1
			}
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Rejects null execution metadata", () => {
		const response = {
			...VALID_ROWS_RESPONSE,
			execution: null
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it("Rejects execution metadata without durationMs", () => {
		const response = {
			...VALID_ROWS_RESPONSE,
			execution: {}
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});

	it.each([
		NaN,
		Infinity,
		-Infinity,
		"10",
		null,
		undefined
	])("Rejects invalid execution duration: %s", (durationMs) => {
		const response = {
			...VALID_ROWS_RESPONSE,
			execution: { durationMs }
		};

		expect(isValidResponse(response, "req-001")).toBe(false);
	});
});