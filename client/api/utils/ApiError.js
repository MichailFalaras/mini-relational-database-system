// Standardized error format for API request failures

export class ApiError extends Error {
	constructor(message, status, data = null) {
		super(message);

		this.name = "ApiError";
		this.status = status;
		this.data = data;
	}
}