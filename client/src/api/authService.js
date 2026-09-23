import { apiRequest } from "./apiClient.js";

// User logs in
export function login(credentials) {
	return apiRequest("/auth/login", {
		method: "POST",
		body: JSON.stringify(credentials)	
	});
}

// User registers
export function register(userData) {
	return apiRequest("/auth/register", {
		method: "POST",
		body: JSON.stringify(userData)
	});
}

// User logs out
export function logout() {
	return apiRequest("/auth/logout", {
		method: "POST"
	});
}