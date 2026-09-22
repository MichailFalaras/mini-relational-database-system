import { apiRequest } from "./apiClient.js";

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
	return apiRequest("/databases", {
		method: "GET"
	});
}

// User connects to a database
export function connectDatabase(databaseId, credentials) {
	return apiRequest(`/databases/${databaseId}/connect`, {
		method: "POST",
		body: JSON.stringify(credentials)
	});
}

// User creates a database
export function createDatabase(database) {
	return apiRequest(`/databases`, {
		method: "POST",
		body: JSON.stringify(database)
	});
}

// User deletes a database
export function deleteDatabase(databaseId) {
	return apiRequest(`/databases/${databaseId}`, {
		method: "DELETE"
	});
}