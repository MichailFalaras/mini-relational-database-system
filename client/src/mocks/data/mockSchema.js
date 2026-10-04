export const MOCK_TABLES = [
    {
        name: "users", rowCount: 14823,
        columns: [
            { name: "id", type: "INT", nullable: false, pk: true },
            { name: "email", type: "VARCHAR(255)", nullable: false, pk: false, constraints: ["UNIQUE", "CHECK (email LIKE '%@%')"] },
            { name: "username", type: "VARCHAR(100)", nullable: false, pk: false, constraints: ["UNIQUE", "CHECK (LENGTH(username) >= 3)"] },
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
            { name: "status", type: "VARCHAR(20)", nullable: false, pk: false, 
                constraints: ["CHECK (status IN ('pending', 'processing', 'shipped', 'delivered', 'cancelled'))"] },
            { name: "total_amount", type: "DECIMAL(10,2)", nullable: false, pk: false, constraints: ["CHECK (total_amount >= 0)"] },
            { name: "created_at", type: "TIMESTAMP", nullable: false, pk: false, default: "CURRENT_TIMESTAMP" },
            { name: "shipped_at", type: "TIMESTAMP", nullable: true, pk: false },
        ],
    },
    {
        name: "products", rowCount: 3204,
        columns: [
            { name: "id", type: "INT", nullable: false, pk: true },
            { name: "sku", type: "VARCHAR(50)", nullable: false, pk: false, constraints: ["UNIQUE"] },
            { name: "name", type: "VARCHAR(300)", nullable: false, pk: false },
            { name: "category_id", type: "INT", nullable: true, pk: false, fk: "categories.id" },
            { name: "price", type: "DECIMAL(10,2)", nullable: false, pk: false, constraints: ["CHECK (price >= 0)"] },
            { name: "stock_quantity", type: "INT", nullable: false, pk: false, default: "0", constraints: ["CHECK (stock_qty >= 0)"] },
        ],
    },
    {
        name: "order_items", rowCount: 312847,
        columns: [
            { name: "id", type: "INT", nullable: false, pk: true },
            { name: "order_id", type: "INT", nullable: false, pk: false, fk: "orders.id", constraints: ["UNIQUE (order_id, product_id)"] },
            { name: "product_id", type: "INT", nullable: false, pk: false, fk: "products.id", constraints: ["UNIQUE (order_id, product_id)"] },
            { name: "quantity", type: "INT", nullable: false, pk: false, constraints: ["CHECK (quantity >= 0)"] },
            { name: "unit_price", type: "DECIMAL(10,2)", nullable: false, pk: false, constraints: ["CHECK (unit_price >= 0)"] },
        ],
    },
    {
        name: "categories", rowCount: 42,
        columns: [
            { name: "id", type: "INT", nullable: false, pk: true },
            { name: "name", type: "VARCHAR(100)", nullable: false, pk: false },
            { name: "parent_id", type: "INT", nullable: true, pk: false, fk: "categories.id", constraints: ["CHECK (parent_id != id)"] },
            { name: "slug", type: "VARCHAR(120)", nullable: false, pk: false, constraints: ["UNIQUE"] },
        ],
    },
    {
        name: "reviews", rowCount: 28561,
        columns: [
            { name: "id", type: "INT", nullable: false, pk: true },
            { name: "product_id", type: "INT", nullable: false, pk: false, fk: "products.id", constraints: ["UNIQUE (product_id, user_id)"]  },
            { name: "user_id", type: "INT", nullable: false, pk: false, fk: "users.id", constraints: ["UNIQUE (product_id, user_id)"] },
            { name: "rating", type: "TINYINT", nullable: false, pk: false, constraints: ["CHECK (rating BETWEEN 1 AND 5)"] },
            { name: "body", type: "TEXT", nullable: true, pk: false },
            { name: "created_at", type: "TIMESTAMP", nullable: false, pk: false, default: "CURRENT_TIMESTAMP" },
        ],
    },
];

export const MOCK_INDEXES = [
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