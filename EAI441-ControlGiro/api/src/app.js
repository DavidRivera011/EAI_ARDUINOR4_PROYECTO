const express = require("express");
const cors = require("cors");

const systemRoutes = require("./routes/system.routes");

const app = express();

app.use(cors());
app.use(express.json());

app.get("/", (req, res) => {
  res.json({
    name: "EAI441 Control de Giro",
    status: "API funcionando",
    mode:
      String(process.env.SIMULATION_MODE).toLowerCase() === "false"
        ? "REAL"
        : "SIMULACION"
  });
});

app.use("/api/system", systemRoutes);

app.use((err, req, res, next) => {
  console.error(err.message);

  res.status(400).json({
    success: false,
    message: err.message || "Error interno."
  });
});

module.exports = app;
