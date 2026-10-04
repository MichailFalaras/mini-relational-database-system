import { useState, useRef } from "react";
import { X, AlertCircle, Loader2, Database } from "lucide-react";
import "./../styles/open-database-modal.css";


function OpenDatabaseModal({ target, onOpen, onCancel }) {
	const [fileName, setFileName] = useState(target?.path?.split(/[\\/]/).pop() ?? "");
	const [filePath, setFilePath] = useState(target?.path ?? "");

	const [displayName, setDisplayName] = useState(target?.name ?? "");
	const [opening, setOpening] = useState(false);
	const [error, setError] = useState("");

	const fileInputRef = useRef(null);

	const isNew = target === null;

	// Open file explorer to browse for .db MiniDB database files
	function handleBrowse() {
		setError("");

		fileInputRef.current?.click();
	}

	// Handle database file selection
	function handleFileSelected(event) {
		const file = event.target.files?.[0];

		if (!file) {
			return;
		}

		if (!file.name.toLowerCase().endsWith(".db")) {
			setError("Select a MiniDB .db file");
			event.target.value = "";
			return;
		}

		setFileName(file.name);

		// Temporary browser limitation. This isn't an absolute filesystem path
		setFilePath(file.name);

		const suggestedName = file.name.replace(/\.db$/i, "");

		if (!displayName.trim() || displayName === target?.name) {
			setDisplayName(suggestedName);
		}

		setError("");

		// Allows selecting the same file again later
		event.target.value = "";
	}

	// Submit form and open selected database
	async function handleSubmit(event) {
		event.preventDefault();

		if (!filePath) {
			setError("Select a database file to continue.");
			return;
		}

		if (!displayName.trim()) {
			setError("Enter a display name for this database.");
			return;
		}

		setError("");
		setOpening(true);

		try {
			if (isNew) {
				await onOpen({
					filePath,
					displayName: displayName.trim()
				});
			} else {
				await onOpen({
					id: target.id
				});
			}
			
		} catch(error) {
			setError(error instanceof Error ? error.message : "Failed to open database");
		} finally {
			setOpening(false);
		}
	}

	return (
		<div id="open-database-modal-backdrop">
			<div id="open-database-modal">

				{/* Header */}
				<div id="open-database-modal-header">
					<div>
						<h2>{isNew ? "Open Database File": `Reopen ${target.name}`}</h2>
						<p>Select a local .db file to open</p>
					</div>

					<button 
						id="close-open-database-modal"
						type="button"
						onClick={onCancel}
					>
						<X style={{ width: "1rem", height: "1rem" }}/>
					</button>
				</div>

				<form id="open-database-form" onSubmit={handleSubmit}>

					{/* File Picker */}
					<div className="open-database-field">
						<label className="open-database-label">Database File</label>

						{/* Actual browser file input */}
						<input 
							ref={fileInputRef}
							id="database-file-input"
							type="file"
							accept=".db"
							onChange={handleFileSelected}
							hidden
						/>

						<button 
							id="database-file-picker"
							type="button"
							onClick={handleBrowse}
							className={fileName ? "has-file": ""}
						>
							{fileName
								? (
									<>
										<div className="database-file-icon selected">
											<Database />
										</div>

										<div className="database-file-info">
											<p className="database-file-name">{fileName}</p>
											<p className="database-file-description">Click to change file</p>
										</div>
									</>
								) : (
									<>
										<div className="database-file-icon">
											<Database />
										</div>

										<div className="database-file-info">
											<p className="database-file-prompt">Browse for file</p>
											<p className="database-file-description">.db files</p>
										</div>
									</>
								)
							}
						</button>
					</div>

					{/* Display Name */}
					<div className="open-database-field">
						<label 
							className="open-database-label"
							htmlFor="database-display-name"
						>
							Display Name
						</label>

						<input 
							id="database-display-name"
							value={displayName}
							onChange={(event) => setDisplayName(event.target.value)}
							placeholder="my_database"
						/>

						<p id="database-display-name-description">
							Shown in the database list — can differ from the filename
						</p>
					</div>

					{/* Error */}
					{error && (
						<div id="open-database-error">
							<AlertCircle style={{
									width: "0.875rem",
    								height: "0.875rem",
    								flexShrink: "0",
    								color: "#dc2626"
								}}
							/>
							<span>{error}</span>
						</div>
					)}

					{/* Action Buttons */}
					<div id="open-database-actions">
						<button
							id="cancel-open-database-btn"
							type="button"
							onClick={onCancel}
						>
							Cancel
						</button>

						<button
							id="open-database-btn"
							type="submit"
							disabled={opening || !fileName}
						>
							{opening ? (
								<>
									<Loader2 className="loader-icon" />
									Opening…
								</>
							) : (
								"Open →"
							)}
						</button>
					</div>
				</form>
			</div>
		</div>
	);
}

export default OpenDatabaseModal;