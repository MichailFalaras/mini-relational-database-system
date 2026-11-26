import { DBMS_ERRORS } from "./dbmsErrors.js";

// Validate protocol response structure for the initial READY message coming from the child process
// Validate "version", "type", and "capabilities" properties
export function isValidReadyMessage(message) {
	if (message === null || typeof message !== "object" || Array.isArray(message)) {
		return false;
	}

	if (!Object.hasOwn(message, "version") || message.version === null || message.version !== 1) {
		return false;
	}

	if (!Object.hasOwn(message, "type") || message.type === null || message.type !== "ready") {
		return false;
	}

	if (!Array.isArray(message.capabilities) || 
		!message.capabilities.every((cap) => typeof cap === "string") ||
		Object.hasOwn(message, "id")) {
		return false;
	}

	return true;
}


// Validate protocol request structure
export function isValidRequest(request) {
	if (request == null || typeof request !== "object" || Array.isArray(request)) {
		return false;
	}

	if (request.version !== 1 || typeof request.id !== "string" ||
		(Buffer.byteLength(request.id, "utf8") <= 0 || 
		Buffer.byteLength(request.id, "utf8") > 128)) {
		return false;
	}

	// Validate common payload properties
	if (!Object.hasOwn(request, "payload") || request.payload === null || 
		typeof request.payload !== "object" || Array.isArray(request.payload)) {
		return false;
	}

	// Validate payload contents of each query type
	switch (request.type) {
		case "query":
			if (Object.keys(request.payload).length === 1 && Object.hasOwn(request.payload, "sql") && 
				request.payload.sql !== null && typeof request.payload.sql === "string" &&
				Buffer.byteLength(request.payload.sql, "utf8") > 0) {
				return true;
			}
			else {
				return false;
			}
			
		case "schema":
		case "shutdown":
		case "close_database":
			if (Object.keys(request.payload).length === 0) {
				return true;
			}
			else {
				return false;
			}

		case "open_database":
		case "delete_database":
			if (Object.keys(request.payload).length === 1 &&
				Object.hasOwn(request.payload, "path") && request.payload.path !== null &&
				typeof request.payload.path === "string" &&
				Buffer.byteLength(request.payload.path, "utf8") > 0) {
				return true;
			}
			else {
				return false;
			}

		case "create_database":
			if (Object.keys(request.payload).length === 2 &&
				Object.hasOwn(request.payload, "name") && request.payload.name !== null &&
				typeof request.payload.name === "string" &&
				Buffer.byteLength(request.payload.name, "utf8") > 0 &&
				Object.hasOwn(request.payload, "path") && request.payload.path !== null &&
				typeof request.payload.path === "string" &&
				Buffer.byteLength(request.payload.path, "utf8") > 0) {
				return true;
			}
			else {
				return false;
			}

		default:
			return false;
	}
}

// Validate protocol response structure for normal "success" and "error" responses
export function isValidResponse(response, expectedId) {
	if (response === null || typeof response !== "object" || Array.isArray(response)) {
		return false;
	}

	if (!Object.hasOwn(response, "version") || response.version !== 1) {
		return false;
	}

	if (!Object.hasOwn(response, "id") || response.id === null || typeof response.id !== "string" ||
		(Buffer.byteLength(response.id, "utf8") <= 0 || 
		Buffer.byteLength(response.id, "utf8") > 128)) {
		return false;
	}

	if (response.id !== expectedId) {
		return false;
	}

	// Validate responses based on their completion status
	switch (response.status) {
		case "success":
			if (!Object.hasOwn(response, "result") || response.result === null || 
				typeof response.result !== "object" ||
				Array.isArray(response.result) || Object.hasOwn(response, "error")) {
				return false;
			}

			if (!Object.hasOwn(response.result, "kind") ||
				!["rows", "command", "schema", "database", "shutdown"].includes(response.result.kind)) {
				return false;
			}

			if (response.result.kind === "rows") {
				if (!Object.hasOwn(response.result, "columns") || response.result.columns === null ||
					!Array.isArray(response.result.columns)) {
					return false;
				}

				if (!Object.hasOwn(response.result, "rows") || response.result.rows === null ||
					!Array.isArray(response.result.rows)) {
					return false;
				}

				if (!Object.hasOwn(response.result, "rowCount") ||
					!Number.isInteger(response.result.rowCount) ||
					response.result.rowCount < 0 ||
					response.result.rowCount !== response.result.rows.length) {
					return false;
				}
			}
			else if (response.result.kind === "command") {
				if (!Object.hasOwn(response.result, "command") || response.result.command === null ||
					typeof response.result.command !== "string") {
					return false;
				}
				
				if (!Object.hasOwn(response.result, "rowsAffected") ||
					(response.result.rowsAffected !== null &&
					(!Number.isInteger(response.result.rowsAffected) || response.result.rowsAffected < 0))) {
					return false;
				}
			}
			else if (response.result.kind === "schema") {
				if (!Object.hasOwn(response.result, "tables") || 
					response.result.tables === null || typeof response.result.tables !== "object" || 
					!Array.isArray(response.result.tables)) {
					return false;
				}

				if (!Object.hasOwn(response.result, "indexes") || 
					response.result.indexes === null || typeof response.result.indexes !== "object" || 
					!Array.isArray(response.result.indexes)) {
					return false;
				}

				// Validate all tables' structures in the schema
				for (const table of response.result.tables) {
					if (table === null || typeof table !== "object" || Array.isArray(table)) {
						return false;
					}

					if (table.name === null || typeof table.name !== "string" ||
						Buffer.byteLength(table.name, "utf8") <= 0) {
						return false;
					}

					if (!Array.isArray(table.columns)) {
						return false;
					}
				}

				
			}
			else if (response.result.kind === "database") {
				if (!Object.hasOwn(response.result, "state") ||
					response.result.state === null || typeof response.result.state !== "string" ||
					Buffer.byteLength(response.result.state, "utf8") <= 0) {
					return false;
				}

				if (!["opened", "closed", "created", "deleted"].includes(response.result.state)) {
					return false;
				}

				if (Object.hasOwn(response.result, "name")) {
					if (response.result.name === null || typeof response.result.name !== "string" ||
						Buffer.byteLength(response.result.name, "utf8") <= 0) {
						return false;
					}
				}
			}
			else if (response.result.kind === "shutdown") {
				// Verify that the result object only contains only the kind for the shutdown operation 
				if (Object.keys(response.result).length !== 1) {
					return false;
				}
			}

			break;
			
		case "error":
			if (!Object.hasOwn(response, "error") || response.error === null || 
				typeof response.error !== "object" ||
				Array.isArray(response.error) || Object.hasOwn(response, "result")) {
				return false;
			}

			if (!Object.hasOwn(response.error, "code") || 
				response.error.code === null || typeof response.error.code !== "string" ||
				!Object.hasOwn(response.error, "category") || 
				response.error.category === null || typeof response.error.category !== "string" ||
				!Object.hasOwn(response.error, "message") || 
				response.error.message === null || typeof response.error.message !== "string") {
				return false;
			}

			// Validate error code and category match
			if (Object.keys(DBMS_ERRORS).indexOf(response.error.code) === -1) {
				return false;
			}

			if (DBMS_ERRORS[response.error.code] !== response.error.category) {
				return false;
			}

			break;

		default:
			return false;
	}

	// Validate optional execution metadata
	if (Object.hasOwn(response, "execution")) {
		if (response.execution === null || typeof response.execution !== "object" ||
			Array.isArray(response.execution)) {
			return false;
		}

		if (!Object.hasOwn(response.execution, "durationMs") ||
			!Number.isFinite(response.execution.durationMs) ||
			response.execution.durationMs < 0) {
			return false;
		}
	}

	return true;
}