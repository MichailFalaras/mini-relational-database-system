export const API_MODES = {
	USER: "user",
	GUEST: "guest",
	NONE: "none"
};

let apiMode = API_MODES.NONE;


/* API Mode utilities */

export function setApiMode(mode) {
	if (mode !== API_MODES.USER && mode !== API_MODES.GUEST && mode !== API_MODES.NONE) {
		throw new Error(`Invalid API mode ${mode}`);
	}

	apiMode = mode;
}

export function getApiMode() {
	return apiMode;
}

export function isGuestMode() {
	return apiMode === API_MODES.GUEST;
}

