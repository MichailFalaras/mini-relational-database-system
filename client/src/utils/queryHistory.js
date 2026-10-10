// We got to keep an upper bound on the query history entries
export const MAX_HISTORY_ENTRIES = 100;

// Value used to access and store the history in Local Storage
const QUERY_HISTORY_KEY = "baseql-query-history";


export function loadQueryHistory() {
	const storedHistory = localStorage.getItem(QUERY_HISTORY_KEY);

	if (!storedHistory) {
		return [];
	}

	try {
		const parsedHistory = JSON.parse(storedHistory);

		return parsedHistory.map((entry) => ({
			...entry,
			executedAt: new Date(entry.executedAt)
		}));
	} catch (error) {
		return [];
	}
}

export function saveQueryHistory(history) {
	localStorage.setItem(QUERY_HISTORY_KEY, JSON.stringify(history));
}