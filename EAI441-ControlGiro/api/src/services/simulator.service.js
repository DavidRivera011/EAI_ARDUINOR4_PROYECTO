let distance = 20;
let motorState = "DETENIDO";
let mode = "AUTOMATICO";
let lastCommand = "DETENER";

function getDistance() {
  // Simula pequeñas variaciones de un sensor real.
  distance = Math.max(
    5,
    Math.min(60, distance + (Math.random() * 4 - 2))
  );

  return Number(distance.toFixed(1));
}

function getAutomaticMotorState(cm) {
  if (cm >= 10 && cm <= 25) {
    return "DERECHA";
  }

  if (cm >= 26 && cm <= 40) {
    return "IZQUIERDA";
  }

  return "DETENIDO";
}

function getStatus() {
  const currentDistance = getDistance();

  if (mode === "AUTOMATICO") {
    motorState = getAutomaticMotorState(currentDistance);
    lastCommand = motorState;
  }

  return {
    success: true,
    mode,
    distanceCm: currentDistance,
    motorState,
    lastCommand,
    simulated: true,
    timestamp: new Date().toISOString()
  };
}

function setMode(newMode) {
  if (!["AUTOMATICO", "MANUAL"].includes(newMode)) {
    throw new Error("Modo no válido");
  }

  mode = newMode;

  if (mode === "AUTOMATICO") {
    motorState = getAutomaticMotorState(distance);
    lastCommand = motorState;
  } else {
    motorState = "DETENIDO";
    lastCommand = "DETENER";
  }

  return getStatus();
}

function executeCommand(command) {
  if (mode !== "MANUAL") {
    throw new Error(
      "Cambia al modo MANUAL antes de enviar comandos."
    );
  }

  const commands = {
    IZQUIERDA: "IZQUIERDA",
    DERECHA: "DERECHA",
    DETENER: "DETENIDO"
  };

  if (!commands[command]) {
    throw new Error("Comando no válido");
  }

  motorState = commands[command];
  lastCommand = command;

  return getStatus();
}

module.exports = {
  getStatus,
  setMode,
  executeCommand
};
