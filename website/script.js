// uuids for nordic 
const NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // notify
const NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // write

// ble state
let device = null;
let server = null;
let txChar = null;
let rxChar = null;

// buffer for incoming messages 
let partial = "";

// Head Position Categories
const HEAD_POSITIONS = [
  "Extreme Left",
  "Medium Left",
  "Relatively Up",
  "Medium Right",
  "Extreme Right"
];

// past samples: array of {t:number, vals:number[]}
const samples = [];

// table columns and charts
const columns = [
  "time",
  "heartRate",
  "spo2",
  "temperature",
  "headPosition",
  "snoring"
];

let chartHR, chartTemp, chartSpO2, chartHead, chartSnoring;

// ui elements
const connectBtn = document.getElementById("connectBtn");
const disconnectBtn = document.getElementById("disconnectBtn");
const statusElement = document.getElementById("ble-status");
const exportBtn = document.getElementById("exportBtn");
const clearBtn = document.getElementById("clearBtn");
const metricsTable = document.getElementById("metrics-table");
const logBox = document.getElementById("log");
const themeSelect = document.getElementById("theme-select");

// Theme Definitions for Charts
const themes = {
  "theme-midnight": {
    textColor: '#94a3b8',
    gridColor: 'rgba(255, 255, 255, 0.1)',
    colors: ['#ef4444', '#f97316', '#3b82f6', '#22c55e', '#a855f7']
  },
  "theme-vitality": {
    textColor: '#64748b',
    gridColor: 'rgba(0, 0, 0, 0.05)',
    colors: ['#ef4444', '#f97316', '#0ea5e9', '#10b981', '#8b5cf6']
  },
  "theme-glass": {
    textColor: 'rgba(255, 255, 255, 0.8)',
    gridColor: 'rgba(255, 255, 255, 0.1)',
    colors: ['#ff6b6b', '#ffa94d', '#74c0fc', '#69db7c', '#da77f2']
  }
};

// helper functions
function setBLEStatus(s) {
  statusElement.textContent = s;

  // Update badge class
  statusElement.classList.remove('connected', 'disconnected');
  if (s.toLowerCase().includes('connected') && !s.toLowerCase().includes('dis')) {
    statusElement.classList.add('connected');
  } else {
    statusElement.classList.add('disconnected');
  }
}

function logLine(s) {
  logBox.textContent += s + "\n";
  logBox.scrollTop = logBox.scrollHeight;
}

function addRow(data) {
  let tbody = metricsTable.querySelector("tbody");
  if (!tbody) {
    tbody = document.createElement("tbody");
    metricsTable.appendChild(tbody);
  }

  const row = document.createElement("tr");

  columns.forEach(col => {
    const td = document.createElement("td");
    td.textContent = data[col] ?? "-";
    row.appendChild(td);
  });

  // Prepend to show newest first (optional, but good for dashboards)
  // tbody.prepend(row); 
  // Or append as before:
  tbody.appendChild(row);
}

function exportData() {
  const escapeField = val => {
    if (val === null || val === undefined) return '';
    const s = String(val);
    if (/[",\r\n]/.test(s)) return '"' + s.replace(/"/g, '""') + '"';
    return s;
  };

  const tbody = metricsTable.querySelector("tbody");
  if (!tbody) return; // No data

  const rows = Array.from(tbody.querySelectorAll('tr'));

  const lines = [];
  lines.push(columns.map(escapeField).join(','));

  for (const row of rows) {
    const cells = Array.from(row.querySelectorAll('td'));
    const values = columns.map((_, idx) => {
      const cell = cells[idx];
      const raw = cell ? cell.textContent : '';
      return escapeField(raw.trim());
    });
    lines.push(values.join(','));
  }

  const csvContent = lines.join('\r\n');
  const ts = new Date().toISOString().replace(/[:.]/g, '-');
  const outName = `metrics-table-${ts}.csv`;

  const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = outName;
  document.body.appendChild(a);
  a.click();
  setTimeout(() => {
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }, 200);
}



function getChartConfig(label, colorIndex, yLabels = null) {
  const currentTheme = document.body.className || 'theme-midnight';
  const theme = themes[currentTheme] || themes['theme-midnight'];

  const config = {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        label: label,
        data: [],
        borderColor: theme.colors[colorIndex],
        borderWidth: 2,
        pointRadius: 0, // cleaner look
        pointHoverRadius: 4,
        tension: 0.4 // smooth curves
      }]
    },
    options: {
      animation: false,
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: {
          display: false, // Hide legend
          labels: { color: theme.textColor }
        }
      },
      scales: {
        x: {
          grid: { color: theme.gridColor },
          ticks: { color: theme.textColor }
        },
        y: {
          grid: { color: theme.gridColor },
          ticks: { color: theme.textColor },
          beginAtZero: false, // Allow scale to zoom in on data
          grace: '5%' // Add some breathing room at top/bottom
        }
      }
    }
  };

  // If custom yLabels are provided (for Head Position)
  if (yLabels) {
    config.options.scales.y.ticks.callback = function (value) {
      return yLabels[value];
    };
    config.options.scales.y.min = 0;
    config.options.scales.y.max = yLabels.length - 1;
    config.options.scales.y.beginAtZero = true;
    config.options.scales.y.grace = 0;
    // Store labels in options for theme updates
    config.options.plugins.yLabels = yLabels;
  }

  return config;
}

function initSubplotCharts() {
  chartHR = new Chart(document.getElementById('chart-hr').getContext('2d'), getChartConfig('Heart Rate', 0));
  chartSpO2 = new Chart(document.getElementById('chart-spo2').getContext('2d'), getChartConfig('SpO2', 1));
  chartTemp = new Chart(document.getElementById('chart-temp').getContext('2d'), getChartConfig('Temperature', 2));
  // chartHead removed - using SVG visualization instead
  chartSnoring = new Chart(document.getElementById('chart-snoring').getContext('2d'), getChartConfig('Snoring', 4));
}

// Update head position visualization
function updateHeadPosition(position) {
  const headSvg = document.getElementById('head-visualization');
  const positionLabel = document.getElementById('position-label');

  if (!headSvg || !positionLabel) return;

  // Map positions to rotation angles
  const rotations = {
    "Extreme Left": -45,
    "Medium Left": -22.5,
    "Relatively Up": 0,
    "Medium Right": 22.5,
    "Extreme Right": 45
  };

  const angle = rotations[position] || 0;
  headSvg.style.transform = `rotate(${angle}deg)`;
  positionLabel.textContent = position || "Unknown";
}

function updateSubplotCharts(parsed) {
  const timeLabel = parsed.time;

  // Helper to push and limit data points
  const pushData = (chart, val) => {
    chart.data.labels.push(timeLabel);
    chart.data.datasets[0].data.push(val);

    // Keep only last 50 points to prevent memory issues/lag
    if (chart.data.labels.length > 50) {
      chart.data.labels.shift();
      chart.data.datasets[0].data.shift();
    }
    chart.update('none');
  };

  // Debug logging
  console.log('Parsed data:', parsed);
  console.log('HR:', Number(parsed.heartRate), 'SpO2:', Number(parsed.spo2), 'Temp:', Number(parsed.temperature));

  pushData(chartHR, Number(parsed.heartRate) || null);
  pushData(chartSpO2, Number(parsed.spo2) || null);
  pushData(chartTemp, Number(parsed.temperature) || null);

  // Update head position visualization
  const normalize = str => str ? str.toLowerCase().split(' ').map(w => w.charAt(0).toUpperCase() + w.slice(1)).join(' ') : "";
  const normalizedPosition = normalize(parsed.headPosition);
  updateHeadPosition(normalizedPosition);

  pushData(chartSnoring, Number(parsed.snoring) || 0);
}

function parseLine(data) {
  try {
    if (!data || typeof data !== "string") throw new Error("Invalid input");

    const dataArray = data.split(",").map(p => p.trim());

    return {
      time: dataArray[0] ?? "-",
      temperature: dataArray[1] ?? "-",
      heartRate: dataArray[2] ?? "-",
      spo2: dataArray[3] ?? "-",
      headPosition: dataArray[4] ?? "-",
      snoring: dataArray[5] ?? "-"
    };
  } catch (e) {
    console.error("[ERR] Parsing error:", e);
    return null;
  }
}

// ---- Theme Logic ----
function setupThemeSwitcher() {
  themeSelect.addEventListener('change', (e) => {
    const newTheme = e.target.value;
    document.body.className = newTheme;
    updateChartsTheme(newTheme);
  });
}

function updateChartsTheme(themeName) {
  const theme = themes[themeName];
  const charts = [chartHR, chartTemp, chartSpO2, chartSnoring]; // Removed chartHead

  charts.forEach((chart, index) => {
    // Update colors
    chart.data.datasets[0].borderColor = theme.colors[index];

    // Update scales
    chart.options.scales.x.grid.color = theme.gridColor;
    chart.options.scales.x.ticks.color = theme.textColor;
    chart.options.scales.y.grid.color = theme.gridColor;
    chart.options.scales.y.ticks.color = theme.textColor;

    // Restore custom yLabels if they exist
    if (chart.options.plugins.yLabels) {
      chart.options.scales.y.ticks.callback = function (value) {
        return chart.options.plugins.yLabels[value];
      };
      chart.options.scales.y.beginAtZero = true;
      chart.options.scales.y.grace = 0;
    } else {
      // Ensure scaling options persist for others
      chart.options.scales.y.beginAtZero = false;
      chart.options.scales.y.grace = '5%';
    }

    // Update legend
    chart.options.plugins.legend.display = false;
    chart.options.plugins.legend.labels.color = theme.textColor;

    chart.update();
  });
}

// bluetooth handlers
function onDisconnected() {
  setBLEStatus("Status: Disconnected");
  connectBtn.disabled = false;
  disconnectBtn.disabled = true;
  logLine("[BLE] disconnected");
}

async function connect() {
  setBLEStatus("requesting device...");

  try {
    device = await navigator.bluetooth.requestDevice({
      acceptAllDevices: true,
      optionalServices: [NUS_SERVICE_UUID],
    });

    // device = await navigator.bluetooth.requestDevice({
    //   filters: [
    //     {
    //       namePrefix: "FeatherSense",
    //       services: [NUS_SERVICE_UUID]
    //     }
    //   ]
    // });

    device.addEventListener("gattserverdisconnected", onDisconnected);

    setBLEStatus("connecting...");
    server = await device.gatt.connect();

    const service = await server.getPrimaryService(NUS_SERVICE_UUID);
    txChar = await service.getCharacteristic(NUS_TX_UUID);
    rxChar = await service.getCharacteristic(NUS_RX_UUID);

    await txChar.startNotifications();
    txChar.addEventListener("characteristicvaluechanged", onNotify);

    setBLEStatus("Connected to " + (device.name || "device"));
    connectBtn.disabled = true;
    disconnectBtn.disabled = false;
    logLine("[BLE] connected");
  } catch (err) {
    setBLEStatus("Error");
    logLine("[ERR] " + err.message);
    console.error(err);
  }
}

async function disconnect() {
  if (device?.gatt?.connected) device.gatt.disconnect();
}

function onNotify(event) {
  const value = event.target.value;
  const chunk = new TextDecoder().decode(value);

  partial += chunk;

  let idx;
  while ((idx = partial.indexOf("\n")) >= 0) {
    const line = partial.slice(0, idx).trim();
    partial = partial.slice(idx + 1);

    if (!line) continue;

    logLine(line);

    const parsed = parseLine(line);
    if (parsed) {
      samples.push(parsed);
      // addRow(parsed)
      updateSubplotCharts(parsed);
    }
  }
}


// button wiring
connectBtn.onclick = connect;
disconnectBtn.onclick = disconnect;
exportBtn.onclick = exportData;

clearBtn.onclick = () => {
  samples.length = 0;

  // Clear charts
  [chartHR, chartTemp, chartSpO2, chartSnoring].forEach(chart => {
    chart.data.labels = [];
    chart.data.datasets[0].data = [];
    chart.update();
  });

  // Reset head position visualization
  updateHeadPosition("Relatively Up");

  // Clear table
  const tbody = metricsTable.querySelector("tbody");
  if (tbody) tbody.innerHTML = "";

  logBox.textContent = "";
};

if (!navigator.bluetooth) {
  setBLEStatus("Web Bluetooth not supported in this browser");
  connectBtn.disabled = true;
}

// Initialize
initSubplotCharts();
setupThemeSwitcher();
