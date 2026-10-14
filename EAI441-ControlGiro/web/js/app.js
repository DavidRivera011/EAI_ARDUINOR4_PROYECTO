
const API_URL = "https://eai441-control-giro-api.onrender.com/api/system";

const elements = {
  distance: document.getElementById("distanceValue"),
  distanceBar: document.getElementById("distanceBar"),
  distanceProgress: document.getElementById("distanceProgress"),
  motorState: document.getElementById("motorState"),
  motorIcon: document.getElementById("motorIcon"),
  motorDescription: document.getElementById("motorDescription"),
  modeValue: document.getElementById("modeValue"),
  connectionType: document.getElementById("connectionType"),
  lastUpdate: document.getElementById("lastUpdate"),
  alert: document.getElementById("connectionAlert"),
  help: document.getElementById("modeHelp"),
  message: document.getElementById("actionMessage"),
  automatic: document.getElementById("automaticMode"),
  manual: document.getElementById("manualMode"),
  left: document.getElementById("leftButton"),
  stop: document.getElementById("stopButton"),
  right: document.getElementById("rightButton")
};

let currentMode = "AUTOMATICO";
let connected = false;
let requestInProgress = false;

async function apiRequest(path, options = {}) {
  const response = await fetch(`${API_URL}${path}`, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...options.headers
    }
  });

  const data = await response.json();

  if (!response.ok) {
    throw new Error(data.message || "Error en la API");
  }

  return data;
}

function setMessage(message, isError = false) {
  elements.message.textContent = message;
  elements.message.classList.toggle("error", isError);
}

function updateControls() {
  const manualMode = currentMode === "MANUAL";

  elements.automatic.classList.toggle("active", !manualMode);
  elements.manual.classList.toggle("active", manualMode);

  elements.left.disabled = !connected || !manualMode;
  elements.stop.disabled = !connected || !manualMode;
  elements.right.disabled = !connected || !manualMode;

  elements.help.textContent = manualMode
    ? "Modo manual: utiliza los botones para controlar el motor. El sensor continúa midiendo."
    : "Modo automático: el sensor determina el giro según la distancia detectada.";
}

function renderStatus(data) {
  currentMode = data.mode;

  if (typeof data.distanceCm === "number") {
    elements.distance.textContent = data.distanceCm.toFixed(1);

    const percentage = Math.min(
      100,
      Math.max(0, (data.distanceCm / 60) * 100)
    );

    elements.distanceBar.style.width = `${percentage}%`;
    elements.distanceProgress.setAttribute(
      "aria-valuenow",
      String(data.distanceCm)
    );
  }

  elements.motorState.textContent = data.motorState;
  elements.modeValue.textContent =
    data.mode === "MANUAL" ? "MANUAL" : "AUTOMÁTICO";

  elements.connectionType.textContent =
    data.simulated ? "SIMULACIÓN" : "ARDUINO";

  const isStopped = data.motorState === "DETENIDO";

  elements.motorIcon.classList.toggle("stopped", isStopped);
  elements.motorIcon.classList.toggle("running", !isStopped);

  elements.motorIcon.textContent = isStopped
    ? "■"
    : data.motorState === "DERECHA" ? "↻" : "↶";

  elements.motorDescription.textContent = isStopped
    ? "Motor detenido"
    : `Girando hacia la ${data.motorState.toLowerCase()}`;

  elements.lastUpdate.textContent =
    new Date(data.timestamp || Date.now()).toLocaleTimeString("es-SV");

  connected = true;
  elements.alert.hidden = true;

  updateControls();
}

async function refreshStatus() {
  if (requestInProgress) return;

  requestInProgress = true;

  try {
    const data = await apiRequest("/status");
    renderStatus(data);
  } catch (error) {
    connected = false;
    elements.alert.hidden = false;
    elements.alert.textContent =
      "No se puede conectar con la API. Comprueba que el servidor esté iniciado.";
    updateControls();
  } finally {
    requestInProgress = false;
  }
}

async function changeMode(mode) {
  try {
    const data = await apiRequest("/mode", {
      method: "PUT",
      body: JSON.stringify({ mode })
    });

    renderStatus(data);
    setMessage(`Modo cambiado a ${mode === "MANUAL" ? "manual" : "automático"}.`);
  } catch (error) {
    setMessage(error.message, true);
    await refreshStatus();
  }
}

async function sendCommand(command) {
  try {
    const data = await apiRequest("/command", {
      method: "POST",
      body: JSON.stringify({ command })
    });

    renderStatus(data);
    setMessage(`Comando ejecutado: ${command}.`);
  } catch (error) {
    setMessage(error.message, true);
    await refreshStatus();
  }
}

elements.automatic.addEventListener("click", () => {
  changeMode("AUTOMATICO");
});

elements.manual.addEventListener("click", () => {
  changeMode("MANUAL");
});

elements.left.addEventListener("click", () => {
  sendCommand("IZQUIERDA");
});

elements.stop.addEventListener("click", () => {
  sendCommand("DETENER");
});

elements.right.addEventListener("click", () => {
  sendCommand("DERECHA");
});

updateControls();
refreshStatus();

// Actualiza la interfaz cada 1.5 segundos.
setInterval(refreshStatus, 1500);
