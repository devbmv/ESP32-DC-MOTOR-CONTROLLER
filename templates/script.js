function round1(value) {
  return Math.round(value * 10) / 10;
}

const save_settings_msg = document.getElementById("save_settings_msg");
const led = document.getElementById("alarma-led");
const alarmaAudio = document.getElementById("alerta-audio");
let intervale = { temperatura: null, alarma: null };

const camere = [
  { tempId: "tempBucataria", barId: "bar1", culoare: "bg-info" },
  { tempId: "tempParintiAntreu", barId: "bar2", culoare: "bg-primary" },
  { tempId: "tempParintiDormitor", barId: "bar3", culoare: "bg-secondary" },
  { tempId: "tempLeaving", barId: "bar4", culoare: "bg-success" },
  { tempId: "engineTempolea", barId: "bar5", culoare: "bg-warning" },
  { tempId: "tempRobert", barId: "bar6", culoare: "bg-danger" },
  { tempId: "tempIrka", barId: "bar7", culoare: "bg-dark" },
  { tempId: "tempBaia", barId: "bar8", culoare: "bg-light text-dark" },
  { tempId: "engineTemporidor", barId: "bar9", culoare: "bg-secondary text-light" },
];

function updateAlarmStatus() {
  fetch('/alarma')
    .then(res => res.json())
    .then(data => {
      const esteAlarma = data.alarma === true;
      const esteInDelay = data.ramas === true;

      // LED vizual
      led.style.display = (esteAlarma && !esteInDelay) ? "inline-block" : "none";

      // Sunet
      if (esteAlarma && !esteInDelay) {
        alarmaAudio.play();
      } else {
        alarmaAudio.pause();
        alarmaAudio.currentTime = 0;
      }
    });
}


function resetAlarma() {
  const btn = document.querySelector('button[onclick="resetAlarma()"]');
  const reset_msg = document.getElementById("reset_msg");
  btn.disabled = true;
  btn.classList.remove("btn-warning");
  btn.classList.add("btn-secondary");
  btn.innerText = "🔄 Se resetează...";

  fetch('/resetare-alarma', { method: 'POST' })
    .then(res => res.text())
    .then(text => {
      reset_msg.innerText = text;
      reset_msg.classList.add('text-success');

      setTimeout(() => {
        reset_msg.innerText = '';
        btn.disabled = false;
        btn.classList.remove("btn-secondary");
        btn.classList.add("btn-warning");
        btn.innerText = "🔕 Reset Alarma";
      }, 2000);

      updateAlarmStatus();
    })
    .catch(() => {
      reset_msg.innerText = "Eroare la resetare!";
      reset_msg.classList.add('text-danger');

      setTimeout(() => {
        reset_msg.innerText = '';
        btn.disabled = false;
        btn.classList.remove("btn-secondary");
        btn.classList.add("btn-warning");
        btn.innerText = "🔕 Reset Alarma";
      }, 2000);
    });
}



function updateTemperaturi() {
  fetch('#')
    .then(r => r.json())
    .then(data => {
      const valori = [
        data.s1, data.s2, data.s3, data.s4, data.s5,
        data.s6, data.s7, data.s8, data.s9
      ];

      valori.forEach((valoare, index) => {
        const camera = camere[index];
        const tempElem = document.getElementById(camera.tempId);
        const barElem = document.getElementById(camera.barId);

        // Tratăm toate cazurile posibile de eroare:
        if (
          valoare === null ||
          valoare === "null" ||
          typeof valoare !== 'number' ||
          isNaN(valoare)
        ) {
          tempElem.innerText = "Eroare";
          barElem.style.width = "0%";
          barElem.className = `progress-bar ${camera.culoare}`;
        } else {
          tempElem.innerText = valoare.toFixed(1);
          barElem.style.width = `${Math.min(valoare * 2, 100)}%`;
          barElem.className = `progress-bar ${camera.culoare}`;
        }
      });
    })
    .catch(() => {
      camere.forEach(camera => {
        document.getElementById(camera.tempId).innerText = "Eroare server";
        document.getElementById(camera.barId).style.width = "0%";
        document.getElementById(camera.barId).className = `progress-bar ${camera.culoare}`;
      });
    });
}


function sendSettings() {
  const btn = document.querySelector('button[onclick="sendSettings()"]');
  btn.disabled = true;
  btn.classList.remove("btn-primary");
  btn.classList.add("btn-secondary");
  btn.innerText = "💾 Salvare...";

  const body = new URLSearchParams();

  // Câmpuri numerice simple
  const campuri = [
    'pragPornire', 'pragOprire', 'pragCamera', 'tempMaxBoiler',
    'offsetCamera', 'timpActivareReleu', 'tempMinTur',
    'tempMinExterna', 'alarmaTempMax', 'intervalActualizare',
    'deactivateAlarmTime'  
  ];

  campuri.forEach(id => {
    const element = document.getElementById(id);
    if (element) body.append(id, element.value);
  });

  // Câmpuri de tip select (boolean)
  body.append('modNoapte', document.getElementById('modNoapte').value === 'Da');
  body.append('releuManual', document.getElementById('releuManual').value === 'Da');
  body.append('backupSetari', document.getElementById('backupSetari').value === 'Activ');

  fetch('/setari', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body
  })
    .then(r => r.text())
    .then(text => {
      save_settings_msg.innerText = text;
      save_settings_msg.classList.remove("text-danger");
      save_settings_msg.classList.add("text-success");

      setTimeout(() => {
        save_settings_msg.innerText = '';
        btn.disabled = false;
        btn.classList.remove("btn-secondary");
        btn.classList.add("btn-primary");
        btn.innerText = "💾 Salvează Setările";
      }, 4000);

      reinicializeazaIntervale();
    })
    .catch(() => {
      save_settings_msg.innerText = "Eroare la salvarea setărilor!";
      save_settings_msg.classList.remove("text-success");
      save_settings_msg.classList.add("text-danger");

      setTimeout(() => {
        save_settings_msg.innerText = '';
        btn.disabled = false;
        btn.classList.remove("btn-secondary");
        btn.classList.add("btn-primary");
        btn.innerText = "💾 Salvează Setările";
      }, 4000);
    });
}


function loadSettings(callback) {
  fetch('/get_settings').then(r => r.json()).then(data => {
    for (const key in data) {
      const elem = document.getElementById(key);
      if (!elem) continue;
      if (elem.tagName === "SELECT") {
        elem.value = data[key] ? 'Da' : 'Nu';
      } else {
        elem.value = data[key];
      }
    }
    if (callback) callback(data);
  });
}

function reinicializeazaIntervale() {
  clearInterval(intervale.temperatura);
  clearInterval(intervale.alarma);

  loadSettings(data => {
    const actualizare = parseInt(data.intervalActualizare || 5);
    intervale.temperatura = setInterval(updateTemperaturi, actualizare * 1000);
    intervale.alarma = setInterval(updateAlarmStatus, 2000);
  });
}

window.onload = () => {
  reinicializeazaIntervale();
  updateTemperaturi();
  updateAlarmStatus();
};