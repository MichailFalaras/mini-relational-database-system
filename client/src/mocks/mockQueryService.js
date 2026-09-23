import { guestState } from "./guestState.js";
import { mockDelay } from "./mockUtils.js";


export async function mockExecuteQuery(databaseId, sql) {
	await mockDelay();

	const rows = guestState.rows[databaseId];

	if (!rows) {
		throw new Error("Database not found");
	}

	const normalizedSQL = sql.trim().toLowerCase();
	
	// Empty query
	if (!normalizedSQL) {
		throw new Error("Query cannot be empty");
	}

	// Syntax error
	if (normalizedSQL.includes("syntax_error")) {
		throw new Error("Syntax error near 'syntax_error'");
	}

	// SELECT query that fetches all rows of the matching table
	if (normalizedSQL.startsWith("select")) {
		const tableName = Object.keys(rows).find((table) => normalizedSQL.includes(`from ${table}`));

		if (!tableName) {
			throw new Error("Table not found");
		}

		const table = rows[tableName];

		// Testing SELECT statement returning empty rows
		if (sql.includes("where 1 = 0")) {
			return {
				type: "select",
				columns: table.columns,
				rows: [],
				executionTime: 5
			};
		}

		return {
			type: "select",
			columns: table.columns,
			rows: table.rows,
			executionTime: 12
		};
	}

	// DML query (UPDATE, INSERT, DELETE)
	if (normalizedSQL.startsWith("insert") ||
		normalizedSQL.startsWith("update") ||
		normalizedSQL.startsWith("delete")) {
		return {
			type: "mutation",
			rowsAffected: 1,
			executionTime: 8
		};
	}

	// DDL query (CREATE TABLE/INDEX, DROP TABLE/INDEX, ALTER)
	if (normalizedSQL.startsWith("create") ||
		normalizedSQL.startsWith("alter") ||
		normalizedSQL.startsWith("drop")) {
		return {
			type: "ddl",
			executionTime: 4
		}
	}

	throw new Error("Unsupported SQL statement");
}