import { useState } from "react";
import { X, AlertCircle, Loader2 } from "lucide-react";
import "./../styles/open-database-modal.css";


function OpenDatabaseModal({ target, onOpen, onCancel }) {
	const [databaseName, setDatabaseName] = useState(target?.name ?? "");
	const [error, setError] = useState("");
	const [opening, setOpening] = useState(false);


	// Open database
	async function handleOpenDatabase(event) {
		event.preventDefault();
		setError("");

		if (!databaseName.trim()) {
			setError("Database name is required.");
			return;
		}

		setOpening(true);

		try {
			await onOpen({ databaseName: databaseName.trim() });
		} finally {
			setOpening(false);
		}
	}

	
	return (
		<div id="open-modal-backdrop">
			<div id="open-database-modal">
				
				{/* Modal Header */}
				<div id="open-modal-header">
					<div>
						<h2>
							{target
								? `Open ${target.name}`
								: "Open Database"
							}
						</h2>

						<p>
							{target
								? "Open this database in BaseQL"
								: "Open an existing MiniDB database"
							}
						</p>
					</div>
					<button 
						id="close-open-modal"
						onClick={onCancel}
					>
						<X style={{ width: "1rem", height: "1rem" }} />
					</button>
				</div>

				{/* Open Existing Database Form */}
				<form id="open-modal-form" onSubmit={handleOpenDatabase}>
					<div>
						<label className="form-label">Database Name</label>
						<input 
							type="text"
							className="form-input"
							value={databaseName}
							disabled={!!target}
							onChange={(event) => setDatabaseName(event.target.value)}
						/>
					</div>

					{error && (
						<div id="open-form-error">
							<AlertCircle style={{width: "0.875rem", height: "0.875rem", flexShrink: "0", color: "#DC2626"}} />
							<span>{error}</span>
						</div>
					)}

					{/* Action Buttons*/}
					<div id="open-form-btns">
						<button id="cancel-btn" type="button" onClick={onCancel}>
							Cancel
						</button>
						<button 
							id="open-btn" 
							type="submit"
							disabled={opening}
						>
							{opening
								? <><Loader2 className="open-loader"/> Opening...</>
								: "Open →"
							}
						</button>
					</div>
				</form>
			</div>
		</div>
	);
}

export default OpenDatabaseModal;