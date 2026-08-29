import { useState, useEffect } from "react";
import { Check, Copy, Clock, Trash2 } from "lucide-react";
import "./../styles/history-view.css";


// Processing Helpers

// Turn a date into the "HH:MM:SS" format
function formatDate(date) {
	const pad = (n) => String(n).padStart(2, "0");
	return `${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`;
}

// Calculates the time since the query's execution
function timeSinceExecution(date) {
	const sec = Math.floor((Date.now() - date.getTime()) / 1000);
	if (sec < 5) {
		return "just now";
	}
	if (sec < 60) {
		return `${sec}s ago`;
	}
	if (sec < 3600) {
		return `${Math.floor(sec / 60)}m ago`;
	}
	return `${Math.floor(sec / 3600)}h ago`;
}

// Preview SQL query string
function sqlPreview(query) {
  const line = query.replace(/\s+/g, " ").trim();
  return line.length > 72 ? line.slice(0, 72) + "…" : line;
}


function HistoryView({ history, onRestore, onClear }) {
	const [copiedId, setCopiedId] = useState(false);
	const [ , setTimeTicker] = useState(0);


	// Re-render search history every 15sec to update
	// the elapsed time since the query's execution
	useEffect(() => {
		const timer = setInterval(() => {
			setTimeTicker((prev) => prev + 1);
		}, 15000);

		return () => clearInterval(timer);
	}, []);


	// Copy SQL from search history entry
	function copyEntry(entry) {
		navigator.clipboard.writeText(entry.sql);
		setCopiedId(entry.id);
		
		setTimeout(() => setCopiedId(null), 1500);
	};



	// No history entries
	if (history?.length === 0) {
		return (
			<div id="no-history-entries">
				<Clock style={{width: "1.75rem", height: "1.75rem"}} />
				<span id="no-history-msg1">No queries run yet</span>
				<span id="no-history-msg2">Each execution is logged here</span>
			</div>
		);
	}

	return ( 
		<div id="history-view">
			{/* Toolbar */}
			<div id="history-view-toolbar">
				<span>
					{history?.length} entr{history?.length !== 1 ? "ies" : "y"} this session
				</span>
				<button onClick={onClear}>
					<Trash2 style={{width: "0.75rem", height: "0.75rem"}} /> Clear
				</button>
			</div>

			{/* Entries */}
			<div id="history-view-entries">
				{history?.map((entry) => (

					<div key={entry.id} className="history-entry">
						{/* Top row: time, database, badge, actions */}
						<div className="history-entry-top-row">
							<span className="history-entry-time" title={entry.executedAt.toLocaleString()}>
								{formatDate(entry.executedAt)}
							</span>
							<span style={{ fontSize: "10px", color: "#C4CAD4"}} >•</span>

							<span className="history-entry-database">
								{entry.database}
							</span>
							<span style={{ fontSize: "10px", color: "#C4CAD4"}} >•</span>

							<span className="history-entry-time-since">
								{timeSinceExecution(entry.executedAt)}
							</span>

							{entry.result.type === "select"
							? (
								<span className="history-entry-status select">
									{entry.result.rows.length} row{entry.result.rows.length !== 1 ? "s" : ""}
								</span>
							) : entry.result.type === "mutation" ? (
								<span className="history-entry-status update">
									{entry.result.rowsAffected} affected
								</span>
							) : entry.result.type === "ddl" ? (
								<span className="history-entry-status ddl">
									DDL OK
								</span>
							) : (
								<span className="history-entry-status error">
									Error
								</span>
							)}

							<span className="history-entry-exec-time">{entry.result.executionTime}ms</span>

							{/* Action buttons that appear while hovering */}
							<div className="history-entry-actions">
								<button 
									className={`history-entry-copy ${copiedId === entry.id ? "copied" : ""}`}
									title="Copy SQL"
									onClick={() => copyEntry(entry)}
								>
									{copiedId === entry.id
										? <Check style={{ width: "0.75rem", height: "0.75rem" }} />
										: <Copy style={{ width: "0.75rem", height: "0.75rem" }} />
									}
								</button>
								<button 
									className="history-entry-restore"
									title="Restore to editor"
									onClick={() => onRestore(entry.sql)}
								>
									Restore ↩
								</button>
							</div>
						</div>

						{/* SQL Query Preview */}
						<p className={`history-sql-preview ${entry.result.type === "error" ? "error" : ""}`}>
							{sqlPreview(entry.sql)}
						</p>

						{entry?.result?.type === "error" && (
							<p className="history-sql-error">{entry?.result?.error}</p>
						)}
					</div>
				))}
			</div>
		</div> 
	);
}

export default HistoryView;