import { guestState } from "./guestState.js";
import { mockDelay } from "./mockUtils.js";

// Return the mock schema
export async function mockGetDatabases() {
	await mockDelay();

	return structuredClone(guestState.databases);
}

// Connect to a mock database
export async function mockOpenDatabase(databaseId) {
	await mockDelay();

	const database = guestState.databases.find((db) => db.id === databaseId);

	if (!database) {
		throw new Error("Database not found");
	}

	database.status = "open";

	return structuredClone(database);
}

// Create a mock database
export async function mockCreateDatabase(database) {
	await mockDelay();

	if (!database?.databaseName) {
		throw new Error("Database name is required");
	}

	// Validating if the new database already exists or not
	const alreadyExists = guestState.databases.some(
		(db) => db.name.toLowerCase() === database.databaseName.toLowerCase()
	);

	if (alreadyExists) {
		throw new Error(`Database ${database.databaseName} already exists`);
	}

	// New database object
	const newDatabase = {
		id: crypto.randomUUID(),
		name: database.databaseName,
		status: "connected",
		user: database.username ?? "guest",
		numTables: 0,
		size: "0 MB"
	};

	// Populate the guestState fields for the new database
	guestState.databases.push(newDatabase);

	guestState.schemas[newDatabase.id] = {
		tables: [],
		indexes: []
	};

	guestState.rows[newDatabase.id] = {};

	return structuredClone(newDatabase);
}

// Delete a mock database
export async function mockDeleteDatabase(databaseId) {
	await mockDelay();

	const index = guestState.databases.findIndex((db) => db.id === databaseId);

	if (index === -1) {
		throw new Error("Database not found");
	}

	guestState.databases.splice(index, 1);

	// Delete the guestState related fields of the deleted database
	delete guestState.schemas[databaseId];
	delete guestState.rows[databaseId];

	return null;
}