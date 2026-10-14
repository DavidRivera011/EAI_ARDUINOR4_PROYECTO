const express = require("express");
const router = express.Router();

const systemService = require("../services/system.service");

router.get("/status", (req, res) => {
  res.json(systemService.getStatus());
});

router.put("/mode", (req, res, next) => {
  try {
    const { mode } = req.body;

    if (!mode) {
      return res.status(400).json({
        success: false,
        message: "Debes indicar el modo."
      });
    }

    res.json(systemService.setMode(mode));
  } catch (error) {
    next(error);
  }
});

router.post("/command", (req, res, next) => {
  try {
    const { command } = req.body;

    if (!command) {
      return res.status(400).json({
        success: false,
        message: "Debes indicar un comando."
      });
    }

    res.json(systemService.executeCommand(command));
  } catch (error) {
    next(error);
  }
});

module.exports = router;
