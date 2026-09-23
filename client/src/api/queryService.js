import { apiRequest } from "./apiClient.js";
import { mockExecuteQuery } from "../mocks/mockQueryService.js";
import { isGuestMode } from "./apiMode.js";


/* 
  Executes an SQL statement against a database.
 
  Expected successful response shapes:
 
  SELECT:
   {
    type: "select",
    columns: ["id", "email", "salary"],
    rows: [
      { id: 1, email: "user1@example.com", salary: 25000 }
    ],
    executionTime: 50
  }
 
  INSERT / UPDATE / DELETE:
   {
    type: "mutation",
    rowsAffected: 10,
    executionTime: 50
  }
 
  DDL:
  {
    type: "ddl",
    executionTime: 50
  }
 */
export function executeQuery(databaseId, sql) {
    if (isGuestMode()) {
        return mockExecuteQuery(databaseId, sql);
    } 

	return apiRequest("/query", {
		method: "POST",
		body: JSON.stringify({
			databaseId,
			sql
		})
	});
}