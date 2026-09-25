import { useState } from "react";
import { ChevronDown, Database, RefreshCcw, Settings } from "lucide-react";
import DatabaseDropdown from "./DatabaseDropdown.jsx";
import UserMenu from "./UserMenu.jsx";
import "./../styles/header.css";

function Header({ currentUser, databases, activeDatabase, onOpenDatabase, onSelectDatabase, onDeleteDatabase, 
	onCreateDatabase, isRefreshing, onRefresh, showSettings, setShowSettings, onSignOut }) {
	
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
					className={`${showDropdown ? "show-dropdown" : ""} ${activeDatabase ? "active-db" : ""}`}
					onClick={() => setShowDropdown((prev) => !prev)}
				>

					{activeDatabase
						? <div 
							id="active-db-signal"
							className={activeDatabase?.status === "open" ? "open" : ""}
						/>
						: <Database style={{ width: "1rem", height: "1rem", color: "#C4CAD4" }}/>
					}
					<span 
						id="active-db-name"
						className={activeDatabase ? "active" : ""}
					>
						{activeDatabase?.name ?? "Select database"}
					</span>

					<ChevronDown id="db-dropdown-chevron" className={showDropdown ? "show-dropdown" : ""}/>
				</button>
				
				{showDropdown && (
					<DatabaseDropdown
						databases={databases} 
						activeDatabaseId={activeDatabase?.id}
						onOpenDatabase={onOpenDatabase}
						onSelectDatabase={onSelectDatabase}
						onDeleteDatabase={onDeleteDatabase}
						onCreateDatabase={onCreateDatabase}
						onClose={() => setShowDropdown(false)}
					/>
				)}
			</div>

			{activeDatabase && (
				<>
					<span id="database-overview">
						{activeDatabase?.numTables}{" Tables "}•{" "}{activeDatabase?.size}
					</span>
					
					<span id="database-user">
						as
						{" "}
						<span>{activeDatabase?.user ?? activeDatabase?.email ?? "Guest"}</span>
					</span>
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

				<UserMenu 
					currentUser={currentUser}
					onSignOut={onSignOut}
				/>
			</div>
		</header>
	);
}

export default Header;