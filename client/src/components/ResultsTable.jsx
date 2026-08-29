import { AlertCircle, CheckCircle, Loader2, Terminal } from "lucide-react";
import "./../styles/results-table.css";



function ResultsTable({ result, isRunning }) {
	// Query is still running
	if (isRunning) {
		return (
			<div id="results-table-loading">
				<Loader2 className="results-loader" />
				<span>Executing query...</span>
			</div>
		);
	}

	// Query returned empty result set
	if (!result) {
		return (
			<div id="results-table-empty">
				<Terminal style={{ width: "1.75rem", height: "1.75rem" }}/>
				<span id="run-query-msg-1">Run a query to see results</span>
				<span id="run-query-msg-2">Ctrl+Enter to execute</span>
			</div>
		);
	}

	// Query produced an error
	if (result?.type === "error") {
		return (
			<div id="results-table-error">
				<div>
					<AlertCircle style={{width: "1rem", height: "1rem", flexShrink: "0", marginTop: "1rem", color: "#DC2626" }}/>
					<div>
						<p id="error-query-title">Query Error</p>
						<p id="error-query-msg">{result?.error}</p>
					</div>
				</div>
			</div>
		);
	}

	// Update or DDL statement result
	if (result?.type === "mutation" || result?.type === "ddl") {
		return (
			<div id="results-table-update">
				<div>
					<CheckCircle style={{width: "1rem", height: "1rem", flexShrink: "0", marginTop: "1rem", color: "#16A34A"}}/>
					<div>
						<p id="update-query-title">Query OK</p>
						<p id="update-query-msg">
							{result?.type === "ddl"
								? "Statement executed successfully"
								: `${result?.rowsAffected} row${result?.rowsAffected !== 1 ? "s" : ""} affected`
							}
							{" - "}
							{result?.executionTime}ms
						</p>
					</div>
				</div>
			</div>
		);
	}

	// Normal query row retrieval

	// Use different formats for different data types
	function formatValue(value) {
		if (value === null || value === undefined) { return "NULL"; }
		if (typeof value === "boolean") { return value ? "TRUE" : "FALSE"; }
		return String(value);
	}

	// Use different colors for different data types
	function cellColor(value) {
		if (value === null || value === undefined) { return "#D1D5DB"; }
		if (typeof value === "boolean") { return value ? "#16A34A" : "#DC2626"; }
		if (typeof value === "number") { return "#2563EB"; }
		if (typeof value === "string" && /^\d{4}-\d{2}-\d{2}/.test(value)) { return "#B45309"; }
		return "#111827";
	} 


	return (
		<div id="results-table-select">
			<table>
				{/* Table Header */}
				<thead>
					<tr id="header-row">
						<th id="header-hash">#</th>
						{result?.columns?.map((col) => (
							<th key={col} className="header-col-name">
								{col}
							</th>
						))}
						
					</tr>
				</thead>

				{/* Table Body */}
				<tbody>
					{result?.rows?.map((row, rowIndex) => (
						<tr key={rowIndex} className="table-row">

							<td className="table-row-index">{rowIndex + 1}</td>
							{result?.columns?.map((col) => {
								const val = row[col];
								const isNull = val === null || val === undefined;

								return (
									<td 
										key={col} 
										className={`table-row-data ${isNull ? "null" : ""}`}
										style={{ color: cellColor(val) }}
									>
										{formatValue(val)}
									</td>
								);
							})}
						</tr>
					))}
				</tbody>
			</table>
		</div>
	);
}

export default ResultsTable;