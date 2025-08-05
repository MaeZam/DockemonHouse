let stream = null;
let isScanning = false;
let credentialsValidated = false;
let faceVerified = false;

async function validateCredentials() {
    const username = document.getElementById('username').value;
    const password = document.getElementById('password').value;

    if (!username || !password) {
        showResult('Completa usuario y contraseña', 'error');
        return;
    }

    try {
        const validateBtn = document.getElementById('validateCredentialsBtn');
        validateBtn.textContent = 'Validando...';
        validateBtn.disabled = true;

        const response = await fetch('/validate_credentials', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                username: username,
                password: password
            })
        });

        const result = await response.json();

        if (result.success) {
            credentialsValidated = true;
            showResult('Datos Verificados ✅', 'success');

            // Mostrar sección de verificación facial
            document.getElementById('separator').style.display = 'block';
            document.getElementById('faceVerificationSection').style.display = 'block';

            // Actualizar botón
            validateBtn.textContent = 'Datos Verificados';
            validateBtn.style.background = 'linear-gradient(135deg, #28a745 0%, #20c997 100%)';

            // Deshabilitar campos
            document.getElementById('username').disabled = true;
            document.getElementById('password').disabled = true;

        } else {
            showResult(result.message, 'error');
            validateBtn.textContent = 'Continuar';
            validateBtn.disabled = false;
        }

    } catch (error) {
        console.error('Error:', error);
        showResult('Error de conexión', 'error');
        document.getElementById('validateCredentialsBtn').textContent = 'Continuar';
        document.getElementById('validateCredentialsBtn').disabled = false;
    }
}

async function startCamera() {
    if (!credentialsValidated) {
        showResult('Primero valida tus datos', 'error');
        return;
    }

    try {
        const videoElement = document.getElementById('videoElement');
        const videoOverlay = document.getElementById('videoOverlay');
        const startBtn = document.getElementById('startBtn');
        const verifyFaceBtn = document.getElementById('verifyFaceBtn');

        videoOverlay.innerHTML = '<p>Iniciando cámara...</p>';
        videoOverlay.style.display = 'block';

        stream = await navigator.mediaDevices.getUserMedia({
            video: {
                width: { ideal: 640 },
                height: { ideal: 480 }
            }
        });

        videoElement.srcObject = stream;
        videoOverlay.style.display = 'none';

        startBtn.textContent = 'Cámara Activa';
        startBtn.disabled = true;
        verifyFaceBtn.disabled = false;

        showResult('Cámara iniciada. Ahora verifica tu rostro.', 'info');

    } catch (error) {
        console.error('Error:', error);
        showResult('Error al acceder a la cámara. Verifica los permisos.', 'error');
        document.getElementById('videoOverlay').innerHTML = '<p>Error de cámara</p>';
    }
}

async function verifyFace() {
    if (!credentialsValidated || !stream || isScanning) return;

    try {
        isScanning = true;
        const videoElement = document.getElementById('videoElement');
        const videoContainer = document.getElementById('videoContainer');
        const verifyFaceBtn = document.getElementById('verifyFaceBtn');

        videoContainer.classList.add('scanning');
        verifyFaceBtn.textContent = 'ANALIZANDO...';
        verifyFaceBtn.disabled = true;

        showResult('Ejecutando reconocimiento facial...', 'info');

        // Capturar imagen del video
        const canvas = document.createElement('canvas');
        canvas.width = videoElement.videoWidth;
        canvas.height = videoElement.videoHeight;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(videoElement, 0, 0);

        // Convertir a blob
        const blob = await new Promise(resolve =>
            canvas.toBlob(resolve, 'image/jpeg', 0.8)
        );

        // Enviar al servidor
        const formData = new FormData();
        formData.append('image', blob, 'face_capture.jpg');

        const response = await fetch('/verify_face', {
            method: 'POST',
            body: formData
        });

        const result = await response.json();

        if (result.success) {
            faceVerified = true;
            showResult('ACCESO AUTORIZADO ✅ Redirigiendo...', 'success');

            verifyFaceBtn.textContent = 'Acceso Autorizado';
            verifyFaceBtn.style.background = 'linear-gradient(135deg, #28a745 0%, #20c997 100%)';

            // Detener cámara
            if (stream) {
                stream.getTracks().forEach(track => track.stop());
                document.getElementById('videoOverlay').innerHTML = '<p>Acceso Autorizado ✅</p>';
                document.getElementById('videoOverlay').style.display = 'block';
            }

            // Redirigir al dashboard después de 2 segundos
            setTimeout(() => {
                window.location.href = '/dashboard';
            }, 2000);

        } else {
            showResult(`${result.message} - Intenta de nuevo`, 'error');
            verifyFaceBtn.textContent = 'Verificar Rostro';
            verifyFaceBtn.disabled = false;
        }

    } catch (error) {
        console.error('Error:', error);
        showResult('Error de conexión con el servidor', 'error');
        document.getElementById('verifyFaceBtn').textContent = 'Verificar Rostro';
        document.getElementById('verifyFaceBtn').disabled = false;
    } finally {
        isScanning = false;
        document.getElementById('videoContainer').classList.remove('scanning');
    }
}

function showResult(message, type) {
    const resultDiv = document.getElementById('result');
    resultDiv.textContent = message;
    resultDiv.className = `result ${type}`;
    resultDiv.style.display = 'block';

    if (type === 'info') {
        setTimeout(() => {
            resultDiv.style.display = 'none';
        }, 5000);
    }
}

document.addEventListener('DOMContentLoaded', function() {
    document.getElementById('username').addEventListener('keypress', function(e) {
        if (e.key === 'Enter') {
            document.getElementById('password').focus();
        }
    });

    document.getElementById('password').addEventListener('keypress', function(e) {
        if (e.key === 'Enter') {
            validateCredentials();
        }
    });
});

window.addEventListener('beforeunload', () => {
    if (stream) {
        stream.getTracks().forEach(track => track.stop());
    }
});