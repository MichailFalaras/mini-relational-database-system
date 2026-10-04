import { useContext } from "react";
import { AuthContext } from "../context/AuthContext.jsx";

// Custom hook any single component can use to access authentication information
export function useAuth() {
	const context = useContext(AuthContext);
	return context;
}