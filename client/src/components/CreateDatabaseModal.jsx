import { useState } from "react";
import { X, AlertCircle, Loader2, Download } from "lucide-react";
import "./../styles/create-database-modal.css";


function CreateDatabaseModal({ onCreate, onCancel }) {
	const [name, setName] = useState("");
	const [location, setLocation] = useState("");
	const [error, setError] = useState("");
	const [creating, setCreating] = useState(false);

	const cleanName = name.trim().replace(/\s+/g, "_").toLowerCase();

	function handleNameChange(event) {
		setName(event.target.value);
		setError("");
	}

	function handleBrowse() {
		if (!cleanName) {
			setError("Enter a database name first.");
			return;
		}

		setError("");

		// TODO: Open native save-location picker.
	}

	async function handleSubmit(event) {
		event.preventDefault();

		setError("");
		
		if (!cleanName) { 
			setError("Database name is required."); 
			return; 
		}

		if (!/^[a-z_][a-z0-9_]*$/.test(cleanName)) {
			setError("Use only lowercase letters, digits, and underscores.");
			return;
		}

		setCreating(true);
		
		try {
			await onCreate({ 
				databaseName: cleanName,
				location
			});
		} catch (error) {
			setError(error instanceof Error ? error.message : "Failed to create database.");
		} finally {
			setCreating(false);
		}
	}


	return (
		<div id="create-database-modal-backdrop">
			<div id="create-database-modal">

				{/* Header */}
				<div id="create-database-modal-header">
					<div>
						<h2>New Database</h2>
						<p>Create a new local MiniDB database file</p>
					</div>

					<button
						id="close-create-database-modal"
						type="button"
						onClick={onCancel}
					>
						<X style={{ width: "1rem", height: "1rem" }}/>
					</button>
				</div>

				<form id="create-database-form" onSubmit={handleSubmit}>

					{/* Database Name */}
					<div className="create-database-field">
						<label
							className="create-database-label"
							htmlFor="new-database-name"
						>
							Database Name
						</label>

						<input
							id="new-database-name"
							value={name}
							onChange={handleNameChange}
							autoFocus
							placeholder="my_new_database"
						/>
					</div>

					{/* Save Location */}
					<div className="create-database-field">
						<label className="create-database-label">
							Save Location
						</label>

						<button
							id="database-location-picker"
							type="button"
							onClick={handleBrowse}
							className={location ? "has-location" : ""}
						>
							<Download style={{
								width: "0.875rem", 
								height: "0.875rem", 
								flexShrink: "0", 
								color: "#9CA3AF"
								}}
							/>
							
							<span id="database-location-text">
								{location || "Choose save location…"}
							</span>
						</button>

						{!location && cleanName && (
							<p id="database-default-location-description">
								Will default to{" "}
								<span>{cleanName}.db</span>{" "}
								in the working directory
							</p>
						)}
					</div>

					{/* Error */}
					{error && (
						<div id="create-database-error">
							<AlertCircle style={{
									width: "0.875rem",
									height: "0.875rem",
									flexShrink: "0",
									color: "#DC2626"
								}}
							/>
							<span>{error}</span>
						</div>
					)}

					{/* Actions */}
					<div id="create-database-actions">
						<button id="cancel-create-database-btn" type="button" onClick={onCancel}>
							Cancel
						</button>

						<button id="create-database-btn" type="submit">
							{creating ? (
								<>
									<Loader2 className="loader-icon"/>
									Creating...
								</>
							) : (
								"Create →"
							)}
						</button>
					</div>
				</form>
			</div>
		</div>
	);
}

export default CreateDatabaseModal;