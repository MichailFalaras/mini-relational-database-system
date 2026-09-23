// Delay helper simulating an API request
export function mockDelay(ms = 500) {
	return new Promise((resolve) => setTimeout(resolve, ms));
}