let updateInterval;
let cameraStreamActive = false;

document.addEventListener('DOMContentLoaded', function() {
    actualizarEstado();
    startAutoUpdate();
});

async function actualizarEstado() {
    try {
        const response = await fetch('/api/system_status');
        const data = await response.json();

        // Actualizar estado conexiones
        const esp32ConnectionStatus = document.getElementById('esp32ConnectionStatus');
        const camaraConnectionStatus = document.getElementById('camaraConnectionStatus');
        const systemStatus = document.getElementById('systemStatus');

        if (data.esp32_conectado) {
            esp32ConnectionStatus.textContent = '✅ Conectado al Arduino';
            esp32ConnectionStatus.style.color = '#28a745';
        } else {
            esp32ConnectionStatus.textContent = '❌ Arduino desconectado - Sistema no funcional';
            esp32ConnectionStatus.style.color = '#dc3545';
        }

        if (data.camara_conectada) {
            camaraConnectionStatus.textContent = '✅ Cámara conectada';
            camaraConnectionStatus.style.color = '#28a745';
        } else {
            camaraConnectionStatus.textContent = '❌ Cámara desconectada';
            camaraConnectionStatus.style.color = '#dc3545';
        }

        systemStatus.textContent = data.esp32_conectado ? '✅ Sistema Conectado' : '❌ Sistema Offline';
        systemStatus.style.background = data.esp32_conectado ? '#28a745' : '#dc3545';

        // Actualizar sistema de seguridad
        actualizarElemento('sistemaStatus', data.sistema_armado ? 'Armado' : 'Desarmado', data.sistema_armado);
        actualizarElemento('puertaPrincipalStatus', data.puerta_principal_abierta ? 'Abierta' : 'Cerrada', data.puerta_principal_abierta);
        actualizarElemento('accesoStatus', data.persona_autorizada ? 'Autorizada' : 'No autorizada', data.persona_autorizada);
        actualizarElemento('camaraStatus', data.camara_activa ? 'Activa' : (data.camara_conectada ? 'Conectada' : 'Desconectada'), data.camara_activa || data.camara_conectada);

        // Actualizar luces
        const luces = ['sala', 'cuarto_principal', 'cuarto', 'comedor', 'cocina', 'lavanderia', 'bano'];
        luces.forEach(luz => {
            const statusId = 'luz' + luz.charAt(0).toUpperCase() + luz.slice(1).replace('_', '') + 'Status';
            const element = document.getElementById(statusId);
            if (element) {
                actualizarElemento(statusId, data.luces[luz] ? 'Encendida' : 'Apagada', data.luces[luz]);
            }
        });

        // Actualizar monitor de cocina
        actualizarMonitorCocina(data);

        // Actualizar alarmas
        actualizarElemento('alarmaAccesoStatus', data.alarma_acceso ? 'Activada' : 'Desactivada', !data.alarma_acceso);
        actualizarElemento('alarmaCocinaStatus', data.alarma_cocina ? 'Activada' : 'Desactivada', !data.alarma_cocina);

        // Actualizar cards de alarmas
        document.getElementById('alarmaAcceso').className = `alarm-card ${data.alarma_acceso ? 'alarm-active' : 'alarm-inactive'}`;
        document.getElementById('alarmaCocina').className = `alarm-card ${data.alarma_cocina ? 'alarm-active' : 'alarm-inactive'}`;

        // Actualizar resumen consolidado
        document.getElementById('statDetecciones').textContent = data.detecciones_hoy || 0;
        document.getElementById('statDuracionPromedio').textContent = data.duracion_promedio ? data.duracion_promedio + 's' : '0s';
        document.getElementById('statUltimaAutorizada').textContent = data.ultima_deteccion || '--:--';
        document.getElementById('statUltimaNoAutorizada').textContent = data.ultimo_no_autorizado || '--:--';
        document.getElementById('statDistanciaActual').textContent =
            data.distancia_actual ? data.distancia_actual + ' cm' : '-- cm';

    } catch (error) {
        console.error('Error actualizando estado:', error);
        showNotification('Error de conexión con el servidor', 'error');
    }
}

function actualizarMonitorCocina(data) {
    const tempValue = document.getElementById('tempValue');
    const humidityValue = document.getElementById('humidityValue');
    const tempTime = document.getElementById('tempTime');
    const humidityTime = document.getElementById('humidityTime');
    const kitchenStatus = document.getElementById('kitchenStatus');
    const tempMetric = document.getElementById('tempMetric');
    const umbralTemp = document.getElementById('umbralTemp');

    // Actualizar valores
    if (data.temperatura > 0) {
        tempValue.textContent = data.temperatura.toFixed(1) + '°C';
        tempTime.textContent = data.ultima_actualizacion_cocina || '--:--';

        // Clasificar temperatura
        if (data.temperatura >= data.umbral_temperatura) {
            tempMetric.className = 'kitchen-metric temp-danger';
            tempValue.style.color = '#dc3545';
        } else if (data.temperatura >= data.umbral_temperatura - 5) {
            tempMetric.className = 'kitchen-metric temp-warning';
            tempValue.style.color = '#ffc107';
        } else {
            tempMetric.className = 'kitchen-metric temp-normal';
            tempValue.style.color = '#28a745';
        }
    } else {
        tempValue.textContent = '--°C';
        tempTime.textContent = '--:--';
        tempMetric.className = 'kitchen-metric';
        tempValue.style.color = '#666';
    }

    if (data.humedad > 0) {
        humidityValue.textContent = data.humedad.toFixed(1) + '%';
        humidityTime.textContent = data.ultima_actualizacion_cocina || '--:--';
        humidityValue.style.color = '#007bff';
    } else {
        humidityValue.textContent = '--%';
        humidityTime.textContent = '--:--';
        humidityValue.style.color = '#666';
    }

    // Actualizar estado general de cocina
    if (data.temperatura > 0 || data.humedad > 0) {
        if (data.alarma_cocina) {
            actualizarElemento('kitchenStatus', 'Alarma Activa', false);
        } else if (data.temperatura >= data.umbral_temperatura) {
            actualizarElemento('kitchenStatus', 'Temperatura Alta', false);
        } else {
            actualizarElemento('kitchenStatus', 'Normal', true);
        }
    } else {
        actualizarElemento('kitchenStatus', 'Sin datos', false);
    }

    // Actualizar umbral
    umbralTemp.textContent = data.umbral_temperatura.toFixed(1) + '°C';
}

function toggleCameraStream() {
    const streamImg = document.getElementById('cameraStream');
    const placeholder = document.getElementById('cameraPlaceholder');
    const button = document.getElementById('streamButton');

    if (!cameraStreamActive) {
        // Iniciar stream usando proxy
        streamImg.src = '/api/camara/stream?t=' + new Date().getTime();
        streamImg.onload = function() {
            placeholder.style.display = 'none';
            streamImg.style.display = 'block';
            button.textContent = 'Detener Stream';
            button.className = 'control-btn btn-off';
            cameraStreamActive = true;
            showNotification('✅ Stream de cámara iniciado', 'success');
        };
        streamImg.onerror = function() {
            showNotification('❌ Error cargando stream de cámara - Verifique la URL', 'error');
        };
    } else {
        // Detener stream
        streamImg.src = '';
        streamImg.style.display = 'none';
        placeholder.style.display = 'flex';
        button.textContent = 'Iniciar Stream';
        button.className = 'control-btn btn-action';
        cameraStreamActive = false;
        showNotification('⏹️ Stream de cámara detenido', 'success');
    }
}

function actualizarElemento(id, texto, esPositivo) {
    const elemento = document.getElementById(id);
    if (elemento) {
        elemento.textContent = texto;
        if (esPositivo !== undefined) {
            elemento.className = `area-status ${esPositivo ? 'status-on' : 'status-off'}`;
        }
    }
}

async function controlarSistema(armar) {
    try {
        const response = await fetch('/api/control/sistema', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({estado: armar})
        });
        const result = await response.json();
        showNotification(result.message, result.success ? 'success' : 'error');
        if (result.success) actualizarEstado();
    } catch (error) {
        showNotification('❌ Error de conexión', 'error');
    }
}

async function controlarPuerta(puerta, accion) {
    try {
        const response = await fetch(`/api/control/puerta/${puerta}/${accion}`, {method: 'POST'});
        const result = await response.json();
        showNotification(result.message, result.success ? 'success' : 'error');
        if (result.success) actualizarEstado();
    } catch (error) {
        showNotification('❌ Error de conexión', 'error');
    }
}

async function controlarLuz(habitacion, accion) {
    try {
        const response = await fetch(`/api/control/luz/${habitacion}/${accion}`, {method: 'POST'});
        const result = await response.json();
        showNotification(result.message, result.success ? 'success' : 'error');
        if (result.success) actualizarEstado();
    } catch (error) {
        showNotification('❌ Error de conexión', 'error');
    }
}

async function controlarAlarma(tipo, activar) {
    try {
        const response = await fetch(`/api/control/alarma/${tipo}`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({estado: activar})
        });
        const result = await response.json();
        showNotification(result.message, result.success ? 'success' : 'error');
        if (result.success) actualizarEstado();
    } catch (error) {
        showNotification('❌ Error de conexión', 'error');
    }
}

async function activarReconocimiento() {
    try {
        showNotification('🔍 Activando reconocimiento facial...', 'success');
        const response = await fetch('/api/reconocimiento/activar', {method: 'POST'});
        const result = await response.json();
        showNotification(result.message, result.success ? 'success' : 'error');
        if (result.success) actualizarEstado();
    } catch (error) {
        showNotification('❌ Error activando reconocimiento', 'error');
    }
}

async function simularReconocimiento(autorizada) {
    try {
        const response = await fetch('/api/reconocimiento/simular', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({autorizada: autorizada})
        });
        const result = await response.json();
        showNotification(result.message, autorizada ? 'success' : 'error');
        actualizarEstado();
    } catch (error) {
        showNotification('❌ Error de conexión', 'error');
    }
}

async function testESP32Connection() {
    try {
        const response = await fetch('/api/esp32/test_connection', {method: 'POST'});
        const result = await response.json();
        showNotification(result.message, result.success ? 'success' : 'error');
        actualizarEstado();
    } catch (error) {
        showNotification('❌ Error probando conexión con Arduino', 'error');
    }
}

async function setESP32IP() {
    try {
        const newIP = document.getElementById('esp32_ip').value.trim();
        if (!newIP) {
            showNotification('❌ Ingresa una IP válida para el Arduino', 'error');
            return;
        }

        const response = await fetch('/api/esp32/set_ip', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ip: newIP})
        });
        const result = await response.json();
        showNotification(result.message, result.success ? 'success' : 'error');
        if (result.success) setTimeout(testESP32Connection, 1000);
    } catch (error) {
        showNotification('❌ Error configurando IP del Arduino', 'error');
    }
}

async function testCamaraConnection() {
    try {
        const response = await fetch('/api/camara/test_connection', {method: 'POST'});
        const result = await response.json();
        showNotification(result.message, result.success ? 'success' : 'error');
        actualizarEstado();
    } catch (error) {
        showNotification('❌ Error probando conexión de cámara', 'error');
    }
}

async function setCamaraURL() {
    try {
        const newURL = document.getElementById('camara_url').value.trim();
        if (!newURL) {
            showNotification('❌ Ingresa una URL válida', 'error');
            return;
        }

        const response = await fetch('/api/camara/set_url', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({url: newURL})
        });
        const result = await response.json();
        showNotification(result.message, result.success ? 'success' : 'error');
        if (result.success) setTimeout(testCamaraConnection, 1000);
    } catch (error) {
        showNotification('❌ Error configurando URL de cámara', 'error');
    }
}

function showNotification(message, type) {
    const notification = document.createElement('div');
    notification.className = `notification ${type}`;
    notification.textContent = message;
    notification.style.display = 'block';
    document.body.appendChild(notification);

    setTimeout(() => {
        notification.style.opacity = '0';
        setTimeout(() => {
            if (notification.parentNode) {
                document.body.removeChild(notification);
            }
        }, 300);
    }, 3000);
}

function startAutoUpdate() {
    updateInterval = setInterval(actualizarEstado, 5000);
}

window.addEventListener('beforeunload', () => {
    if (updateInterval) clearInterval(updateInterval);
});