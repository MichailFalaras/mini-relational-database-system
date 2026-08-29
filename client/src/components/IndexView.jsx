import "./../styles/index-view.css";

const INDEX_VIEW_COLUMNS = ["Index Name", "Table", "Columns", "Type"];

function IndexView({ tableName, indexes }) {

	const idxs = tableName 
					? indexes.filter((index) => index.table === tableName) 
					: indexes;

	
	return ( 
		<div id="index-view">
			<table>
				<thead>
					<tr>
						{INDEX_VIEW_COLUMNS?.map((col) => (
							<th key={col} className="index-view-columns">{col}</th>
						))}
					</tr>
				</thead>

				<tbody>
					{idxs.map((index) => (
						<tr key={index?.name} className="index-view-rows">
							<td className="index-name">{index?.name}</td>
							<td className="index-table">{index?.table}</td>
							<td className="index-columns">{index?.columns?.join(", ")}</td>
							<td className="index-is-unique">
								{index?.unique ? (
									<span className="index-unique">UNIQUE</span>
								) : (
									<span className="index-not-unique">INDEX</span>
								)}
							</td>
						</tr>
					))}
					{idxs.length === 0 && (
						<tr>
							<td colSpan={4} id="no-indexes">No indexes on this table</td>
						</tr>
					)}
				</tbody>
			</table>
		</div> 
	);
}

export default IndexView;