const express = require("express");
const authController = require("../controllers/authController");

const router = express.Router();

// Redirects user to Spotify for authorization
router.get("/login", authController.login);

// Handles the callback from Spotify after authorization
router.get("/callback", authController.callback);

// TODO: Add route for refreshing token
// router.get('/refresh_token', authController.refreshToken);

module.exports = router;
