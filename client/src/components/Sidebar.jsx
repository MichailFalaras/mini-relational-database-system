import { useState } from "react";
import { Search, Database, ChevronDown, ChevronRight, 
		Loader2, Table2, Hash } from "lucide-react";
import "./../styles/sidebar.css";



function Sidebar({ activeConn, tables, indexes, isRefreshing, activeTable, onSelectTable }) {
	// Table-related state
	const [expandTables, setExpandTables] = useState(true);

	// Index-related state
	const [expandIndexes, setExpandIndexes] = useState(false);

	// Sidebar input state
	const [sidebarSearch, setSidebarSearch] = useState("");

	const filteredTables = tables?.filter(
		(table) => !sidebarSearch || table?.name?.toLowerCase().includes(sidebarSearch.toLowerCase())
	);
	const filteredIndexes = indexes?.filter(
		(index) => !sidebarSearch || index?.name?.toLowerCase().includes(sidebarSearch.toLowerCase())
	);
	

	return (
		<aside id="sidebar">
			{/* Search bar */}
			<div id="sidebar-search-bar">
				<div>
					<Search style={{width: "0.875rem", height: "0.875rem", flexShrink: "0", color: "#9CA3AF" }}/>
					<input 
						type="text"
						placeholder="Filter..."
						id="sidebar-search-input"
						value={sidebarSearch}
						onChange={(event) => setSidebarSearch(event.target.value)}
					/>
				</div>
			</div>

			<div id="sidebar-contents">
				{/* Empty sidebar when user is not connected */}
				{ activeConn == null
					? (
						<div id="sidebar-no-connection">
							<Database style={{ width: "1.5rem", height: "1.5rem", color: "#D1D5DB"}}/>
							<p>No database connected</p>
						</div>
					)
					: (
						<>
							{/* Tables of selected Database  */}
							<button 
								id="sidebar-tables-btn"
								onClick={() => setExpandTables((prev) => !prev)}
							>
								{expandTables 
									? <ChevronDown style={{ width: "0.75rem", height: "0.75rem" }} />
									: <ChevronRight style={{ width: "0.75rem", height: "0.75rem" }} />
								}
								Tables
								<span>{tables?.length ?? 0}</span>
							</button>

							{expandTables && (
								isRefreshing
									? (
										<div id="refreshing-schema">
											<Loader2 className="loader" />
											<span>Refreshing schema...</span>
										</div>
									)
									: !tables || tables?.length === 0 
										? (
											<div id="no-tables-available">
												No schema available for<br />
												<span>{activeConn?.name}</span>
											</div>
										)
										: (
										filteredTables?.map((table) => {
											const isActive = table.name === activeTable;

											return (
												<button 
													key={table?.name} 
													onClick={() => onSelectTable(table.name)}
													className={`table-option-btn ${isActive ? "active" : ""}`}
												>
													<Table2 className={`table-icon ${isActive ? "active" : ""}`} />
													<span className="table-option-name">{table?.name}</span>
													<span className="table-option-rowcount">{table?.rowCount}</span>
												</button>
											);
										})
									)
							)}

							{/* Indexes of selected Database */}
							<button
								id="sidebar-indexes-btn"
								onClick={() => setExpandIndexes((prev) => !prev)}
							>
								{expandIndexes 
									? <ChevronDown style={{ width: "0.75rem", height: "0.75rem" }} />
									: <ChevronRight style={{ width: "0.75rem", height: "0.75rem" }} />
								}
								Indexes
								<span>{indexes?.length ?? 0}</span>
							</button>

							{expandIndexes && filteredIndexes?.map((index) => (
								<div 
									key={index?.name}
									className="index-option"
								>
									<Hash style={{ width: "0.875rem", height: "0.875rem", flexShrink: "0", color: "#C4CAD4" }} />
									<span className="index-option-name">{index?.name}</span>
									{index?.unique && (
										<span className="index-option-unique">U</span>
									)}
								</div>	
							))}
						</>
					)
				}			
			</div>
		</aside>
	);
}

export default Sidebar;