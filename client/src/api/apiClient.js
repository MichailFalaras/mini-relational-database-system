import { ApiError } from "./utils/ApiError.js";

const BASE_URL = import.meta.env.VITE_API_BASE_URL ?? "http://localhost:3000/api";


/* Performs a request to the BaseQL backend API.
 
  Successful responses are parsed as JSON and returned unchanged.
  HTTP, network, and request-level failures are normalized as ApiError.
  Requests returning HTTP 204 resolve to null.
 */
export async function apiRequest(endpoint, options = {}) {
	try {
		const response = await fetch(`${BASE_URL}${endpoint}`, {
			...options,
			headers: {
				"Content-Type": "application/json",
				...options.headers
			}
		});

		let data = null;

		// Determine if we have a response body (success or failure at this point)
		if (response.status !== 204) {
			data = await response.json();	
		}

		// Request failure
		if (!response.ok) {
			throw new ApiError(data?.message ?? "Request failed", response.status, data);
		}

		return data;
	} catch (error) {
		// Already normalized API error
		if (error instanceof ApiError) {
			throw error;
		}

		// Network or other request-level failures
		throw new ApiError(error.message ?? "Unable to reach the server", null);
	}
}