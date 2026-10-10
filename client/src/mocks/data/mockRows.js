export const MOCK_ROWS = {
	users: {
		columns: ["id", "email", "username", "full_name", "created_at", "is_active"],
		rows: [
			{ id: 1, email: "sarah.chen@example.com", username: "schen", full_name: "Sarah Chen", created_at: "2023-01-15 09:23:11", is_active: true },
			{ id: 2, email: "marcus.okafor@example.com", username: "mokafor", full_name: "Marcus Okafor", created_at: "2023-01-16 14:07:32", is_active: true },
			{ id: 3, email: "priya.sharma@example.com", username: "psharma", full_name: "Priya Sharma", created_at: "2023-02-03 11:45:09", is_active: true },
			{ id: 4, email: "jake.williams@example.com", username: "jwilliams", full_name: "Jake Williams", created_at: "2023-02-08 08:12:55", is_active: false },
			{ id: 5, email: "luna.petrov@example.com", username: "lpetrov", full_name: "Luna Petrov", created_at: "2023-02-14 16:33:41", is_active: true },
			{ id: 6, email: "david.nakamura@example.com", username: "dnakamura", full_name: "David Nakamura", created_at: "2023-03-01 10:19:28", is_active: true },
			{ id: 7, email: "amara.diallo@example.com", username: "adiallo", full_name: "Amara Diallo", created_at: "2023-03-12 09:54:17", is_active: true },
			{ id: 8, email: "felix.mendez@example.com", username: "fmendez", full_name: "Felix Mendez", created_at: "2023-03-19 13:28:44", is_active: false },
		]
	},

	orders: {
		columns: ["id", "user_id", "status", "total_amount", "created_at", "shipped_at"],
		rows: [
			{ id: 1001, user_id: 3, status: "delivered", total_amount: 149.99, created_at: "2024-01-03 10:12:00", shipped_at: "2024-01-04 08:30:00" },
			{ id: 1002, user_id: 1, status: "processing", total_amount: 89.50, created_at: "2024-01-05 14:22:10", shipped_at: null },
			{ id: 1003, user_id: 7, status: "shipped", total_amount: 312.00, created_at: "2024-01-06 09:45:33", shipped_at: "2024-01-07 11:20:00" },
			{ id: 1004, user_id: 2, status: "cancelled", total_amount: 55.00, created_at: "2024-01-06 16:10:55", shipped_at: null },
			{ id: 1005, user_id: 5, status: "delivered", total_amount: 234.75, created_at: "2024-01-08 08:30:22", shipped_at: "2024-01-09 13:15:40" },
			{ id: 1006, user_id: 6, status: "processing", total_amount: 78.99, created_at: "2024-01-09 11:05:17", shipped_at: null },
			{ id: 1007, user_id: 3, status: "delivered", total_amount: 420.00, created_at: "2024-01-10 15:33:08", shipped_at: "2024-01-11 10:45:00" },
			{ id: 1008, user_id: 4, status: "shipped", total_amount: 67.25, created_at: "2024-01-11 09:22:44", shipped_at: "2024-01-12 09:00:00" },
		]
	},

	products: {
		columns: ["id", "sku", "name", "category_id", "price", "stock_quantity"],
		rows: [
			{ id: 1, sku: "ELEC-0042", name: "Wireless Noise-Cancelling Headphones", category_id: 3, price: 89.99, stock_quantity: 142 },
			{ id: 2, sku: "ELEC-0117", name: "USB-C Charging Hub 7-Port", category_id: 3, price: 34.99, stock_quantity: 88 },
			{ id: 3, sku: "BOOK-0019", name: "Database Internals: A Deep Dive", category_id: 7, price: 44.99, stock_quantity: 31 },
			{ id: 4, sku: "HOME-0204", name: "Bamboo Desk Organizer Set", category_id: 5, price: 22.50, stock_quantity: 210 },
			{ id: 5, sku: "ELEC-0098", name: "Mechanical Keyboard TKL RGB", category_id: 3, price: 129.00, stock_quantity: 55 },
			{ id: 6, sku: "CLTH-0077", name: "Merino Wool Quarter-Zip Pullover", category_id: 2, price: 68.00, stock_quantity: 178 },
			{ id: 7, sku: "HOME-0312", name: "Cast Iron Skillet 10-inch", category_id: 5, price: 39.95, stock_quantity: 94 },
			{ id: 8, sku: "ELEC-0223", name: "Portable SSD 1TB USB-C", category_id: 3, price: 109.99, stock_quantity: 67 },
		]
	},
	
	order_items: {
		columns: ["id", "order_id", "product_id", "quantity", "unit_price"],
		rows: [
			{ id: 1, order_id: 1001, product_id: 1, quantity: 1, unit_price: 89.99 },
			{ id: 2, order_id: 1001, product_id: 4, quantity: 2, unit_price: 22.50 },
			{ id: 3, order_id: 1002, product_id: 5, quantity: 1, unit_price: 89.50 },
			{ id: 4, order_id: 1003, product_id: 5, quantity: 1, unit_price: 129.00 },
			{ id: 5, order_id: 1003, product_id: 8, quantity: 1, unit_price: 109.99 },
			{ id: 6, order_id: 1003, product_id: 6, quantity: 1, unit_price: 68.00 },
			{ id: 7, order_id: 1005, product_id: 2, quantity: 2, unit_price: 34.99 },
			{ id: 8, order_id: 1005, product_id: 1, quantity: 1, unit_price: 89.99 },
		]
	},
	
	categories: {
		columns: ["id", "name", "parent_id", "slug"],
		rows: [
			{ id: 1, name: "All Products", parent_id: null, slug: "all-products" },
			{ id: 2, name: "Clothing", parent_id: 1, slug: "clothing" },
			{ id: 3, name: "Electronics", parent_id: 1, slug: "electronics" },
			{ id: 4, name: "Audio", parent_id: 3, slug: "audio" },
			{ id: 5, name: "Home & Kitchen", parent_id: 1, slug: "home-kitchen" },
			{ id: 6, name: "Sports", parent_id: 1, slug: "sports" },
			{ id: 7, name: "Books", parent_id: 1, slug: "books" },
			{ id: 8, name: "Headphones", parent_id: 4, slug: "headphones" },
		]
	},
	
	reviews: {
		columns: ["id", "product_id", "user_id", "rating", "body", "created_at"],
		rows: [
			{ id: 1, product_id: 1, user_id: 2, rating: 5, body: "Incredible sound quality, very comfortable for long sessions.", created_at: "2024-01-15 10:23:00" },
			{ id: 2, product_id: 5, user_id: 3, rating: 4, body: "Great keyboard, solid build and the RGB is a nice touch.", created_at: "2024-01-16 14:55:00" },
			{ id: 3, product_id: 3, user_id: 1, rating: 5, body: "Essential reading for any developer working with databases.", created_at: "2024-01-17 09:10:00" },
			{ id: 4, product_id: 8, user_id: 7, rating: 4, body: "Fast transfer speeds and very compact. Good value.", created_at: "2024-01-18 16:30:00" },
			{ id: 5, product_id: 2, user_id: 5, rating: 3, body: "Works fine but build quality could be better.", created_at: "2024-01-20 11:45:00" },
			{ id: 6, product_id: 1, user_id: 6, rating: 5, body: null, created_at: "2024-01-22 08:15:00" },
			{ id: 7, product_id: 7, user_id: 4, rating: 4, body: "Excellent skillet, heats very evenly.", created_at: "2024-01-23 13:00:00" },
			{ id: 8, product_id: 6, user_id: 8, rating: 3, body: "Nice material quality but runs slightly small.", created_at: "2024-01-24 10:20:00" },
		]
	}
};