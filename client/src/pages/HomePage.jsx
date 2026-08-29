import { useState, useEffect, useRef, useCallback } from "react";
import Header from "./../components/Header.jsx";
import Sidebar from "./../components/Sidebar.jsx";
import EmptyWorkspace from "./../components/EmptyWorkspace.jsx";
import SettingsModal from "./../components/SettingsModal.jsx";
import CreateDatabaseModal from "../components/CreateDatabaseModal.jsx";
import ConnectDatabaseModal from "../components/ConnectDatabaseModal.jsx";
import SQLEditor from "../components/SQLEditor.jsx";
import ResultsPanel from "../components/ResultsPanel.jsx";
import { X, Plus, Loader2, PlayIcon, Check, Copy, Download } from "lucide-react";
import "./../styles/home.css";



const TEST_CONNECTIONS = [
    {id: 1, name: "e_commercedb", status: "connected", user:"apostolis", numTables: "8", size: "7.2 MB"}, 
    {id: 2, name: "analytics_db", status: "disconnected", user:"apostolis", numTables: "6", size: "9.8 MB"}, 
	{id: 3, name: "hr_system", status: "disconnected", user:"apostolis", numTables: "7", size: "14.6 MB"}, 
	{id: 4, name: "users_db", status: "disconnected", user:"apostolis", numTables: "5", size: "15.2 MB"}
];

const TABLES = [
    {
        name: "users", rowCount: 14823,
        columns: [
            { name: "id", type: "INT", nullable: false, pk: true },
            { name: "email", type: "VARCHAR(255)", nullable: false, pk: false },
            { name: "username", type: "VARCHAR(100)", nullable: false, pk: false },
            { name: "full_name", type: "VARCHAR(200)", nullable: true, pk: false },
            { name: "created_at", type: "TIMESTAMP", nullable: false, pk: false, default: "CURRENT_TIMESTAMP" },
            { name: "is_active", type: "BOOLEAN", nullable: false, pk: false, default: "TRUE" },
        ],
    },
    {
        name: "orders", rowCount: 89341,
        columns: [
            { name: "id", type: "INT", nullable: false, pk: true },
            { name: "user_id", type: "INT", nullable: false, pk: false, fk: "users.id" },
            { name: "status", type: "VARCHAR(20)", nullable: false, pk: false },
            { name: "total_amount", type: "DECIMAL(10,2)", nullable: false, pk: false },
            { name: "created_at", type: "TIMESTAMP", nullable: false, pk: false, default: "CURRENT_TIMESTAMP" },
            { name: "shipped_at", type: "TIMESTAMP", nullable: true, pk: false },
        ],
    },
    {
        name: "products", rowCount: 3204,
        columns: [
            { name: "id", type: "INT", nullable: false, pk: true },
            { name: "sku", type: "VARCHAR(50)", nullable: false, pk: false },
            { name: "name", type: "VARCHAR(300)", nullable: false, pk: false },
            { name: "category_id", type: "INT", nullable: true, pk: false, fk: "categories.id" },
            { name: "price", type: "DECIMAL(10,2)", nullable: false, pk: false },
            { name: "stock_qty", type: "INT", nullable: false, pk: false, default: "0" },
        ],
    },
    {
        name: "order_items", rowCount: 312847,
        columns: [
            { name: "id", type: "INT", nullable: false, pk: true },
            { name: "order_id", type: "INT", nullable: false, pk: false, fk: "orders.id" },
            { name: "product_id", type: "INT", nullable: false, pk: false, fk: "products.id" },
            { name: "quantity", type: "INT", nullable: false, pk: false },
            { name: "unit_price", type: "DECIMAL(10,2)", nullable: false, pk: false },
        ],
    },
    {
        name: "categories", rowCount: 42,
        columns: [
            { name: "id", type: "INT", nullable: false, pk: true },
            { name: "name", type: "VARCHAR(100)", nullable: false, pk: false },
            { name: "parent_id", type: "INT", nullable: true, pk: false, fk: "categories.id" },
            { name: "slug", type: "VARCHAR(120)", nullable: false, pk: false },
        ],
    },
    {
        name: "reviews", rowCount: 28561,
        columns: [
            { name: "id", type: "INT", nullable: false, pk: true },
            { name: "product_id", type: "INT", nullable: false, pk: false, fk: "products.id" },
            { name: "user_id", type: "INT", nullable: false, pk: false, fk: "users.id" },
            { name: "rating", type: "TINYINT", nullable: false, pk: false },
            { name: "body", type: "TEXT", nullable: true, pk: false },
            { name: "created_at", type: "TIMESTAMP", nullable: false, pk: false, default: "CURRENT_TIMESTAMP" },
        ],
    },
];

const INDEXES = [
    { name: "idx_users_email", table: "users", columns: ["email"], unique: true },
    { name: "idx_users_username", table: "users", columns: ["username"], unique: true },
    { name: "idx_orders_user_id", table: "orders", columns: ["user_id"], unique: false },
    { name: "idx_orders_status_created", table: "orders", columns: ["status", "created_at"], unique: false },
    { name: "idx_products_sku", table: "products", columns: ["sku"], unique: true },
    { name: "idx_products_category", table: "products", columns: ["category_id"], unique: false },
    { name: "idx_order_items_order", table: "order_items", columns: ["order_id"], unique: false },
    { name: "idx_order_items_product", table: "order_items", columns: ["product_id"], unique: false },
    { name: "idx_reviews_product_rating", table: "reviews", columns: ["product_id", "rating"], unique: false },
];

const TABS = [
    {id: 1, title: "Query 1"},
    {id: 2, title: "Query 2"}
];

const MOCK_ROWS = {
  users: {
    rows: [
    { id: 1, email: "sarah.chen@example.com", username: "schen", full_name: "Sarah Chen", created_at: "2023-01-15 09:23:11", is_active: true },
    { id: 2, email: "marcus.okafor@example.com", username: "mokafor", full_name: "Marcus Okafor", created_at: "2023-01-16 14:07:32", is_active: true },
    { id: 3, email: "priya.sharma@example.com", username: "psharma", full_name: "Priya Sharma", created_at: "2023-02-03 11:45:09", is_active: true },
    { id: 4, email: "jake.williams@example.com", username: "jwilliams", full_name: "Jake Williams", created_at: "2023-02-08 08:12:55", is_active: false },
    { id: 5, email: "luna.petrov@example.com", username: "lpetrov", full_name: "Luna Petrov", created_at: "2023-02-14 16:33:41", is_active: true },
    { id: 6, email: "david.nakamura@example.com", username: "dnakamura", full_name: "David Nakamura", created_at: "2023-03-01 10:19:28", is_active: true },
    { id: 7, email: "amara.diallo@example.com", username: "adiallo", full_name: "Amara Diallo", created_at: "2023-03-12 09:54:17", is_active: true },
    { id: 8, email: "felix.mendez@example.com", username: "fmendez", full_name: "Felix Mendez", created_at: "2023-03-19 13:28:44", is_active: false },
  ],
    columns: ["id", "email", "username", "full_name", "created_at"]
  },
  orders: [
    { id: 1001, user_id: 3, status: "delivered", total_amount: 149.99, created_at: "2024-01-03 10:12:00", shipped_at: "2024-01-04 08:30:00" },
    { id: 1002, user_id: 1, status: "processing", total_amount: 89.50, created_at: "2024-01-05 14:22:10", shipped_at: null },
    { id: 1003, user_id: 7, status: "shipped", total_amount: 312.00, created_at: "2024-01-06 09:45:33", shipped_at: "2024-01-07 11:20:00" },
    { id: 1004, user_id: 2, status: "cancelled", total_amount: 55.00, created_at: "2024-01-06 16:10:55", shipped_at: null },
    { id: 1005, user_id: 5, status: "delivered", total_amount: 234.75, created_at: "2024-01-08 08:30:22", shipped_at: "2024-01-09 13:15:40" },
    { id: 1006, user_id: 6, status: "processing", total_amount: 78.99, created_at: "2024-01-09 11:05:17", shipped_at: null },
    { id: 1007, user_id: 3, status: "delivered", total_amount: 420.00, created_at: "2024-01-10 15:33:08", shipped_at: "2024-01-11 10:45:00" },
    { id: 1008, user_id: 4, status: "shipped", total_amount: 67.25, created_at: "2024-01-11 09:22:44", shipped_at: "2024-01-12 09:00:00" },
  ],
  products: [
    { id: 1, sku: "ELEC-0042", name: "Wireless Noise-Cancelling Headphones", category_id: 3, price: 89.99, stock_qty: 142 },
    { id: 2, sku: "ELEC-0117", name: "USB-C Charging Hub 7-Port", category_id: 3, price: 34.99, stock_qty: 88 },
    { id: 3, sku: "BOOK-0019", name: "Database Internals: A Deep Dive", category_id: 7, price: 44.99, stock_qty: 31 },
    { id: 4, sku: "HOME-0204", name: "Bamboo Desk Organizer Set", category_id: 5, price: 22.50, stock_qty: 210 },
    { id: 5, sku: "ELEC-0098", name: "Mechanical Keyboard TKL RGB", category_id: 3, price: 129.00, stock_qty: 55 },
    { id: 6, sku: "CLTH-0077", name: "Merino Wool Quarter-Zip Pullover", category_id: 2, price: 68.00, stock_qty: 178 },
    { id: 7, sku: "HOME-0312", name: "Cast Iron Skillet 10-inch", category_id: 5, price: 39.95, stock_qty: 94 },
    { id: 8, sku: "ELEC-0223", name: "Portable SSD 1TB USB-C", category_id: 3, price: 109.99, stock_qty: 67 },
  ],
  order_items: [
    { id: 1, order_id: 1001, product_id: 1, quantity: 1, unit_price: 89.99 },
    { id: 2, order_id: 1001, product_id: 4, quantity: 2, unit_price: 22.50 },
    { id: 3, order_id: 1002, product_id: 5, quantity: 1, unit_price: 89.50 },
    { id: 4, order_id: 1003, product_id: 5, quantity: 1, unit_price: 129.00 },
    { id: 5, order_id: 1003, product_id: 8, quantity: 1, unit_price: 109.99 },
    { id: 6, order_id: 1003, product_id: 6, quantity: 1, unit_price: 68.00 },
    { id: 7, order_id: 1005, product_id: 2, quantity: 2, unit_price: 34.99 },
    { id: 8, order_id: 1005, product_id: 1, quantity: 1, unit_price: 89.99 },
  ],
  categories: [
    { id: 1, name: "All Products", parent_id: null, slug: "all-products" },
    { id: 2, name: "Clothing", parent_id: 1, slug: "clothing" },
    { id: 3, name: "Electronics", parent_id: 1, slug: "electronics" },
    { id: 4, name: "Audio", parent_id: 3, slug: "audio" },
    { id: 5, name: "Home & Kitchen", parent_id: 1, slug: "home-kitchen" },
    { id: 6, name: "Sports", parent_id: 1, slug: "sports" },
    { id: 7, name: "Books", parent_id: 1, slug: "books" },
    { id: 8, name: "Headphones", parent_id: 4, slug: "headphones" },
  ],
  reviews: [
    { id: 1, product_id: 1, user_id: 2, rating: 5, body: "Incredible sound quality, very comfortable for long sessions.", created_at: "2024-01-15 10:23:00" },
    { id: 2, product_id: 5, user_id: 3, rating: 4, body: "Great keyboard, solid build and the RGB is a nice touch.", created_at: "2024-01-16 14:55:00" },
    { id: 3, product_id: 3, user_id: 1, rating: 5, body: "Essential reading for any developer working with databases.", created_at: "2024-01-17 09:10:00" },
    { id: 4, product_id: 8, user_id: 7, rating: 4, body: "Fast transfer speeds and very compact. Good value.", created_at: "2024-01-18 16:30:00" },
    { id: 5, product_id: 2, user_id: 5, rating: 3, body: "Works fine but build quality could be better.", created_at: "2024-01-20 11:45:00" },
    { id: 6, product_id: 1, user_id: 6, rating: 5, body: null, created_at: "2024-01-22 08:15:00" },
    { id: 7, product_id: 7, user_id: 4, rating: 4, body: "Excellent skillet, heats very evenly.", created_at: "2024-01-23 13:00:00" },
    { id: 8, product_id: 6, user_id: 8, rating: 3, body: "Nice material quality but runs slightly small.", created_at: "2024-01-24 10:20:00" },
  ],
};

const HISTORY = [
    {id: "1",
        sql: `
            SELECT
                u.id,
                u.username,
                u.email,
                COUNT(o.id) AS order_count,
                SUM(o.total_amount) AS total_spent
            FROM users u
            LEFT JOIN orders o ON u.id = o.user_id
            WHERE u.is_active = TRUE
            GROUP BY u.id, u.username, u.email
            ORDER BY total_spent DESC
            LIMIT 10;
        `.trim(),

        database: "e_commerce_db", executedAt: new Date(Date.now() - 12_000),
        result: {
            type: "select",
            rows: [
                { id: 1, username: "schen", email: "sarah.chen@example.com", order_count: 12, total_spent: 1250.40 },
                { id: 3, username: "psharma", email: "priya.sharma@example.com", order_count: 9, total_spent: 987.25 },
                { id: 7, username: "adiallo", email: "amara.diallo@example.com", order_count: 8, total_spent: 864.10 },
                { id: 5, username: "lpetrov", email: "luna.petrov@example.com", order_count: 6, total_spent: 731.90 }
            ],
            columns: [
                "id",
                "username",
                "email",
                "order_count",
                "total_spent"
            ],
            executionTime: 14
        }}
];

function HomePage() {
    // Header-related state
    const [isRefreshing, setIsRefreshing] = useState(false);

    // Refresh button handler
	async function handleRefresh() {
		if (isRefreshing) {
			return;
		}

		setIsRefreshing(true);

		await new Promise(resolve => setTimeout(resolve, 700));

		setIsRefreshing(false);
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
    
    // Connection-related state
    const [connections, setConnections] = useState(TEST_CONNECTIONS ?? []);
    const [activeConnId, setActiveConnId] = useState(1);
    const [showCreateModal, setShowCreateModal] = useState(false);
    const [connModal, setConnModal] = useState(undefined);
    const [isRunning, setIsRunning] = useState(false);

    let activeConn = connections.find((conn) => conn.id === activeConnId);
    let statusText = isRunning
        ? "Executing..."
        : "Ready";

    
    // Connect to an existing database or create a new connection
    async function handleConnectDatabase(data) {
        // TODO: API call

        if (connModal) {
            // Change status of existing connection (from the connection dropdown)
            setConnections((prev) => 
                prev.map((conn) => 
                    conn.id === connModal.id
                        ? {
                            ...conn,
                            user: data.username,
                            status: "connected"
                        }
                        : conn
                )
            );

            setActiveConnId(connModal.id);
        }
        else {
            // New (empty database) connection
            const id = Date.now();
            const newConnection = { 
                id,
                name: data.databaseName, 
                user: data.username,
                status: "disconnected",
                numTables: "0",
                size: "0 MB"
            };

            setConnections((prev) => [...prev, newConnection]);
            setActiveConnId(id);
        }

        setConnModal(undefined);
    }

    // Delete a database connection
    async function handleDeleteConnection(id) {
        // TODO: API call

        setConnections((prev) => prev.filter((conn) => conn.id !== id));

        if (activeConnId === id) {
            setActiveConnId(null);
        }
    }

    async function handleCreateDatabase(form) {
        // TODO: API call

        const id = Date.now();
        const newConnection = {
            id,
            name: form.databaseName,
            user: form.username,
            status: "connected",
            numTables: "0",
            size: "0 MB"
        };

        setConnections((prev) => [...prev, newConnection]);
        setActiveConnId(id);
        setShowCreateModal(false);
    }

    // Try mock database
    function handleTryDemo() {
        const demo = connections.find((conn) => conn.id === 1);
        if (!demo) {
            return;
        }

        setConnections((prev) => 
            prev.map((conn) =>
                conn.id === demo.id
                    ? {
                        ...conn,
                        status: "connected",
                        user: "guest"
                    }
                    : conn
            )
        );
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
    const [result, setResult] = useState({ type: "select", rows: MOCK_ROWS.users.rows, columns: MOCK_ROWS.users.columns, executionTime: 50});
    const [activeTable, setActiveTable] = useState(TABLES[1]?.name);
    const [history, setHistory] = useState(HISTORY);

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
        updateSQL(`SELECT *\nFROM ${tableName}\nLIMIT ${settings.defaultLimit}`);
    }

    // Execute SQL query
    function handleQueryRun() {
        if (!isRunning) {
            return;
        }

        setIsRunning(true);

        try {
            // API call
            // Set results state
        } finally {
            setIsRunning(false);
        }

    }


	return (
		<div id="home-page">
            <Header 
                connections={connections}
                activeConn={activeConn}

                onConnect={(conn) => setConnModal(conn)}
                onSelectConnection={setActiveConnId}
                onDeleteConnection={handleDeleteConnection}
                onNewConnection={() => setConnModal(null)}
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
                    activeConn={activeConn}
                    tables={TABLES}
                    indexes={INDEXES}
                    isRefreshing={isRefreshing}
                    activeTable={activeTable}
                    onSelectTable={selectTable}
                />

                {activeConn == null
                    ? <EmptyWorkspace 
                          onAddConnection={() => setConnModal(null)}
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
                                tables={TABLES}
                                indexes={INDEXES}
                                activeTable={activeTable}
                                updateSQL={updateSQL}
                                history={history}
                                setHistory={setHistory}
                            />
                    </main>
                    )
                }
            </div>

            {/* Footer Status Bar */}
            <footer id="status-bar-footer">
                <div id="status-connection">
                    <div className={`connection ${activeConn?.status === "connected" ? "connected": "" }`} />
                    <span>{activeConn?.name ?? "no database"}</span>
                </div>

                <div style={{ width: "1px", height: "0.75rem", backgroundColor: "rgba(0,0,0,0.1)" }} />
                
                <span style={{ color: result?.type === "error" ? "#dc2626" : "#9ca3af" }}>{statusText}</span>
                
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

            {connModal !== undefined && (
                <ConnectDatabaseModal 
                    target={connModal}
                    onConnect={handleConnectDatabase}
                    onCancel={() => setConnModal(undefined)}
                />
            )}
        

		</div>
	);
}

export default HomePage;