import { guestState } from "./guestState.js";
import { mockDelay } from "./mockUtils.js";


// Return the schema of a mock database
export async function mockGetSchema(databaseId) {
	await mockDelay();

	const schema = guestState.schemas[databaseId];

	if (!schema) {
		throw new Error("Database not found");
	}

	return structuredClone(schema);
}