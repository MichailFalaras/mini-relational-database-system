import { useState } from "react";
import { X, Eye, EyeOff, AlertCircle, Loader2 } from "lucide-react";
import "./../styles/create-database-modal.css";


function CreateDatabaseModal({ onCreate, onCancel }) {
	const [form, setForm] = useState({
		databaseName: "",
		username: "",
		password: "",
		confirmPassword: ""
	});
	const [showPassword, setShowPassword] = useState(false);
	const [error, setError] = useState("");
	const [connecting, setConnecting] = useState(false);


	async function handleCreateDatabase(event) {
		event.preventDefault();
		setError("");
		
		if (!form?.databaseName?.trim()) { 
			setError("Database name is required."); 
			return; 
		}

		if (!form?.username?.trim()) { 
			setError("Username is required."); 
			return; 
		}

		if (!form?.password?.trim()) { 
			setError("Password is required."); 
			return; 
		}

		if (form?.password?.length < 8) { 
			setError("Password must be at least 8 characters."); 
			return; 
		}

		if (!form?.confirmPassword.trim()) { 
			setError("Confirmation password is required."); 
			return; 
		}
		
		if (form?.password !== form?.confirmPassword) { 
			setError("Passwords don't match."); 
			return; 
		}

		setConnecting(true);
		
		try {
			await onConnect({
				databaseName: form.databaseName.trim(),
				username: form.username.trim(),
				password: form.password.trim()
			});
		} finally {
			setConnecting(false);
		}
	}


	return (
		<div id="create-modal-backdrop">
			<div id="create-database-modal">
				
				{/* Modal Header */}
				<div id="create-modal-header">
					<div>
						<h2>New Connection</h2>
						<p>Configure a new database connection</p>
					</div>
					<button 
						id="close-create-modal"
						onClick={onCancel}
					>
						<X style={{ width: "1rem", height: "1rem" }} />
					</button>
				</div>

				{/* New Database Form Creation */}
				<form id="create-modal-form" onSubmit={handleCreateDatabase}>
					<div>
						<label className="form-label">Database Name</label>
						<input 
							type="text"
							className="form-input"
							value={form?.databaseName}
							onChange={(event) => 
								setForm((prev) => ({
									...prev,
									databaseName: event.target.value
								}))
							}
						/>
					</div>
					<div>
						<label className="form-label">Username</label>
						<input 
							type="text"
							className="form-input"
							value={form?.username}
							onChange={(event) => 
								setForm((prev) => ({
									...prev,
									username: event.target.value
								}))
							}
						/>
					</div>
					<div>
						<label className="form-label">Password</label>
						<div style={{ position: "relative"}}>

							<input 
								type={showPassword ? "text" : "password"}
								className="form-input"
								style={{ paddingRight: "36px" }}
								value={form?.password}
								onChange={(event) => 
									setForm((prev) => ({
										...prev,
										password: event.target.value
									}))
								}
							/>
							<button 
								type="button"
								id="show-password-btn"
								onClick={() => setShowPassword((prev) => !prev)}
							>
								{showPassword 
									? <EyeOff style={{width: "0.875rem", height: "0.875rem"}}/> 
									: <Eye style={{width: "0.875rem", height: "0.875rem"}}/>
								}
							</button>
						</div>
					</div>
					<div>
						<label className="form-label">Confirm Password</label>
						<input 
							type="password"
							className="form-input"
							value={form?.confirmPassword}
							onChange={(event) => 
								setForm((prev) => ({
									...prev,
									confirmPassword: event.target.value
								}))
							}
						/>
					</div>

					{error && (
						<div id="create-form-error">
							<AlertCircle style={{width: "0.875rem", height: "0.875rem", flexShrink: "0", color: "#DC2626"}} />
							<span>{error}</span>
						</div>
					)}

					{/* Action Buttons*/}
					<div id="create-form-btns">
						<button id="cancel-btn" type="button" onClick={onCancel}>
							Cancel
						</button>
						<button id="create-btn" type="submit">
							{connecting
								? <><Loader2 className="connect-loader"/> Connecting...</>
								: "Connect →"
							}
						</button>
					</div>
				</form>
			</div>
		</div>
	);
}

export default CreateDatabaseModal;