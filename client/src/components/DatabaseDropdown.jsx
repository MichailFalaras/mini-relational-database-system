import { useState } from "react";
import { Trash2, Check, ArrowRight, Plus, Database } from "lucide-react";
import "./../styles/database-dropdown.css";



function DatabaseDropdown({ databases, activeDatabaseId, onOpenDatabase, onSelectDatabase,
    onClose, onDeleteDatabase, onCreateDatabase
}) {
	// State that keeps track of the "to-be-deleted" database (User clicked trash icon)
	const [confirmId, setConfirmId] = useState(null);

	return (
		<>
			<div id="dropdown-backdrop" onClick={onClose} />

			<div id="database-dropdown">
				<div id="dropdown-header">
					<span>Database</span>
				</div>

				<div id="dropdown-options">
					{databases.map((db) => {
						const isActive = db.id === activeDatabaseId;

						{/* Displaying styles for options that the user clicked the trash icon */}
						if (confirmId === db.id) {
							return (
								<div key={db.id} className="to-be-deleted-option">
									<Trash2 style={{ width: "0.875rem", height: "0.875rem", flexShrink: "0", color: "#DC2626" }}/>
									<span>
										Remove <strong>{db.name}</strong>?
									</span>

									<button 
										className="cancel-delete-option"
										onClick={() => setConfirmId(null)}
									>
										Cancel
									</button>

									<button 
										className="confirm-delete-option"
										onClick={() => {
											onDeleteDatabase(db.id);
											setConfirmId(null);
										}}
									>
										Remove
									</button>
								</div>
							);
						} 

						return (
							<div 
								key={db.id} 
								className={`database-option ${isActive ? "is-active" : ""}`}
								onClick={() => {
									if (db.status === "open") {
										onSelectDatabase(db.id);
										onClose();
									}
									else {
										onOpenDatabase(db);
									}
								}}
							>
								<div className={`active-db-option ${db.status === "open" ? "open" : ""}`} />
								
								<div className="option-info">
									<div className="db-name">
										<span className={isActive ? "is-active" : ""}>
											{db.name}
										</span>
										{isActive && <Check style={{width: "0.75rem", height: "0.75rem", flexShrink: "0", color: "#4F46E5"}} />}
									</div>
								</div>
								
								{db.status === "closed" && (
									<span className="closed-dbs">Open <ArrowRight style={{width: "0.75rem", height: "0.75rem", flexShrink: "0", color: "#4F46E5"}}/> </span>
								)}

								{!isActive && (
									<button 
										className="remove-database-option"
										onClick={(event) => {
											event.stopPropagation();
											setConfirmId(db.id);
										}}
									>
										<Trash2 style={{width: "0.75rem", height: "0.75rem"}}/>
									</button>
								)}
							</div>
						);
					})}
				</div>

				<div id="dropdown-actions">
					<button 
						className="dropdown-btn"
						onClick={() => { 
							onOpenDatabase(null);
							onClose(); 
						}}
					>
						<Plus style={{width: "0.875rem", height: "0.875rem", color: "#9CA3AF"}}/> 
						Open Database...
					</button>
					<button 
						className="dropdown-btn"
						onClick={() => { onCreateDatabase(); onClose(); }}
					>
						<Database style={{width: "0.875rem", height: "0.875rem", color: "#9CA3AF"}}/> 
						Create Database
					</button>
				</div>
			</div>
		</>
	)
}

export default DatabaseDropdown;