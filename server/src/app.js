import express from "express";

const app = express();

// Parse incoming JSON request bodies
app.use(express.json({
	limit: "1mb"
}));

// Temporary endpoint to verify JSON parsing
app.post("/api/test", (req, res) => {
	res.status(200).json({
		message: "JSON received successfully.",
		data: req.body
	});
});

export default app;