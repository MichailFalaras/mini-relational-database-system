import { useState } from "react";
import { X, Eye, EyeOff, AlertCircle, Loader2 } from "lucide-react";
import "./../styles/connect-database-modal.css";


function ConnectDatabaseModal({ target, onConnect, onCancel }) {
	const [form, setForm] = useState({
		databaseName: target?.name ?? "",
		username: target?.user ?? "",
		password: ""
	});
	const [showPassword, setShowPassword] = useState(false);
	const [error, setError] = useState("");
	const [connecting, setConnecting] = useState(false);


	// Submit form and connect to the database
	async function handleConnectDatabase(event) {
		event.preventDefault();
		setError("");

		if (!form.databaseName.trim()) {
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

		setConnecting(true);

		try {
			await onConnect({
				databaseName: form.databaseName,
				username: form.username,
				password: form.password
			});
		} finally {
			setConnecting(false);
		}
	}

	
	return (
		<div id="connect-modal-backdrop">
			<div id="connect-database-modal">
				
				{/* Modal Header */}
				<div id="connect-modal-header">
					<div>
						<h2>
							{target 
								? `Connect to ${target?.name}`
								: "New Connection"
							}
						</h2>
						<p>Enter your credentials to connect</p>
					</div>
					<button 
						id="close-connect-modal"
						onClick={onCancel}
					>
						<X style={{ width: "1rem", height: "1rem" }} />
					</button>
				</div>

				{/* Connect to Existing Database Form */}
				<form id="connect-modal-form" onSubmit={handleConnectDatabase}>
					<div>
						<label className="form-label">Database Name</label>
						<input 
							type="text"
							className="form-input"
							value={form?.databaseName}
							disabled={!!target}
							onChange={(event) => 
								setForm((prev) => ({
									...prev,
									databaseName: event.target.value,
								})
							)}
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

					{error && (
						<div id="connect-form-error">
							<AlertCircle style={{width: "0.875rem", height: "0.875rem", flexShrink: "0", color: "#DC2626"}} />
							<span>{error}</span>
						</div>
					)}

					{/* Action Buttons*/}
					<div id="connect-form-btns">
						<button id="cancel-btn" type="button" onClick={onCancel}>
							Cancel
						</button>
						<button id="connect-btn" type="submit">
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

export default ConnectDatabaseModal;