import { guestState } from "./guestState.js";

export function mockGetCurrentUser() {
	return structuredClone(guestState.user);
}