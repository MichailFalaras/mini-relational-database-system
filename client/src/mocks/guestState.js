import { MOCK_DATABASES } from "./data/mockDatabases.js";
import { MOCK_TABLES, MOCK_INDEXES } from "./data/mockSchema.js";
import { MOCK_ROWS } from "./data/mockRows.js";


// Aggregate all separate database components into a unified Guest state object
function createGuestState() {
	return {
		databases: structuredClone(MOCK_DATABASES),

		schemas: {
			1: {
				tables: structuredClone(MOCK_TABLES),
				indexes: structuredClone(MOCK_INDEXES)
			}
		},

		rows: {
			1: structuredClone(MOCK_ROWS)
		}
	};
}

// Making the guest state object public for the mock services to use,
export let guestState = createGuestState();

// as well as its reset function when the mock dataset is updated
export function resetGuestState() {
	guestState = createGuestState();
}