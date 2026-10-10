import { useState, useRef } from "react";
import "./../styles/sql-editor.css";

// All available SQL keywords
const SQL_KEYWORDS = /\b(SELECT|FROM|WHERE|JOIN|LEFT|RIGHT|INNER|OUTER|CROSS|FULL|ON|GROUP|ORDER|BY|LIMIT|OFFSET|INSERT|INTO|VALUES|UPDATE|SET|DELETE|CREATE|TABLE|DROP|ALTER|ADD|COLUMN|INDEX|UNIQUE|PRIMARY|KEY|FOREIGN|REFERENCES|AND|OR|NOT|NULL|IS|IN|LIKE|BETWEEN|AS|DISTINCT|COUNT|SUM|AVG|MAX|MIN|HAVING|CASE|WHEN|THEN|ELSE|END|TRUE|FALSE|EXISTS|DEFAULT|INT|VARCHAR|TEXT|TIMESTAMP|BOOLEAN|DECIMAL|TINYINT|CHAR|FLOAT|DOUBLE|AUTO_INCREMENT|CONSTRAINT)\b/gi;

// Process each line
// 1) Temporarily extract string literals from query to prevent them from falsely being recongized as keywords
//    e.g., SELECT 'SELECT FROM table'
// 2) Formatting keywords
// 3) Formatting numbers in their own color
// 4) Restoring string literals to their initial positions
// 5) Restore matching bracket markers last
function processTokens(line) {
	const stringLiterals = [];

	let procLine = line.replace(
		/'(?:''|[^'])*'/g, 
		(str) => { 
			const index = stringLiterals.length;
			stringLiterals.push(str); 

			return `\x01__STR_${index}__\x01`;
		} 
	);

	procLine = procLine.replace(
		SQL_KEYWORDS, 
		(keyword) => `<span style="color:#4338ca;font-weight:bold">${keyword}</span>`
	);

	procLine = procLine.replace(
		/\b(\d+(?:\.\d+)?)\b/g, (number) => `<span style="color:#0d9488">${number}</span>`);

	procLine = procLine.replace(
		/\x01__STR_(\d+)__\x01/g, 
		(_, i) => `<span style="color:#9333ea">${stringLiterals[+i]}</span>`
	);

	procLine = procLine.replace(
		/\x02__BRACKET_(\d+)__\x02/g,
		(_, code) => {
			const char = String.fromCharCode(Number(code));

			return `<span class="sql-matching-bracket">${char}</span>`;
		}
	);

	return procLine;
}

// Highlight SQL keywords in query
// 1) Separate input query text in lines. 
//    For each line:
//	   2) Mark any brackets with the following placeholder string (to check for bracket matching)
//     3) Substiture "&", "<", ">" with corresponding HTML entities &amp; , &lt; , &gt;
// 	   4) Find comment-starting characters "--"
//     5) Process the line and highlight SQL keywords (processTokens)
function highlightSQL(value, bracketMatch) {
	const lines = value.split("\n");

	let globalOffset = 0;

	return lines.map((line) => {
		let markedLine = "";
		
		for (let i = 0; i < line.length; i++) {
			const globalIndex = globalOffset + i;
			const char = line[i];

			if (bracketMatch &&
				(globalIndex === bracketMatch.first || globalIndex === bracketMatch.second)) {
				markedLine += `\x02__BRACKET_${char.charCodeAt(0)}__\x02`;
			} else {
				markedLine += char;
			}
		}

		globalOffset += line.length + 1;

		const esc = markedLine
			.replace(/&/g, "&amp;")
			.replace(/</g, "&lt;")
			.replace(/>/g, "&gt;");

		const ci = esc.indexOf("--");
		
		if (ci >= 0) {
			return processTokens(esc.slice(0, ci)) +
				   `<span style="color:#9ca3af;font-style:italic">${esc.slice(ci)}</span>`;
		}

		return processTokens(esc);
	}).join("\n");
}


// Bracket-matching utilities

const BRACKET_PAIRS = {
	"(": ")",
	"[": "]",
	"{": "}"
};

const OPENING_BRACKETS = new Set(Object.keys(BRACKET_PAIRS));
const CLOSING_BRACKETS = new Set(Object.values(BRACKET_PAIRS));


// Caret is currently on an opening bracket, and we must find the corresponding closing bracket
// in a subsequent position in the query string
function findMatchingBracketForward(value, openingIndex, ignored) {
	const stack = [];

	for (let i = openingIndex; i < value.length; i++) {
		if (ignored[i]) {
			continue;
		}

		const char = value[i];

		// Any opening brackets are pushed into a stack,
		// since they represent the start of an inner pair of brackets  
		if (OPENING_BRACKETS.has(char)) {
			stack.push(char);
			continue;
		}

		// If we encounter a closing bracket, and it matches the latest opening character
		// we pop the opening character
		if (CLOSING_BRACKETS.has(char)) {
			if (stack.length === 0) {
				return -1;
			}

			const openingChar = stack[stack.length - 1];

			if (BRACKET_PAIRS[openingChar] !== char) {
				return -1;
			}

			stack.pop();

			// If the stack becomes empty, we've found the matching closing bracket and return its index
			if (stack.length === 0) {
				return i;
			}
		}
	}

	return -1;
}

// Caret is currently on a closing bracket, and we must find the corresponding opening bracket
// in a previous position in the query string
function findMatchingBracketBackward(value, closingIndex, ignored) {
	const stack = [];

	for (let i = closingIndex; i >= 0; i--) {
		if (ignored[i]) {
			continue;
		}

		const char = value[i];

		// Any closing brackets are pushed into a stack,
		// since they represent the end of an inner pair of brackets  
		if (CLOSING_BRACKETS.has(char)) {
			stack.push(char);
			continue;
		}

		// If we encounter an opening bracket, and it matches the latest closing character
		// we pop the closing character
		if (OPENING_BRACKETS.has(char)) {
			if (stack.length === 0) {
				return -1;
			}

			const closingChar = stack[stack.length - 1];

			if (BRACKET_PAIRS[char] !== closingChar) {
				return -1;
			}

			stack.pop();

			// If the stack becomes empty, we've found the matching opening bracket and return its index
			if (stack.length === 0) {
				return i;
			}
		}
	}

	return -1;
}

// Build an array of indexes that correspond to positions where a bracket shouldn't
// be a considered a matching candidate:
// - inside string literals, e.g. '(' 
// - inside comments, e.g. -- ( 
function buildIgnoredPositions(value) {
	const ignored = new Array(value.length).fill(false);

	let insideString = false;
	let insideComment = false;

	for (let i = 0; i < value.length; i++) {
		const char = value[i];

		// Already inside comment
		if (insideComment) {
			ignored[i] = true;

			if (char === "\n") { insideComment = false; }
			continue
		}

		// Already inside string
		if (insideString) {
			ignored[i] = true;

			if (char === "'") {
				// SQL escaped quote: ''
				if (value[i+1] === "'") { ignored[i+1] = true; i++; } 
				else { insideString = false; }
			}
			continue;
		}

		// Start of SQL comment
		if (char === "-" && value[i+1] === "-") {
			insideComment = true;
			ignored[i] = true;
			ignored[i+1] = true;
			i++;
			continue;
		}

		// Start of string literal
		if (char === "'") {
			insideString = true;
			ignored[i] = true;
		}
	}

	return ignored;
}

// Generic utility that decides the bracket-matching case to call 
function findMatchingBracket(value, index) {
	if (index < 0 || index >= value.length) {
		return -1;
	}

	const ignored = buildIgnoredPositions(value);

	if (ignored[index]) {
		return -1;
	}

	const char = value[index];

	if (OPENING_BRACKETS.has(char)) {
		return findMatchingBracketForward(value, index, ignored);
	}

	if (CLOSING_BRACKETS.has(char)) {
		return findMatchingBracketBackward(value, index, ignored);
	}

	return -1;
}


function SQLEditor({ value, onChange, onRun, editorFontSize, tabWidth }) {
	const taRef = useRef(null);
	const preRef = useRef(null);
	
	const [bracketMatch, setBracketMatch] = useState(null);

	// Sync <pre> and <textarea> contents during scrolling
	function syncScroll() {
		if (taRef.current && preRef.current) {
			preRef.current.scrollTop = taRef.current.scrollTop;
			preRef.current.scrollLeft = taRef.current.scrollLeft;
		}
	}

	// Handle SQL editor keyboard behavior
	function handleKeyDown(event) {
		const textArea = taRef.current;

		if ((event.ctrlKey || event.metaKey) && event.key === "Enter") {
			event.preventDefault();
			onRun?.();
			return;
		}

		if (event.key === "Tab") {
			event.preventDefault();
			
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

			return;
		}

		const start = textArea.selectionStart;
		const end = textArea.selectionEnd;

		// Use the available bracket and quote pairs to add the corresponding closing char
		const pairs = { 
			"'": "'", 
			'"': '"', 
			"(": ")", 
			"[": "]", 
			"{": "}"
		};
		
		// If the user types a closing character that is already
      	// immediately after the caret, move over it instead
		const closingChars = new Set(Object.values(pairs));

		if (start === end && closingChars.has(event.key) && value[start] === event.key) {
			event.preventDefault();

			textArea.selectionStart = start + 1;
			textArea.selectionEnd = start + 1;
			return;
		}

		// Automatically insert the corresponding closing character
		const closingChar = pairs[event.key];

		if (closingChar) {
			event.preventDefault();
			
			const textArea = taRef.current;
			const start = textArea.selectionStart;
			const end = textArea.selectionEnd;

			// contains a non-empty string if a portion of the string is selected
			// upon the user typing a bracket or quote character
			const selectedText = value.slice(start, end);

			// Add current and corresponding closing char to that selected position in the query
			const next = 
				value.slice(0, start) + 
				event.key + 
				selectedText +
				closingChar + 
				value.slice(end);

			onChange(next);

			// Keep any selected text, selected after adding the brackets/quotes
			requestAnimationFrame(() => {
				if (start !== end) {
					textArea.selectionStart = start + 1;
					textArea.selectionEnd = end + 1;
				} else {	
					textArea.selectionStart = start + 1;
					textArea.selectionEnd = start + 1;
				}
			});

			return;
		}

		// Handle the case of removing an empty pair of brackets or quotes,
		// when the caret is in between them
		if (event.key === "Backspace") {

			if (start === end && pairs[value[start-1]] === value[start]) {
				event.preventDefault();

				const next = value.slice(0, start-1) + value.slice(start+1);
				
				onChange(next);

				requestAnimationFrame(() => {
					textArea.selectionStart = start - 1;
					textArea.selectionEnd = start - 1;
				});

				return;
			}
		}

		// Enter indentation case after an opening ( or { character
		if (event.key === "Enter") {
			event.preventDefault();

			// Determining the current line's indentation
			const lineStart = value.lastIndexOf("\n", start - 1) + 1;
			const currentLine = value.slice(lineStart, start);

			const currentIndent = currentLine.match(/^\s*/)?.[0] ?? "";
			const indent = " ".repeat(tabWidth);

			const previousChar = value[start-1];
			const nextChar = value[start];

			const indentPairs = {
				"(": ")",
				"{": "}"
			};

			// Enter when caret is inside () or {}
			if (start === end && indentPairs[previousChar] === nextChar) {
				const innerIndent = currentIndent + indent;

				const insertion = "\n" + innerIndent + "\n" + currentIndent;

				const next = value.slice(0, start) + insertion + value.slice(start);

				onChange(next);

				requestAnimationFrame(() => {
					const caret = start + 1 + innerIndent.length;

					textArea.selectionStart = caret;
					textArea.selectionEnd = caret;
				});

				return;
			}

			// Normal Enter
			const shouldIndent = currentLine.trimEnd().endsWith("(");

			const nextIndent = shouldIndent ? currentIndent + indent : currentIndent;

			const insertion = "\n" + nextIndent;

			const next = value.slice(0, start) + insertion + value.slice(end);

			onChange(next);

			requestAnimationFrame(() => {
				const caret = start + insertion.length;

				textArea.selectionStart = caret;
				textArea.selectionEnd = caret;
			});

			return;
		}
	} 

	// Handle caret position change, and potential bracket-matching
	function handleSelectionChange() {
		const textArea = taRef.current;

		if (!textArea) {
			return;
		}

		const start = textArea.selectionStart;
		const end = textArea.selectionEnd;

		// Don't perform bracket matching while text is selected
		if (start !== end) {
			setBracketMatch(null);
			return;
		}

		// First, check the character immediately before the caret, e.g., (word)|
		// Then, check the character immediately after the caret, e.g., |(word)

		let bracketIndex = -1;

		const previousChar = value[start - 1];
		const nextChar = value[start];

		if (OPENING_BRACKETS.has(previousChar) || CLOSING_BRACKETS.has(previousChar)) {
			bracketIndex = start - 1;
		} else if (OPENING_BRACKETS.has(nextChar) || CLOSING_BRACKETS.has(nextChar)) {
			bracketIndex = start;
		}

		if (bracketIndex === -1) {
			setBracketMatch(null);
			return;
		}

		const matchingIndex = findMatchingBracket(value, bracketIndex);

		if (matchingIndex === -1) {
			setBracketMatch(null);
			return;
		}

		setBracketMatch({
			first: bracketIndex,
			second: matchingIndex
		});
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
					dangerouslySetInnerHTML={{ __html: highlightSQL(value, bracketMatch) + "\n"}}
				/>
				<textarea 
					ref={taRef}
					id="sql-editor-text"
					style={{ fontSize: `${editorFontSize ?? 12}px` }}
					value={value}
					onChange={(event) => onChange(event.target.value)}
					onKeyDown={handleKeyDown}
					onSelect={handleSelectionChange}
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