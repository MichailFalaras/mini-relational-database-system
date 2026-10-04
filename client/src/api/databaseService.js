import { apiRequest } from "./apiClient.js";
import { 
	mockGetDatabases, 
	mockOpenDatabase,
	mockCloseDatabase, 
	mockCreateDatabase, 
	mockDeleteDatabase } from "../mocks/mockDatabaseService.js";
import { isGuestMode } from "./apiMode.js";

/* 
  Retrieves all databases. The response format is:
  [
  	{
		id: 1,
		name: "e_commercedb",
		path: "demo/ecommerce.db",
		status: "open" | "closed",
		isDemo: true | false,
		numTables: 8,
		size: "7.2 MB"
  	}
  ]     
*/
export function getDatabases() {
	if (isGuestMode()) {
		return mockGetDatabases();
	}

	return apiRequest("/databases", {
		method: "GET"
	});
}

// User opens to a database
export function openDatabase(form) {
	if (isGuestMode()) {
		return mockOpenDatabase(form);
	}

	// Reopen known existing database
	if (form.id != null) {
		return apiRequest(`/databases/${form.id}/open`, {
			method: "POST"
		});
	}

	// Open and register external MiniDB file
	return apiRequest("/databases/open", {
		method: "POST",
		body: JSON.stringify({
			filePath: form.filePath,
			displayName: form.displayName
		})
	});
}

// User closes the current database
export function closeDatabase(databaseId) {
	if (isGuestMode()) {
		return mockCloseDatabase(databaseId);
	}

	return apiRequest(`/databases/${databaseId}/close`, {
		method: "POST"
	});
}

// User creates a database
export function createDatabase(form) {
	if (isGuestMode()) {
		return mockCreateDatabase(form);
	}

	return apiRequest(`/databases`, {
		method: "POST",
		body: JSON.stringify(form)
	});
}

// User deletes a database
export function deleteDatabase(databaseId) {
	if (isGuestMode()) {
		return mockDeleteDatabase(databaseId);
	}

	return apiRequest(`/databases/${databaseId}`, {
		method: "DELETE"
	});
}

// Open demo database when user is authenticated
export async function openDemoDatabase() {
	const databases = await mockGetDatabases();

	const demo = databases.find((db) => db.id === 1) ?? null;

	if (!demo) {
		throw new Error("Demo database not found");
	}

	return mockOpenDatabase({
		id: demo.id
	});
}