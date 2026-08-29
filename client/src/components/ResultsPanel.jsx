import { useState } from "react";
import ResultsTable from "./ResultsTable.jsx";
import SchemaView from "./SchemaView.jsx";
import IndexView from "./IndexView.jsx";
import HistoryView from "./HistoryView.jsx";
import { Clock } from "lucide-react";
import "./../styles/results-panel.css"

const RESULTS_TABS = ["results", "schema", "indexes", "history"];

function ResultsPanel({ result, isRunning, tables, indexes, activeTable, updateSQL, history, setHistory }) {
	const [resultPanel, setResultPanel] = useState(RESULTS_TABS[0]);

	return (
		<div id="results-panel">
			{/* Header */}
			<div id="results-header">
				{RESULTS_TABS.map((tab) => {
					const isActive = resultPanel === tab;

					return (
						<button 
							key={tab}
							className={`results-tab-btn ${isActive ? "active" : ""}`}
							onClick={() => setResultPanel(tab)}
						>
							{isActive && <div id="active-tab-styles"/>}
							{tab}

							{tab === "results" && result?.type === "select" && (
								<span className="row-count">
									{result?.rows?.length}
								</span>
							)}

							{tab === "history" && history?.length > 0 && (
								<span className="row-count">
									{history?.length > 99 ? "99+" : history?.length}
								</span>
							)}
						</button>
					);
				})}

				<div id="execution-time">
					{result && result.type !== "error" && (
						<div>
							<Clock style={{ width: "0.875rem", height: "0.875rem" }}/>
							<span>{result?.executionTime ?? 5}ms</span>
						</div>
					) }
				</div>
			</div>

			{/* Results Area */}
			<div id="results-area">
				{resultPanel === "results" && <ResultsTable result={result} isRunning={isRunning} />}
				{resultPanel === "schema" && <SchemaView tableName={activeTable} tables={tables} /> }
				{resultPanel === "indexes" && <IndexView tableName={activeTable} indexes={indexes} /> }
				{resultPanel === "history" &&
					<HistoryView 
						history={history} 
						onRestore={(sql) => { 
							updateSQL(sql); 
							setResultPanel("results"); 
							}
						} 
						onClear={() => setHistory([])}
					/>
				}
			</div>
		</div>
	);
}

export default ResultsPanel;