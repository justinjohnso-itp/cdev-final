import express from "express";
import * as authController from "../controllers/authController.js"; // Changed import and added .js

const router = express.Router();

// Redirects user to Spotify for authorization
router.get("/login", authController.login);

// Handles the callback from Spotify after authorization
router.get("/callback", authController.callback);

// TODO: Add route for refreshing token
// router.get('/refresh_token', authController.refreshToken);

export default router; // Changed to export default
