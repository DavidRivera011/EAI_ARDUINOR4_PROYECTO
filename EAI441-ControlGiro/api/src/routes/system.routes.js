const express = require("express");
const router = express.Router();

const systemService = require("../services/system.service");

function requireDeviceKey(req, res, next) {
  const expectedKey = process.env.DEVICE_API_KEY;
  const providedKey = req.get("X-Device-Key");

  if (!expectedKey) {
    return res.status(503).json({
      success: false,
      message: "La API del dispositivo no está configurada."
    });
  }

  if (providedKey !== expectedKey) {
    return res.status(401).json({
      success: false,
      message: "Clave del dispositivo incorrecta."
    });
  }

  next();
}

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

// El Arduino consulta el modo y el comando actual.
router.get("/device/next", requireDeviceKey, (req, res) => {
  res.json(systemService.getDeviceConfig());
});

// El Arduino publica sus lecturas reales.
router.post("/telemetry", requireDeviceKey, (req, res, next) => {
  try {
    res.json(systemService.saveTelemetry(req.body));
  } catch (error) {
    next(error);
  }
});

module.exports = router;
