import { apiRequest } from "./apiClient.js";
import { isGuestMode } from "./apiMode.js";
import { mockGetSchema } from "../mocks/mockSchemaService.js";

/*
 User fetches the schema (table schema and indexes) of a database. The response format is:
 {
   tables: [
		{
			name: "users",
			rowCount: 14823,
			columns: [
				{ name: "id", type: "INT", nullable: false, pk: true },
				{ name: "username", type: "VARCHAR(100)", nullable: false, pk: false, constraints: ["UNIQUE", "CHECK (LENGTH(username) >= 3)"] },
				{ name: "user_id", type: "INT", nullable: false, pk: false, fk: "users.id" }
			]
		}
   ],
   indexes: [
		{
			name: "idx_users_email",
			table: "users",
			columns: ["email"],
			unique: true
		}
   ]
 }
*/
export function getSchema(database) {
	if (isGuestMode() || database?.isDemo === true) {
		return mockGetSchema(database.id);
	}

	return apiRequest(`/databases/${database.id}/schema`, {
		method: "GET"
	});
}