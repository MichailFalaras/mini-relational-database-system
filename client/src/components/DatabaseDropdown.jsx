import { useState } from "react";
import { Trash2, Check, ArrowRight, Plus, Database } from "lucide-react";
import "./../styles/database-dropdown.css";

const ACTIVE_ID = 1;

function DatabaseDropdown({ connections, activeConnId, onSelect, onConnectExisting, onClose, onDelete, onNewConnection, onCreateDatabase }) {
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
					{connections.map((conn) => {
						const isActive = conn.id === activeConnId;

						{/* Displaying styles for options that the user clicked the trash icon */}
						if (confirmId === conn.id) {
							return (
								<div key={conn.id} className="to-be-deleted-option">
									<Trash2 style={{ width: "0.875rem", height: "0.875rem", flexShrink: "0", color: "#DC2626" }}/>
									<span>
										Remove <strong>{conn.name}</strong>?
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
											onDelete(conn.id);
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
								key={conn.id} 
								className={`database-option ${isActive ? "is-active" : ""}`}
								onClick={() => {
									if (conn.status === "connected") {
										onSelect(conn.id);
										onClose();
									}
									else {
										onConnectExisting(conn);
									}
								}}
							>
								<div className={`active-db-option ${conn.status === "connected" ? "connected" : ""}`} />
								
								<div className="option-info">
									<div className="db-name">
										<span className={isActive ? "is-active" : ""}>
											{conn.name}
										</span>
										{isActive && <Check style={{width: "0.75rem", height: "0.75rem", flexShrink: "0", color: "#4F46E5"}} />}
									</div>
									<div className="db-user">
										{conn.user ? `${conn.user}@` : ""}
									</div>
								</div>
								
								{conn.status === "disconnected" && (
									<span className="disconnected-dbs">Connect <ArrowRight style={{width: "0.75rem", height: "0.75rem", flexShrink: "0", color: "#4F46E5"}}/> </span>
								)}

								{!isActive && (
									<button className="delete-disconnected-option">
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
						onClick={() => { onNewConnection(); onClose(); }}
					>
						<Plus style={{width: "0.875rem", height: "0.875rem", color: "#9CA3AF"}}/> 
						New Connection
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