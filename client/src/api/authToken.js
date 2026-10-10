const TOKEN_KEY = "baseql_auth_token";

export function getAuthToken() {
	return localStorage.getItem(TOKEN_KEY);
}

export function setAuthToken(token) {
	localStorage.setItem(TOKEN_KEY, token);
}

export function clearAuthToken() {
	localStorage.removeItem(TOKEN_KEY);
}