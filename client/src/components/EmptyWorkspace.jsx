import { Database, Plus, Terminal,  } from "lucide-react";
import "./../styles/empty-workspace.css";

const SUPPORTED_OPS = [
  ["SELECT", "FROM", "WHERE", "JOIN", "GROUP BY", "ORDER BY", "LIMIT"],
  ["INSERT INTO", "UPDATE … SET", "DELETE FROM"],
  ["CREATE TABLE", "DROP TABLE", "ALTER TABLE"],
  ["CREATE INDEX", "DROP INDEX"],
];

function EmptyWorkspace({ onAddConnection, onTryDemo }) {
	
	return (
		<div id="empty-workspace">
			<div>
				{/* Icon */}
				<div id="empty-workspace-icon">
					<Database style={{ width: "1.75rem", height: "1.75rem", color: "#FFFFFF"}}/>
				</div>

				<h2>No database connected</h2>
				<p>Connect to a database to start writing queries, browsing schemas, and exploring your data.</p>

				{/* Action Buttons */}
				<div id="empty-workspace-btns">
					<button 
						id="add-connection-btn"
						onClick={onAddConnection}
					>
						<Plus style={{ width: "1rem", height: "1rem" }}/> 
						Add connection
					</button>
					<button 
						id="try-sample-data"
						onClick={onTryDemo}
					>
						<Terminal style={{ width: "1rem", height: "1rem", color: "#9CA3AF" }}/>
						Try with sample data
					</button>
				</div>

				{/* Supported Operations */}
				<div id="supported-operations">
					<p>Supported Operations</p>
					
					<div className="group-ops-container">
						{SUPPORTED_OPS?.map((group, groupIndex) => (
							<div key={groupIndex} className="group-ops">
								{group?.map((op) => (
									<span key={op} className="op">{op}</span>
								))}
							</div>
						))}
					</div>
				</div>
			</div>
		</div>
	);
}

export default EmptyWorkspace;