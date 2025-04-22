require("dotenv").config();
const express = require("express");
const path = require("path");

// Import routes
const authRoutes = require("./src/routes/auth");
// const apiRoutes = require('./src/routes/api'); // Keep for later

const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Serve static files from the client build directory (adjust path as needed)
// app.use(express.static(path.join(__dirname, '../client/dist')));

// Setup Routes
app.use("/auth", authRoutes);
// app.use('/api', apiRoutes); // Keep for later

// Basic root route
app.get("/", (req, res) => {
  res.send("Spotify LED Visualizer Server is running!");
});

// Error handling middleware (basic)
app.use((err, req, res, next) => {
  console.error(err.stack);
  res.status(500).send("Something broke!");
});

app.listen(PORT, () => {
  console.log(`Server listening on port ${PORT}`);
});
