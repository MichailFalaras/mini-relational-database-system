import { useState, useEffect, useRef, useCallback } from "react";
import Header from "./../components/Header.jsx";
import Sidebar from "./../components/Sidebar.jsx";
import EmptyWorkspace from "./../components/EmptyWorkspace.jsx";
import SettingsModal from "./../components/SettingsModal.jsx";
import CreateDatabaseModal from "../components/CreateDatabaseModal.jsx";
import OpenDatabaseModal from "../components/OpenDatabaseModal.jsx";
import SQLEditor from "../components/SQLEditor.jsx";
import ResultsPanel from "../components/ResultsPanel.jsx";
import { X, Plus, Loader2, PlayIcon, Check, Copy, Download } from "lucide-react";
import { loadQueryHistory, saveQueryHistory, MAX_HISTORY_ENTRIES } from "./../utils/queryHistory.js";
import "./../styles/home.css";
import { openDatabase, createDatabase, getDatabases, deleteDatabase } from "../api/databaseService.js";
import { getSchema } from "../api/schemaService.js";
import { executeQuery } from "../api/queryService.js";
import { getCurrentUser } from "../api/authService.js";

// Status text options for the Footer section 
const STATUS_TEXT = {
    idle: "Ready",
    executing: "Executing...",
    success: "Query completed",
    error: "Query failed"
};


function HomePage() {
    // User state
    const [currentUser, setCurrentUser] = useState(null);

    // Load current user
    async function loadCurrentUser() {
        try {
            const user = await getCurrentUser();
            setCurrentUser(user);
        } catch (error) {
            console.error("Unable to load current user");
        }
    }

    // Header-related state
    const [isRefreshing, setIsRefreshing] = useState(false);

    // Refresh button handler
	async function handleRefresh() {
		if (isRefreshing) {
			return;
		}

		setIsRefreshing(true);

		try {
            await loadDatabases();
            await loadSchema(false);
        } catch(error) {
            console.error("Unable to refresh database data");
        } finally {
            setIsRefreshing(false);
        }
	}

    // Tabs-related state
    const [tabs, setTabs] = useState([{id: "1", title: "Query 1", sql: "SELECT id, name, salary FROM employees WHERE salary > 50000;"}]);
    const [activeTabId, setActiveTabId] = useState("1");

    const activeTab = tabs?.find((tab) => tab?.id === activeTabId) ?? tabs[0];

    function addTab() {
        const id = `${Date.now()}`;
        setTabs((prev) => [...prev, {id, title: `Query ${prev?.length + 1}`, sql: ""}]);
        setActiveTabId(id);
    }

    function closeTab(id, event) {
        event.stopPropagation();
        if (tabs?.length === 1) { return; }

        setTabs((prev) => {
            const next = prev.filter((tab) => tab?.id !== id);
            if (activeTabId === id) setActiveTabId(next[next.length-1].id);
            return next;
        });
    }

    // Settings-related state
    const [settings, setSettings] = useState({
        defaultLimit: 100,
        editorFontSize: 13,
        tabWidth: 2,
        queryTimeout: 30
    });
    const [showSettings, setShowSettings] = useState(false);
    
    // Database-related state
    const [databases, setDatabases] = useState([]);
    const [activeDatabaseId, setActiveDatabaseId] = useState(null);
    const [isInitialLoading, setIsInitialLoading] = useState(true);

    // Load all databases displayed in the database dropdown
    async function loadDatabases() {
        try {
            const loadedDatabases = await getDatabases();

            setDatabases(loadedDatabases);

            // Restore active database ID
            // It's useful during refresh
            setActiveDatabaseId((currentId) => {
                const stillExists = loadedDatabases.some((db) => db.id === currentId);

                if (stillExists) {
                    return currentId;
                }

                return loadedDatabases[0]?.id ?? null;
            });

            if (loadedDatabases.length === 0) {
                setIsInitialLoading(false);
            }

        } catch(error) {
            console.error("Unable to load databases", error);
            setIsInitialLoading(false);
        }
    }

    useEffect(() => {
        loadCurrentUser();
        loadDatabases();
    }, []);

    // Database schema state
    const [tables, setTables] = useState([]);
    const [indexes, setIndexes] = useState([]);
    const [isLoadingSchema, setIsLoadingSchema] = useState(false);

    // Load database schema for the currently open database
    async function loadSchema(showLoading = true) {
        if (activeDatabaseId === null) {
            setTables([]);
            setIndexes([]);
            return;
        }
        
        if (showLoading && !isInitialLoading) {
            setIsLoadingSchema(true);
        }

        try {
            const schema = await getSchema(activeDatabaseId);

            setTables(schema.tables);
            setIndexes(schema.indexes);

        } catch (error) {
            console.error("Unable to load database schema");
            setTables([]);
            setIndexes([]);
        } finally {
            if (showLoading) {
                setIsLoadingSchema(false);
            }
            
            setIsInitialLoading(false);
        }
    }

    useEffect(() => {
        loadSchema();   
    }, [activeDatabaseId]);

    const [showCreateModal, setShowCreateModal] = useState(false);
    const [openDatabaseModal, setOpenDatabaseModal] = useState(undefined);
    const [executionStatus, setExecutionStatus] = useState("idle");

    let isRunning = executionStatus === "executing";
    let statusText = STATUS_TEXT[executionStatus];

    let activeDatabase = databases.find((db) => db.id === activeDatabaseId);
    

    // Open an existing database
    async function handleOpenDatabase() {
        try {
            if (openDatabaseModal) {
                const openedDatabase = await openDatabase(openDatabaseModal.id);

                // Change status of existing database
                setDatabases((prev) => 
                    prev.map((db) => 
                        db.id === openedDatabase.id
                            ? { ...db, status: "open" }
                            : db
                    )
                );

                setActiveDatabaseId(openedDatabase.id);
            }

            setOpenDatabaseModal(undefined);

        } catch (error) {
            console.error("Unable to open database");
        }
    }

    // Remove a database
    async function handleDeleteDatabase(id) {
        try {
            await deleteDatabase(id);

            setDatabases((prev) => prev.filter((db) => db.id !== id));

            if (activeDatabaseId === id) {
                setActiveDatabaseId(null);
            }
        } catch (error) {
            console.error("Unable to delete database");
        }
    }

    async function handleCreateDatabase(form) {
        try {
            const newDatabase = await createDatabase(form);

            setDatabases((prev) => [...prev, newDatabase]);
            setActiveDatabaseId(newDatabase.id);
            setShowCreateModal(false);

        } catch(error) {
            console.error("Unable to create new database");
        }
    }

    // Try mock database
    async function handleTryDemo() {
        const demo = databases.find((db) => db.id === 1);

        if (!demo) {
            return;
        }

        try {
            setIsInitialLoading(true);

            // Load demo database
            const openedDatabase = await openDatabase(demo.id);

            setDatabases((prev) => 
                prev.map((db) =>
                    db.id === openedDatabase.id
                        ? { ...db, ...openedDatabase, status: "open" }
                        : db
                )
            );

            setActiveDatabaseId(openedDatabase.id);

        } catch(error) {
            console.error("Unable to open demo database");
            setIsInitialLoading(false);
        }
    }

    async function handleSignOut() {
        // TODO: API call

        // Redirect back to Auth Page
    }
     
    // Resize Handler-related state and event listener
    const [editorHeight, setEditorHeight] = useState(260);
    const resizing = useRef(false);
    const resizeStartY = useRef(0);
    const resizeStartH = useRef(0);

    function handleResize(event) {
        resizing.current = true;
        resizeStartY.current = event.clientY;
        resizeStartH.current = editorHeight;
        event.preventDefault();
    }

    useEffect(() => {
        const onMove = (event) => {
            if (!resizing.current) {
                return;
            }
            setEditorHeight(Math.max(80, Math.min(520, resizeStartH.current + (event.clientY - resizeStartY.current))));
        }

        const onUp = () => { resizing.current = false; }
        
        window.addEventListener("mousemove", onMove);
        window.addEventListener("mouseup", onUp);

        return () => {
            window.removeEventListener("mousemove", onMove);
            window.removeEventListener("mouseup", onUp);
        }
    }, []);

    // Editor-related state
    const [isSQLCopied, setIsSQLCopied] = useState(false);
    const [result, setResult] = useState(null);
    const [activeTable, setActiveTable] = useState(null);
    const [resultPanel, setResultPanel] = useState("results");
    
    // Restore Query history from the Local storage
    const [history, setHistory] = useState(loadQueryHistory);
	const [selectedHistoryId, setSelectedHistoryId] = useState(null);

    // Add the latest query to the query search history
    function addHistoryEntry(sql, result) {
        const newHistoryEntry = {
            id: crypto.randomUUID(),
            sql,
            database: activeDatabase?.name ?? "no database",
            executedAt: new Date(),
            result
        };

        setHistory((prev) => [newHistoryEntry, ...prev].slice(0, MAX_HISTORY_ENTRIES));
    }

    useEffect(() => {
        saveQueryHistory(history);
    }, [history]);


    // Copy SQL text to clipboard
    function handleCopySQL() {
        navigator.clipboard.writeText(activeTab.sql);

        setIsSQLCopied(true);

        setTimeout(() => {
            setIsSQLCopied(false);
        }, 1500);
    }

    // Update SQL query string
    const updateSQL = useCallback((sql) => {
        setTabs((prev) => prev.map((tab) => (tab.id === activeTabId ? {...tab, sql} : tab)));
    }, [activeTabId]);

    // Select table in the sidebar
    function selectTable(tableName) {
        setActiveTable(tableName);
        setResultPanel("schema");

        updateSQL(`SELECT *\nFROM ${tableName}\nLIMIT ${settings.defaultLimit}`);

    }

    // Execute SQL query
    async function executeSQL(sql) {
        if (executionStatus === "executing") {
            return;
        }
        
        // Prevent an empty SQL query from running
        if (!activeDatabase || !sql.trim()) {
            return;
        }

        // Start new execution
        setResultPanel("results");
        setExecutionStatus("executing");
        setResult(null);

        try {
            const queryResult = await executeQuery(activeDatabase.id, sql);


            setResult(queryResult);
            setExecutionStatus("success");
            addHistoryEntry(sql, queryResult);
        
        } catch(error) {
            const errorResult = {
                type: "error",
                error: error.message
            };

            setResult(errorResult);
            setExecutionStatus("error");

            addHistoryEntry(sql, errorResult);
        }
    }

    // Execute the active editor tab's query
    async function handleQueryRun() {
        setSelectedHistoryId(null);
        await executeSQL(activeTab.sql);
    }

    // Execute a history entry query
    async function handleHistoryRerun(entry) {
        updateSQL(entry.sql);
        await executeSQL(entry.sql);
    }

	return (
		<div id="home-page">
            <Header 
                currentUser={currentUser}
                databases={databases}
                activeDatabase={activeDatabase}

                onOpenDatabase={(db) => setOpenDatabaseModal(db)}
                onSelectDatabase={setActiveDatabaseId}
                onDeleteDatabase={handleDeleteDatabase}
                onCreateDatabase={() => setShowCreateModal(true)}

                isRefreshing={isRefreshing}
                onRefresh={handleRefresh}

                showSettings={showSettings}
                setShowSettings={() => setShowSettings(true)}

                onSignOut={handleSignOut}
            />

            {/* Central App Section */}
            <div id="body">
                <Sidebar 
                    activeDatabase={activeDatabase}
                    tables={tables}
                    indexes={indexes}
                    isLoadingSchema={isLoadingSchema}
                    isRefreshing={isRefreshing}
                    activeTable={activeTable}
                    onSelectTable={selectTable}
                />

                {isInitialLoading
                    ? (
                        <div id="workspace-loading">
                            <Loader2 className="loader" />
                            <span>Loading database...</span>
                        </div>
                    )
                    : (
                        activeDatabase == null
                            ? <EmptyWorkspace 
                                onOpenDatabase={() => setOpenDatabaseModal(null)}
                                onTryDemo={handleTryDemo}
                            />
                            : (
                            <main>
                                    {/* Tabs Bar */}
                                    <div id="top-bar">
                                        {tabs?.map((tab) => {
                                            const isActive = tab?.id === activeTabId;

                                            return (
                                                <button 
                                                    key={tab?.id}
                                                    className={`tab ${isActive ? "active" : ""}`}
                                                    onClick={() => setActiveTabId(tab?.id)}
                                                >   
                                                    {isActive && <div className="tab-active-underline"/> }
                                                    {tab?.title}
                                                    {tabs?.length > 1 && (
                                                        <span className="tab-close-btn" onClick={(event) => closeTab(tab?.id, event)}>
                                                            <X style={{ width: "0.625rem", height: "0.625rem" }}/>
                                                        </span>
                                                    )}
                                                </button>
                                            );
                                        })}

                                        <button id="add-tabs-btn" onClick={addTab}>
                                            <Plus style={{width: "0.875rem", height: "0.875rem"}} />
                                        </button>
                                    </div>

                                    {/* Toolbar */}
                                    <div id="toolbar">
                                        <button 
                                            id="run-btn" 
                                            onClick={handleQueryRun} 
                                            disabled={isRunning}
                                        >
                                            {isRunning
                                                ? <Loader2 className="loader-icon"/>
                                                : <PlayIcon style={{ width: "0.75rem", height: "0.75rem", fill: "#FFFFFF" }}/>
                                            }
                                            Run
                                            <span className="enter-icon">⌃↵</span>
                                        </button>
                                        
                                        <div id="toolbar-sep"/>
                                        <button 
                                            id="copy-sql-btn"
                                            onClick={handleCopySQL}
                                        >
                                            {isSQLCopied
                                                ? <Check style={{ width: "0.75rem", height: "0.75rem", color: "#16A34A" }}/>
                                                : <Copy style={{ width: "0.75rem", height: "0.75rem" }} />
                                            }
                                            {isSQLCopied ? <span style={{ color: "#16A34A" }}>Copied</span> : "Copy"}
                                        </button>
                                        
                                        <button 
                                            id="clear-sql-btn"
                                            onClick={() => updateSQL("")}
                                        >
                                            <X style={{ width: "0.75rem", height: "0.75rem" }} /> Clear
                                        </button>
                                        
                                        <div id="export-container">
                                            {result?.type === "select" && (
                                                <button id="export-csv-btn">
                                                    <Download style={{ width: "0.75rem", height: "0.75rem" }} />
                                                    Export CSV
                                                </button>
                                            )}
                                        </div>
                                    </div>

                                    {/* SQL Editor */}
                                    <div id="sql-editor-container" style={{height: editorHeight }}>
                                        <SQLEditor 
                                            value={activeTab.sql}
                                            onChange={updateSQL}
                                            onRun={handleQueryRun}
                                            editorFontSize={settings?.editorFontSize ?? "13px"}
                                            tabWidth={settings?.tabWidth ?? "2"}
                                        />
                                    </div>

                                    {/* Resize Handle */}
                                    <div id="resize-handle" onMouseDown={handleResize}/>
                                    
                                    <ResultsPanel 
                                        result={result}
                                        isRunning={isRunning}
                                        tables={tables}
                                        indexes={indexes}
                                        activeTable={activeTable}
                                        resultPanel={resultPanel}
                                        setResultPanel={setResultPanel}
                                        updateSQL={updateSQL}
                                        history={history}
                                        setHistory={setHistory}
                                        selectedHistoryId={selectedHistoryId}
                                        setSelectedHistoryId={setSelectedHistoryId}
                                        onHistoryRerun={handleHistoryRerun}
                                    />
                            </main>
                            )
                    )
                
                }
            </div>

            {/* Footer Status Bar */}
            <footer id="status-bar-footer">
                <div id="status-database">
                    <div className={`database-status ${activeDatabase?.status === "open" ? "open": "" }`} />
                    <span>{activeDatabase?.name ?? "no database"}</span>
                </div>

                <div style={{ width: "1px", height: "0.75rem", backgroundColor: "rgba(0,0,0,0.1)" }} />
                
                <span className={`footer-execution-status ${executionStatus}`}>{statusText}</span>
                
                <div className="status-info">
                    <span>UTF-8</span>
                    <span>SQL</span>
                    <span>Ctrl+Enter to run</span>
                </div>
            </footer>

            {/* All Modals that appear from various Actions */}
            {showSettings && (
                <SettingsModal 
                    settings={settings}
                    setSettings={setSettings}
                    onClose={() => setShowSettings(false)}
                />
            )}

            {showCreateModal && (
                <CreateDatabaseModal 
                    onCreate={handleCreateDatabase}
                    onCancel={() => setShowCreateModal(false)}
                />
            )}

            {openDatabaseModal !== undefined && (
                <OpenDatabaseModal 
                    target={openDatabaseModal}
                    onOpen={handleOpenDatabase}
                    onCancel={() => setOpenDatabaseModal(undefined)}
                />
            )}
        

		</div>
	);
}

export default HomePage;