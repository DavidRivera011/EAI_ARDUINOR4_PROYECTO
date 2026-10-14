const simulator = require("./simulator.service");

function getStatus() {
  return simulator.getStatus();
}

function setMode(mode) {
  return simulator.setMode(mode);
}

function executeCommand(command) {
  return simulator.executeCommand(command);
}

module.exports = {
  getStatus,
  setMode,
  executeCommand
};
