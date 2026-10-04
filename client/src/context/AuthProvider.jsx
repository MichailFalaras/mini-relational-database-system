import { useEffect, useState } from "react";
import { AuthContext } from "./AuthContext";
import { setApiMode, API_MODES } from "../api/apiMode";
import { getAuthToken, setAuthToken, clearAuthToken } from "../api/authToken";
import { resetGuestState } from "../mocks/guestState";
import { 
	login as loginRequest,
	register as registerRequest,
	logout as logoutRequest, 
	getCurrentUser 
} from "../api/authService";


export function AuthProvider({ children }) {
	// Authentication state about the current user and authentication's loading status
	const [currentUser, setCurrentUser] = useState(null);
	const [authLoading, setAuthLoading] = useState(true);

	// Upon startup, check for potential existing token-credential
	// If user was in guest state, it always resets
	useEffect(() => {
		async function restoreAuthentication() {
			const token = getAuthToken();

			if (!token) {
				setAuthLoading(false);
				return;
			}

			try {
				setApiMode(API_MODES.USER);

				const user = await getCurrentUser();
				setCurrentUser(user);
			} catch {
				clearAuthToken();
				setCurrentUser(null);
			} finally {
				setAuthLoading(false);
			}
		}

		restoreAuthentication();
	}, []);

	// Check for expired tokens that were used in a request
	useEffect(() => {
		function handleAuthExpired() {
			clearAuthToken();
			setApiMode(API_MODES.NONE);
			setCurrentUser(null);
		}

		window.addEventListener("auth-expired", handleAuthExpired);

		return () => {
			window.removeEventListener("auth-expired", handleAuthExpired);
		};
	}, []);

	// Handle user login
	async function login(credentials) {
		const response = await loginRequest(credentials);

		setAuthToken(response.token);
		setApiMode(API_MODES.USER);
		setCurrentUser(response.user);
		
		return response;
	}

	// Handle guest login
	async function loginAsGuest() {
		resetGuestState();
		clearAuthToken();

		setApiMode(API_MODES.GUEST);

		const guest = await getCurrentUser();
		setCurrentUser(guest);
	}

	// Handle user register
	async function register(userData) {
		const response = await registerRequest(userData);

		setAuthToken(response.token);
		setApiMode(API_MODES.USER);
		setCurrentUser(response.user);

		return response;
	}

	// Handle both authenticated user and guest logout
	async function logout() {
		try {
			if (!currentUser?.isGuest) {
				await logoutRequest();
			}
		} finally {
			clearAuthToken();
			resetGuestState();
			setApiMode(API_MODES.NONE);
			setCurrentUser(null);
		}
	}

	

	return (
		<AuthContext.Provider value={{ currentUser, authLoading, login, loginAsGuest, register, logout }}>
			{children}
		</AuthContext.Provider>
	)
}