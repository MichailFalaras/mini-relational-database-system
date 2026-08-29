import { useState } from "react";
import { ChevronDown, Database, RefreshCcw, Settings } from "lucide-react";
import DatabaseDropdown from "./DatabaseDropdown.jsx";
import UserMenu from "./UserMenu.jsx";
import "./../styles/header.css";

function Header({ connections, activeConn, onConnect, onSelectConnection, onDeleteConnection, 
	onNewConnection, onCreateDatabase, isRefreshing, onRefresh, showSettings, setShowSettings, onSignOut }) {
	
	const [showDropdown, setShowDropdown] = useState(false);


	return (
		<header id="header">

			{/* App Logo + Name */}
			<div id="app-logo">
				<div>
					<Database style={{width: "1rem", height: "1rem", color: "#FFFFFF"}} />
				</div>
				<span>BaseQL</span>
			</div>

			{/* Vertical separator line */}
			<div className="vertical-sep"/>

			{/* Database Select Dropdown */}
			<div id="db-dropdown">
				<button 
					id="db-dropdown-btn"
					className={`${showDropdown ? "show-dropdown" : ""} ${activeConn ? "active-db" : ""}`}
					onClick={() => setShowDropdown((prev) => !prev)}
				>

					{activeConn
						? <div 
							id="active-db-signal"
							className={activeConn ? "connected" : ""}
						/>
						: <Database style={{ width: "1rem", height: "1rem", color: "#C4CAD4" }}/>
					}
					<span 
						id="active-db-name"
						className={activeConn ? "active" : ""}
					>
						{activeConn?.name ?? "Select database"}
					</span>

					<ChevronDown id="db-dropdown-chevron" className={showDropdown ? "show-dropdown" : ""}/>
				</button>
				
				{showDropdown && (
					<DatabaseDropdown
						connections={connections} 
						activeConnId={activeConn?.id}
						onConnect={onConnect}
						onSelectConnection={onSelectConnection}
						onDeleteConnection={onDeleteConnection}
						onNewConnection={onNewConnection}
						onCreateDatabase={onCreateDatabase}
						onClose={() => setShowDropdown(false)}
					/>
				)}
			</div>

			{activeConn && (
				<>
					<span id="database-overview">
						{activeConn?.numTables}{" Tables "}•{" "}{activeConn?.size}
					</span>
					
					<span id="database-user">as{" "}<span>{activeConn?.user}</span></span>
				</>
			)}

			<div id="header-actions">
				<button
					title="Refresh Schema" 
					id="refresh-btn" 
					className={isRefreshing ? "refreshing" : ""}
					disabled={isRefreshing}
					onClick={onRefresh}
				>
					<RefreshCcw 
						style={{ width: "0.875rem", height: "0.875rem" }}
						className={isRefreshing ? "refresh-icon" : ""} 
					/>
				</button>
				<button 
					title="Settings"
					id="settings-btn"
					className={showSettings ? "active": ""}
					onClick={setShowSettings}
				>
					<Settings style={{ width: "0.875rem", height: "0.875rem", }} />
				</button>
				
				<div className="vertical-sep"/>

				<UserMenu onSignOut={onSignOut}/>
			</div>
		</header>
	);
}

export default Header;