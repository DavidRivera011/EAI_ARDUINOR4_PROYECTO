const simulator = require("./simulator.service");
const device = require("./device.service");

const simulationEnabled =
  String(process.env.SIMULATION_MODE).toLowerCase() !== "false";

function getStatus() {
  if (simulationEnabled) {
    return simulator.getStatus();
  }

  const telemetry = device.getTelemetry();

  return {
    success: true,
    mode: device.getMode(),
    distanceCm: telemetry?.distanceCm ?? null,
    motorState: telemetry?.motorState ?? "DETENIDO",
    lastCommand: device.getDeviceConfig().command,
    simulated: false,
    deviceOnline: Boolean(
      telemetry &&
      Date.now() - Date.parse(telemetry.timestamp) < 15000
    ),
    timestamp: telemetry?.timestamp ?? null
  };
}

function setMode(mode) {
  device.setMode(mode);
  simulator.setMode(mode);

  return getStatus();
}

function executeCommand(command) {
  device.setCommand(command);
  simulator.executeCommand(command);

  return getStatus();
}

function getDeviceConfig() {
  return device.getDeviceConfig();
}

function saveTelemetry(data) {
  return device.saveTelemetry(data);
}

module.exports = {
  getStatus,
  setMode,
  executeCommand,
  getDeviceConfig,
  saveTelemetry
};
