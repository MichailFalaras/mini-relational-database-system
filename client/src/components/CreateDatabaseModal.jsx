import { useState } from "react";
import { X, AlertCircle, Loader2 } from "lucide-react";
import "./../styles/create-database-modal.css";


function CreateDatabaseModal({ onCreate, onCancel }) {
	const [databaseName, setDatabaseName] = useState("");
	const [error, setError] = useState("");
	const [creating, setCreating] = useState(false);


	async function handleCreateDatabase(event) {
		event.preventDefault();
		setError("");
		
		if (!databaseName?.trim()) { 
			setError("Database name is required."); 
			return; 
		}

		setCreating(true);
		
		try {
			await onCreate({ databaseName: databaseName.trim() });
		} finally {
			setCreating(false);
		}
	}


	return (
		<div id="create-modal-backdrop">
			<div id="create-database-modal">
				
				{/* Modal Header */}
				<div id="create-modal-header">
					<div>
						<h2>Create Database</h2>
						<p>Create a new MiniDB database</p>
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
							value={databaseName}
							onChange={(event) => setDatabaseName(event.target.value)}
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
						<button 
							id="create-btn" 
							type="submit"
							disabled={creating}
						>
							{creating
								? <><Loader2 className="create-loader"/> Creating...</>
								: "Create →"
							}
						</button>
					</div>
				</form>
			</div>
		</div>
	);
}

export default CreateDatabaseModal;