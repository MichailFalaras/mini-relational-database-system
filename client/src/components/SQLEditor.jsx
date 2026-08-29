import { useRef } from "react";
import "./../styles/sql-editor.css";

// All available SQL keywords
const SQL_KEYWORDS = /\b(SELECT|FROM|WHERE|JOIN|LEFT|RIGHT|INNER|OUTER|CROSS|FULL|ON|GROUP|ORDER|BY|LIMIT|OFFSET|INSERT|INTO|VALUES|UPDATE|SET|DELETE|CREATE|TABLE|DROP|ALTER|ADD|COLUMN|INDEX|UNIQUE|PRIMARY|KEY|FOREIGN|REFERENCES|AND|OR|NOT|NULL|IS|IN|LIKE|BETWEEN|AS|DISTINCT|COUNT|SUM|AVG|MAX|MIN|HAVING|CASE|WHEN|THEN|ELSE|END|TRUE|FALSE|EXISTS|DEFAULT|INT|VARCHAR|TEXT|TIMESTAMP|BOOLEAN|DECIMAL|TINYINT|CHAR|FLOAT|DOUBLE|AUTO_INCREMENT|CONSTRAINT)\b/gi;

// Process each line
// 1) Temporarily extract string literals from query to prevent them from falsely being recongized as keywords
//    e.g., SELECT 'SELECT FROM table'
// 2) Formatting keywords
// 3) Formatting numbers in their own color
// 4) Finally, restoring string literals to their initial positions
function processTokens(line) {
	const stringLiterals = [];

	let procLine = line.replace(/'[^']*'/g, 
		(str) => { 
			stringLiterals.push(str); 
			return `\x01${stringLiterals.length - 1}\x01`;
		} 
	);

	procLine = procLine.replace(SQL_KEYWORDS, (keyword) => `<span style="color:#4338ca;font-weight:bold">${keyword.toUpperCase()}</span>`);

	procLine = procLine.replace(/\b(\d+(?:\.\d+)?)\b/g, (number) => `<span style="color:#0d9488">${number}</span>`);

	procLine = procLine.replace(/\x01(\d+)\x01/g, (_, i) => `<span style="color:#9333ea">${stringLiterals[+i]}</span>`);

	return procLine;
}

// Highlight SQL keywords in query
// 1) Separate input query text in lines. In each line:
// 2) Substiture "&", "<", ">" with corresponding HTML entities &amp; , &lt; , &gt;
// 3) Find comment-starting characters "--"
// 4) Process the line and highlight SQL keywords (processTokens)
function highlightSQL(value) {
	const lines = value.split("\n");

	return lines.map((line) => {
		const esc = line.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
		const ci = esc.indexOf("--");
		
		if (ci >= 0) {
			return processTokens(esc.slice(0, ci)) +
				   `<span style="color:#9ca3af;font-style:italic">${esc.slice(ci)}</span>`;
		}

		return processTokens(esc);
	}).join("\n");
}


function SQLEditor({ value, onChange, onRun, editorFontSize, tabWidth }) {
	const taRef = useRef(null);
	const preRef = useRef(null);

	// Sync <pre> and <textarea> contents during scrolling
	function syncScroll() {
		if (taRef.current && preRef.current) {
			preRef.current.scrollTop = taRef.current.scrollTop;
			preRef.current.scrollLeft = taRef.current.scrollLeft;
		}
	}

	// Handle Ctrl + Enter (Query Execution) and Tab key down
	function handleKeyDown(event) {
		if ((event.ctrlKey || event.metaKey) && event.key === "Enter") {
			event.preventDefault();
			onRun?.();
			return;
		}

		if (event.key === "Tab") {
			event.preventDefault();
			const textArea = taRef.current;

			// Starting & ending positions of the caret
			const start = textArea.selectionStart;
			const end = textArea.selectionEnd;

			// Generate updated SQL string
			const indent = " ".repeat(tabWidth);
			const next = value.slice(0, start) + indent + value.slice(end);

			// Update SQL string state
			onChange(next);

			// Move the caret right after the tab spaces
			requestAnimationFrame(() => { 
				textArea.selectionStart = start + tabWidth;
				textArea.selectionEnd = start + tabWidth;
			});
		}
	} 

	const lineCount = value.split("\n").length;

	return (
		<div id="sql-editor">
			
			{/* Line Numbers */}
			<div 
				id="sql-editor-line-numbers"
				style={{fontSize: editorFontSize ?? "12px"}}
			>
				{Array.from({ length: Math.max(lineCount, 1)}, (_, i) => (
					<div key={i}>{i+1}</div>
				))}
			</div>

			{/* Editor Area */}
			<div 
				id="sql-editor-area"
				style={{ fontSize: `${editorFontSize ?? 12}px` }}
			>
				<pre 
					ref={preRef}
					aria-hidden
					id="sql-editor-format"
					style={{ fontSize: `${editorFontSize ?? 12}px` }}
					dangerouslySetInnerHTML={{ __html: highlightSQL(value) + "\n"}}
				/>
				<textarea 
					ref={taRef}
					id="sql-editor-text"
					style={{ fontSize: `${editorFontSize ?? 12}px` }}
					value={value}
					onChange={(event) => onChange(event.target.value)}
					onKeyDown={handleKeyDown}
					onScroll={syncScroll}
					autoCapitalize="off"
					autoComplete="off"
					autoCorrect="off"
					spellCheck={false}
				/>
			</div>
		</div>
	);
}

export default SQLEditor;