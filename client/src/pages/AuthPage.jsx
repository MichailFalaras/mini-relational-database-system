import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { AlertCircle, ArrowRight, Check, Database, Eye, EyeOff, Loader2, Terminal } from "lucide-react";
import "./../styles/auth.css";


const BRAND_DETAILS = [
	"Create and manage MiniDB databases",
	"SQL editor with syntax highlighting",
	"Explore tables, indexes & query history",
	"User accounts and secure sessions"
];


function AuthPage() {
	const [mode, setMode] = useState("login");

	// Login Form state
	const [loginEmail, setLoginEmail] = useState("");
	const [loginPassword, setLoginPassword] = useState("");
	const [showLoginPassword, setShowLoginPassword] = useState(false);
	const [loginLoading, setLoginLoading] = useState(false);
	const [loginError, setLoginError] = useState("");

	// Register Form state
	const [fullName, setFullName] = useState("");
	const [email, setEmail] = useState("");
	const [password, setPassword] = useState("");
	const [confirmPassword, setConfirmPassword] = useState("");
	const [showRegisterPassword, setShowRegisterPassword] = useState(false);
	const [registerLoading, setRegisterLoading] = useState(false);
	const [registerError, setRegisterError] = useState("");

	const navigate = useNavigate();


	// Performs API request to login
	async function handleLogin(event) {
		event.preventDefault();

		setLoginError("");
		setLoginLoading(true);

		// TODO: API Request

		setLoginLoading(false);
	}

	// Performs API request to register
	async function handleRegister(event) {
		event.preventDefault();

		setRegisterError("");

		// Validate input data
		// i) Full name existence
		// ii) Email existence and correct format
		/// iii) Password length and matching confirm password 
		if (!fullName.trim()) { setRegisterError("Full name is required."); return; }

		if (!email.trim()) { setRegisterError("Email is required."); return; }

		if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) { setRegisterError("Enter a valid email address."); return; }

		if (!password.trim()) { setRegisterError("Password is required."); return; }

		if (password.length < 8) { setRegisterError("Password must be at least 8 characters."); return; }

		if (!confirmPassword.trim()) { setRegisterError("Confirmation password is required."); return; }
		
		if (password !== confirmPassword) { setRegisterError("Passwords don't match."); return; }

		setRegisterLoading(true);

		// TODO: API Request

		setRegisterLoading(false);
	}

	// Guest login lets users access a demo database
	function handleGuestLogin() {
		navigate("/home");
	}

	return (
		<div id="auth-page">
			{/* Authentication Container */}
			<main>

				{/* Left Brand Panel */}
				<div id="brand-panel">
					{/* Header */}
					<div>
						<div id="brand-name-logo">
							<div>
								<Database style={{width: "1rem", height: "1rem", color: "#FFFFFF"}} />
							</div>
							<span>BaseQL</span>
						</div>

						<h1 id="brand-title">A modern interface<br />for MiniDB</h1>
						<p id="brand-description">
							Create, query, and explore relational databases powered by our custom database engine.
						</p>
					</div>

					{/* Brand Details */}
					<div id="brand-details">
						{BRAND_DETAILS.map((detail, index) => (
							<div key={index} className="detail">
								<div>
									<Check style={{width: "0.625rem", height: "0.625rem", color: "#FFFFFF" }} />
								</div>
								<span>{detail}</span>
							</div>
						))}
					</div>

					{/* Demo Login Credentials */}
					<div id="brand-demo-login">
						<p>BaseQL — powered by the MiniDB relational engine</p>
					</div>
				</div>
				
				{/* Right Login/Signup Panel */}
				<div id="auth-panel">
					{mode === "login" ? (
						<form onSubmit={handleLogin}>
							{/* Form Header */}
							<div className="form-header">
								<h2>Welcome back</h2>
								<p>Sign in to your BaseQL account</p>
							</div>

							<div className="form-fields">
								<div>
									<label className="form-label">Email</label>
									<input 
										className="form-input"
										placeholder="you@example.com"
										type="email"
										value={loginEmail}
										onChange={(event) => setLoginEmail(event.target.value)}
									/>
								</div>

								<div>
									<label className="form-label">Password</label>
									<div style={{position: "relative"}}>
										<input 
											className="form-input"
											type={showLoginPassword ? "text": "password"}
											placeholder="•••••••••••••••••••"
											value={loginPassword}
											onChange={(event) => setLoginPassword(event.target.value)}
										/>
										<button
											className="view-password-btn"
											type="button"
											onClick={() => setShowLoginPassword((prev) => !prev)}
										>
											{showLoginPassword 
												? <EyeOff style={{width: "1rem", height: "1rem"}}/> 
												: <Eye style={{width: "1rem", height: "1rem"}}/>
											}
										</button>
									</div>
								</div>
							</div>

							{/* Login error */}
							{loginError && (
								<div className="error-msg">
									<AlertCircle style={{ width: "1rem", height: "1rem", flexShrink: 0, color: "#DC2626"}} />
									<span>{loginError}</span>
								</div>
							)}

							<button
								type="submit"
								className="form-submit-btn"
								disabled={loginLoading}	
							>
								{loginLoading
									? <><Loader2 className="loader-icon"/>Signing in</>
									: <>Sign in <ArrowRight /></>
								}
							</button>

							<p className="footer-msg">
								{"Don't have an account "} 
								<button
									type="button"
									onClick={() => setMode("register")}
									className="switch-mode-btn"
								>
									Create One
								</button>
							</p>

							{/* Separating normal login from guest login */}
							<div id="login-separator">
								<div />
								<span>or</span>
								<div />
							</div>
							
							<button 
								type="button"
								id="guest-login-btn"
								onClick={handleGuestLogin}
							>
								<Terminal style={{ width: "1rem", height: "1rem", color: "#9CA3AF"}}/>
								Continue as guest
							</button>

						</form>
					) : (
						<form onSubmit={handleRegister}>
							<div className="form-header">
								<h2>Create your account</h2>
								<p>Get started with BaseQL for free</p>
							</div>

							<div className="form-fields">

								<div>
									<label className="form-label">Full Name</label>
									<input 
										type="text"
										placeholder="John Doe"
										value={fullName}
										onChange={(event) => setFullName(event.target.value)}
										className="form-input"
									/>
								</div>

								<div>
									<label className="form-label">Email</label>
									<input 
										type="text"
										placeholder="you@example.com"
										value={email}
										onChange={(event) => setEmail(event.target.value)}
										className="form-input"
									/>
								</div>

								<div id="register-passwords">
									<div>
										<label className="form-label">Password</label>
										<div style={{position: "relative"}}>
											<input 
												className="form-input"
												type={showRegisterPassword ? "text": "password"}
												placeholder="Min. 8 characters"
												value={password}
												onChange={(event) => setPassword(event.target.value)}
											/>
											<button
												className="view-password-btn"
												type="button"
												onClick={() => setShowRegisterPassword((prev) => !prev)}
											>
												{showRegisterPassword 
													? <EyeOff style={{width: "1rem", height: "1rem"}}/> 
													: <Eye style={{width: "1rem", height: "1rem"}}/>
												}
											</button>
										</div>
									</div>

									<div>
										<label className="form-label">Confirm</label>
										<input
											type="password" 
											className="form-input"
											placeholder="Repeat password"
											value={confirmPassword}
											onChange={(event) => setConfirmPassword(event.target.value)}
										/>
									</div>
								</div>
							</div>
							
							{/* Registration error */}
							{registerError && (
								<div className="error-msg">
									<AlertCircle style={{ width: "1rem", height: "1rem", flexShrink: 0, color: "#DC02626"}} />
									<span>{registerError}</span>
								</div>
							)}

							<button
								type="submit"
								className="form-submit-btn"
								disabled={registerLoading}	
							>
								{registerLoading
									? <><Loader2 className="loader-icon"/>Creating account...</>
									: <>Create account <ArrowRight /></>
								}
							</button>

							<p className="footer-msg">
								{"Already have an account? "} 
								<button
									type="button"
									onClick={() => setMode("login")}
									className="switch-mode-btn"
								>
									Sign in
								</button>
							</p>
						</form>
					)}
				</div>
			</main>


		</div>
	);
}

export default AuthPage; 