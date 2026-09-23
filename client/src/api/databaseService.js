import { apiRequest } from "./apiClient.js";
import { 
	mockGetDatabases, 
	mockConnectDatabase, 
	mockCreateDatabase, 
	mockDeleteDatabase } from "../mocks/mockDatabaseService.js";
import { isGuestMode } from "./apiMode.js";

/* 
  Retrieves all databases. The response format is:
  [
  	{
		id: 1,
		name: "e_commercedb",
		status: "connected",
		user: "apostolis",
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

// User connects to a database
export function connectDatabase(databaseId, credentials) {
	if (isGuestMode()) {
		return mockConnectDatabase(databaseId);
	}

	return apiRequest(`/databases/${databaseId}/connect`, {
		method: "POST",
		body: JSON.stringify(credentials)
	});
}

// User creates a database
export function createDatabase(database) {
	if (isGuestMode()) {
		return mockCreateDatabase(database);
	}

	return apiRequest(`/databases`, {
		method: "POST",
		body: JSON.stringify(database)
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