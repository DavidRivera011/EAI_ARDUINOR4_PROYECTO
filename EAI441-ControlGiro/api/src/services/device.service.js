const VALID_MODES = ["AUTOMATICO", "MANUAL"];
const VALID_STATES = ["DERECHA", "IZQUIERDA", "DETENIDO"];

let mode = "AUTOMATICO";
let command = "DETENIDO";
let telemetry = null;
let revision = 0;

function setMode(newMode) {
  if (!VALID_MODES.includes(newMode)) {
    throw new Error("Modo no válido");
  }

  mode = newMode;
  command = "DETENIDO";
  revision++;

  return getDeviceConfig();
}

function setCommand(newCommand) {
  if (mode !== "MANUAL") {
    throw new Error("Cambia al modo MANUAL antes de enviar comandos.");
  }

  if (!VALID_STATES.includes(newCommand)) {
    throw new Error("Comando no válido");
  }

  command = newCommand;
  revision++;

  return getDeviceConfig();
}

function getDeviceConfig() {
  return {
    success: true,
    mode,
    command,
    revision
  };
}

function saveTelemetry(data) {
  const distanceCm = Number(data.distanceCm);

  if (!Number.isFinite(distanceCm) || distanceCm < 0 || distanceCm > 500) {
    throw new Error("La distancia debe estar entre 0 y 500 cm.");
  }

  if (!VALID_STATES.includes(data.motorState)) {
    throw new Error("Estado de motor no válido.");
  }

  telemetry = {
    distanceCm,
    motorState: data.motorState,
    timestamp: new Date().toISOString()
  };

  return {
    success: true,
    message: "Telemetría recibida",
    timestamp: telemetry.timestamp
  };
}

function getTelemetry() {
  return telemetry;
}

function getMode() {
  return mode;
}

module.exports = {
  setMode,
  setCommand,
  getDeviceConfig,
  saveTelemetry,
  getTelemetry,
  getMode
};
