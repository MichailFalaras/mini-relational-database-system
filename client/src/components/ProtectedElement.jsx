import { Navigate } from "react-router-dom"; 
import { useAuth } from "../hooks/useAuth.jsx";

function ProtectedElement({ children }) {
	const { currentUser, authLoading } = useAuth();

	if (authLoading) {
		return null;
	}

	if (!currentUser) {
		return <Navigate to="/auth" replace />
	}

	return children;
}

export default ProtectedElement;