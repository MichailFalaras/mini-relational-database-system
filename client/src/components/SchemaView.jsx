import { Key, Table2 } from "lucide-react";
import "./../styles/schema-view.css";


const COLUMN_FEATURES = ["Column", "Type", "Nullable", "Key", "Default"];


function SchemaView({ tableName, tables }) {
	
	// No/Invalid table is selected to be viewed
	if (!tableName || !tables.some((table) => table.name === tableName)) {
		return ( 
			<div id="table-schema-empty">
				Click a table in the sidebar to inspect its schema
			</div> 
		);
	}

	// Table exists in the database
	const table = tables.find((table) => table.name === tableName);

	return (
		<div id="table-schema">
			{/* Header */}
			<div id="table-schema-header">
				<Table2 style={{width: "0.875rem", height: "0.875rem", flexShrink: "0", color: "#4F46E5"}} />
				<span className="table-name">{tableName}</span>
				<span className="table-rowcount">{table.rowCount.toLocaleString()} rows</span>
			</div>

			{/* Schema */}
			<table>
				<thead>
					<tr>
						{COLUMN_FEATURES.map((feat) => (
							<th key={feat} className="header-feat-name">
								{feat}
							</th>
						))}
					</tr>
				</thead>

				<tbody>
					{table?.columns?.map((col) => (
						<tr key={col.name} className="column-row">
							
							{/* Column name */}
							<td style={{ padding: "0.5rem 1rem" }}>
								<div className="column-key">
									{col?.pk 
										? <Key style={{width: "0.75rem", height: "0.75rem", flexShrink: "0", color: "#D97706"}} />
										: col?.fk
											? <span className="column-fk">FK</span>
											: <span className="column-no-key" />
									}
									<span className="column-name">{col?.name}</span>
								</div>
							</td>

							{/* Column Type */}
							<td className="column-type">{col?.type}</td>
							
							{/* Nullable Status */}
							<td className={`column-nullable ${col?.nullable ? "nullable" : "not-nullable"}`}>
								{col?.nullable ? "YES" : "NO"}
							</td>

							{/* Key Status */}
							<td style={{ padding: "0.5rem 1rem" }}>
								{col?.pk && (
									<span className="column-pk-status">PK</span>
								)}
								{col?.fk && (
									<span className="column-fk-status">→ {col?.fk}</span>
								)}
							</td>

							{/* Default Value */}
							<td className="column-default-value">
								{col?.default ?? <span style={{ opacity: 0.4 }}>-</span>}
							</td>
						</tr>
					))}
				</tbody>
			</table>

		</div>
	);



}

export default SchemaView;