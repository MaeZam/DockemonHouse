from flask import Flask, render_template, request, jsonify, Response, session, redirect, url_for, \
    send_from_directory
from flask_cors import CORS
import requests
import os
import json
import cv2
import face_recognition
import numpy as np
from datetime import datetime
import threading
import time
from PIL import Image
import io
from functools import wraps

app = Flask(__name__)
CORS(app)

# Configuración de sesiones
app.secret_key = 'tu_clave_secreta_super_segura_cambiala_en_produccion'

# Configuración
ESP32_IP = "192.168.100.88"
CAMARA_URL = 'http://192.168.100.24:8080/videofeed'
ROSTROS_DIR = "rostros_autorizados"

# Estado del sistema unificado
sistema_estado = {
    # Conexiones
    'esp32_conectado': False,
    'camara_conectada': False,
    'camara_activa': False,

    # Sistema de entrada
    'sistema_armado': True,
    'persona_autorizada': False,
    'puerta_principal_abierta': False,
    'puerta_trasera_abierta': False,
    'detecciones_hoy': 0,
    'ultima_deteccion': None,
    'ultimo_no_autorizado': None,
    'duracion_promedio': 0,

    # Sensor de distancia
    'distancia_actual': 0,
    'distancia_minima': 999,
    'distancia_maxima': 0,
    'ultima_lectura_distancia': None,

    # Luces de la casa
    'luces': {
        'sala': False,
        'cuarto_principal': False,
        'cuarto': False,
        'comedor': False,
        'cocina': False,
        'lavanderia': False,
        'bano': False
    },

    # Monitoreo cocina
    'temperatura': 0.0,
    'humedad': 0.0,
    'umbral_temperatura': 40.0,
    'ultima_actualizacion_cocina': None,

    # Alarmas
    'alarma_acceso': False,
    'alarma_cocina': False
}

# Rostros autorizados
rostros_conocidos = []
nombres_conocidos = []


#
# SISTEMA DE AUTENTICACIÓN
#
class SimpleFaceAuth:
    def __init__(self):
        self.face_image_encodings = None

        # Usuarios y contraseñas permitidos
        self.valid_users = {
            "marlen": "123456",
            "gerson": "gerson"  # Usuario adicional para el dashboard
        }

        self.setup_face_recognition()

    def setup_face_recognition(self):
        try:
            image_path = "assets/marlen.jpg"

            if not os.path.exists(image_path):
                print(f"Error: No se encontró {image_path}")
                return

            image = cv2.imread(image_path)
            face_loc = face_recognition.face_locations(image)[0]

            self.face_image_encodings = face_recognition.face_encodings(image, known_face_locations=[face_loc])[0]

        except Exception as e:
            print(f"Error al cargar imagen de referencia: {e}")

    def verify_face(self, image_data):
        try:
            if self.face_image_encodings is None:
                return False, "Sistema no inicializado - falta imagen de referencia"

            # Convertir imagen del navegador a formato OpenCV
            image = Image.open(io.BytesIO(image_data))
            frame = cv2.cvtColor(np.array(image), cv2.COLOR_RGB2BGR)

            face_locations = face_recognition.face_locations(frame)

            if face_locations != []:
                for face_location in face_locations:
                    face_frame_encodings = face_recognition.face_encodings(frame, known_face_locations=[face_location])[
                        0]
                    result = face_recognition.compare_faces([self.face_image_encodings], face_frame_encodings)

                    if result[0] == True:
                        return True, "Rostro reconocido"
                    else:
                        return False, "Rostro no reconocido"
            else:
                return False, "No se detectó ningún rostro en la imagen"

        except Exception as e:
            print(f"Error procesando imagen: {e}")
            return False, f"Error en el reconocimiento: {str(e)}"

    def verify_credentials(self, username, password):
        """Verificar usuario y contraseña"""
        return self.valid_users.get(username) == password


# Instancia del sistema de autenticación
face_auth = SimpleFaceAuth()


# Decorador para rutas que requieren autenticación
def login_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if 'user_authenticated' not in session or not session['user_authenticated']:
            return redirect(url_for('login'))
        return f(*args, **kwargs)

    return decorated_function


#
# RUTAS DE AUTENTICACIÓN
#
@app.route('/login')
def login():
    """Página de login"""
    return render_template('login.html')


@app.route('/validate_credentials', methods=['POST'])
def validate_credentials():
    """Endpoint para validar solo usuario y contraseña"""
    try:
        data = request.get_json()
        username = data.get('username')
        password = data.get('password')

        if not username or not password:
            return jsonify({
                'success': False,
                'message': 'Completar usuario y contraseña'
            })

        if face_auth.verify_credentials(username, password):
            # Guardar username en sesión temporal (no completamente autenticado aún)
            session['temp_username'] = username
            return jsonify({
                'success': True,
                'message': f'Datos verificados para {username}'
            })
        else:
            return jsonify({
                'success': False,
                'message': 'Usuario o contraseña incorrectos'
            })

    except Exception as e:
        return jsonify({
            'success': False,
            'message': f'Error: {str(e)}'
        })


@app.route('/verify_face', methods=['POST'])
def verify_face():
    """Endpoint para verificar rostro"""
    try:
        if 'temp_username' not in session:
            return jsonify({'success': False, 'message': 'Sesión expirada'})

        if 'image' not in request.files:
            return jsonify({'success': False, 'message': 'No se recibió imagen'})

        image_file = request.files['image']
        image_data = image_file.read()

        success, message = face_auth.verify_face(image_data)

        if success:
            # Autenticación completa exitosa
            session['user_authenticated'] = True
            session['username'] = session['temp_username']
            session.pop('temp_username', None)  # Limpiar sesión temporal

            log_evento(f"Login exitoso: {session['username']}")

            return jsonify({
                'success': True,
                'message': 'Autenticación completa exitosa'
            })
        else:
            return jsonify({
                'success': False,
                'message': message
            })

    except Exception as e:
        return jsonify({
            'success': False,
            'message': f'Error: {str(e)}'
        })


@app.route('/logout')
def logout():
    """Cerrar sesión"""
    username = session.get('username', 'Usuario')
    session.clear()
    log_evento(f"Logout: {username}")
    return redirect(url_for('login'))


#
# FUNCIONES DEL SISTEMA DOMÓTICO (SIN CAMBIOS)
#

def crear_directorios():
    """Crear directorios necesarios"""
    dirs = [ROSTROS_DIR, "rostros_desconocidos", "logs", "assets", "templates", "static/css", "static/js"]
    for directorio in dirs:
        if not os.path.exists(directorio):
            os.makedirs(directorio)


def cargar_rostros_autorizados():
    """Cargar rostros autorizados al inicio"""
    global rostros_conocidos, nombres_conocidos
    rostros_conocidos.clear()
    nombres_conocidos.clear()

    if not os.path.exists(ROSTROS_DIR):
        return

    for archivo in os.listdir(ROSTROS_DIR):
        if archivo.endswith(('.jpg', '.jpeg', '.png')):
            try:
                ruta = os.path.join(ROSTROS_DIR, archivo)
                imagen = face_recognition.load_image_file(ruta)
                encodings = face_recognition.face_encodings(imagen)

                if encodings:
                    rostros_conocidos.append(encodings[0])
                    nombres_conocidos.append(os.path.splitext(archivo)[0])
            except Exception as e:
                print(f"Error cargando {archivo}: {e}")


def comunicar_esp32(endpoint, datos=None):
    """Comunicarse con el ESP32"""
    try:
        url = f"http://{ESP32_IP}/{endpoint}"
        if datos:
            response = requests.post(url, json=datos, timeout=5)
        else:
            response = requests.get(url, timeout=5)
        return response
    except requests.exceptions.RequestException as e:
        print(f"Error comunicando con Arduino: {e}")
        return None


def verificar_conexion_esp32():
    """Verificar conexión con ESP32/Arduino"""
    response = comunicar_esp32("estado")
    if response and response.status_code == 200:
        sistema_estado['esp32_conectado'] = True
        try:
            data = response.json()

            # Actualizar datos del Arduino según su formato
            sistema_estado['sistema_armado'] = data.get('sistema', False)
            sistema_estado['persona_autorizada'] = data.get('persona_autorizada', False)
            sistema_estado['temperatura'] = data.get('temperatura', 0.0)
            sistema_estado['humedad'] = data.get('humedad', 0.0)
            sistema_estado['alarma_cocina'] = data.get('alarma_cocina', False)
            sistema_estado['alarma_acceso'] = data.get('alarma_acceso', False)

            # Actualizar última lectura de cocina si hay datos
            if sistema_estado['temperatura'] > 0 or sistema_estado['humedad'] > 0:
                sistema_estado['ultima_actualizacion_cocina'] = datetime.now().strftime('%H:%M:%S')

            # Actualizar distancia
            distancia = data.get('distancia', 0)
            if distancia > 0:
                sistema_estado['distancia_actual'] = distancia
                sistema_estado['ultima_lectura_distancia'] = datetime.now().strftime('%H:%M:%S')

                if sistema_estado['distancia_minima'] == 999:
                    sistema_estado['distancia_minima'] = distancia
                elif distancia < sistema_estado['distancia_minima']:
                    sistema_estado['distancia_minima'] = distancia

                if distancia > sistema_estado['distancia_maxima']:
                    sistema_estado['distancia_maxima'] = distancia

            # Actualizar estado de luces desde Arduino
            luces_data = data.get('luces', {})
            sistema_estado['luces']['sala'] = luces_data.get('sala', False)
            sistema_estado['luces']['cuarto'] = luces_data.get('cuarto2', False)
            sistema_estado['luces']['comedor'] = luces_data.get('comedor', False)
            sistema_estado['luces']['cocina'] = luces_data.get('cocina', False)
            sistema_estado['luces']['lavanderia'] = luces_data.get('lavanderia', False)
            sistema_estado['luces']['bano'] = luces_data.get('bano', False)

            return True
        except Exception as e:
            print(f"Error procesando datos del Arduino: {e}")
            return False

    sistema_estado['esp32_conectado'] = False
    print(f"Arduino no responde en {ESP32_IP}")
    return False


def verificar_conexion_camara():
    """Verificar conexión con cámara"""
    try:
        cap = cv2.VideoCapture(CAMARA_URL)
        if cap.isOpened():
            ret, frame = cap.read()
            cap.release()
            if ret:
                sistema_estado['camara_conectada'] = True
                return True

        sistema_estado['camara_conectada'] = False
        return False
    except Exception:
        sistema_estado['camara_conectada'] = False
        return False


def procesar_reconocimiento_facial():
    """Procesar reconocimiento facial cuando se detecta persona"""
    if len(rostros_conocidos) == 0:
        return False, "Sin rostros autorizados"

    try:
        start_time = time.time()
        cap = cv2.VideoCapture(CAMARA_URL)
        if not cap.isOpened():
            return False, "Error de cámara"

        sistema_estado['camara_activa'] = True

        for intento in range(5):
            ret, frame = cap.read()
            if not ret:
                continue

            frame_pequeno = cv2.resize(frame, (0, 0), fx=0.25, fy=0.25)
            rgb_frame = cv2.cvtColor(frame_pequeno, cv2.COLOR_BGR2RGB)

            ubicaciones_rostros = face_recognition.face_locations(rgb_frame)
            encodings_rostros = face_recognition.face_encodings(rgb_frame, ubicaciones_rostros)

            for encoding_rostro in encodings_rostros:
                coincidencias = face_recognition.compare_faces(
                    rostros_conocidos, encoding_rostro, tolerance=0.6
                )
                distancias = face_recognition.face_distance(rostros_conocidos, encoding_rostro)

                if True in coincidencias:
                    indice_mejor = np.argmin(distancias)
                    if coincidencias[indice_mejor]:
                        nombre = nombres_conocidos[indice_mejor]

                        # Calcular duración del proceso
                        end_time = time.time()
                        duracion_actual = int(end_time - start_time)

                        # Calcular promedio móvil simple
                        if sistema_estado['duracion_promedio'] == 0:
                            sistema_estado['duracion_promedio'] = duracion_actual
                        else:
                            sistema_estado['duracion_promedio'] = int(
                                (sistema_estado['duracion_promedio'] + duracion_actual) / 2)

                        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                        cv2.imwrite(f"logs/autorizada_{nombre}_{timestamp}.jpg", frame)

                        cap.release()
                        sistema_estado['camara_activa'] = False
                        return True, nombre

            time.sleep(0.5)

        # Calcular duración incluso si no se autoriza
        end_time = time.time()
        duracion_actual = int(end_time - start_time)

        if sistema_estado['duracion_promedio'] == 0:
            sistema_estado['duracion_promedio'] = duracion_actual
        else:
            sistema_estado['duracion_promedio'] = int((sistema_estado['duracion_promedio'] + duracion_actual) / 2)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        cv2.imwrite(f"rostros_desconocidos/no_autorizada_{timestamp}.jpg", frame)

        cap.release()
        sistema_estado['camara_activa'] = False
        return False, "No autorizada"

    except Exception as e:
        sistema_estado['camara_activa'] = False
        return False, f"Error: {e}"


def log_evento(evento):
    """Registrar evento en log"""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    log_entry = f"[{timestamp}] {evento}\n"

    try:
        with open("logs/sistema.log", "a", encoding='utf-8') as f:
            f.write(log_entry)
    except:
        pass

    print(f"LOG: {log_entry.strip()}")


#
# RUTAS DEL DASHBOARD (CON PROTECCIÓN DE LOGIN)
#

@app.route('/')
def index():
    """Ruta principal - redirige según estado de autenticación"""
    if 'user_authenticated' in session and session['user_authenticated']:
        return redirect(url_for('dashboard'))
    else:
        return redirect(url_for('login'))


@app.route('/dashboard')
@login_required
def dashboard():
    """Dashboard principal - REQUIERE AUTENTICACIÓN"""
    username = session.get('username', 'Usuario')
    return render_template('dashboard.html',
                           esp32_ip=ESP32_IP,
                           camara_url=CAMARA_URL,
                           total_rostros=len(rostros_conocidos),
                           username=username)


#
# APIs DEL DASHBOARD (PROTEGIDAS CON LOGIN)
#

@app.route('/api/system_status')
@login_required
def api_system_status():
    """API para obtener estado del sistema"""
    verificar_conexion_esp32()
    verificar_conexion_camara()
    return jsonify(sistema_estado)


@app.route('/api/control/sistema', methods=['POST'])
@login_required
def api_control_sistema():
    """Controlar sistema de seguridad"""
    data = request.get_json()
    estado = data.get('estado', False)

    # Usar endpoint del Arduino
    response = comunicar_esp32(f"sistema?estado={'true' if estado else 'false'}")

    if response and response.status_code == 200:
        sistema_estado['sistema_armado'] = estado
        mensaje = f"Sistema {'ARMADO' if estado else 'DESARMADO'}"
        log_evento(mensaje)
        return jsonify({'success': True, 'message': mensaje})
    else:
        return jsonify({'success': False, 'message': f'Error comunicando con Arduino en {ESP32_IP}'})


@app.route('/api/control/puerta/<puerta>/<accion>', methods=['POST'])
@login_required
def api_control_puerta(puerta, accion):
    """Controlar puertas"""
    puerta_key = f'puerta_{puerta}_abierta'

    if accion == 'abrir':
        if puerta == 'principal':
            # Puerta principal - usa endpoint /puerta (controlada por sistema de seguridad)
            response = comunicar_esp32("puerta")
        elif puerta == 'trasera':
            # NUEVA: Puerta trasera - usa endpoint /puerta_trasera (independiente)
            response = comunicar_esp32("puerta_trasera")
        else:
            return jsonify({'success': False, 'message': 'Puerta no válida'})

        if response and response.status_code == 200:
            sistema_estado[puerta_key] = True
            sistema_estado['ultima_deteccion'] = datetime.now().strftime('%H:%M')
            log_evento(f"Puerta {puerta} abierta")

            # La puerta se cierra automáticamente después de 3 segundos (Arduino)
            threading.Timer(3.5, lambda: setattr(sistema_estado, puerta_key, False)).start()
            return jsonify({'success': True, 'message': f'Puerta {puerta} abierta correctamente'})
        else:
            return jsonify({'success': False, 'message': f'Error comunicando con Arduino en {ESP32_IP}'})

    elif accion == 'cerrar':
        sistema_estado[puerta_key] = False
        log_evento(f"Puerta {puerta} cerrada manualmente")
        return jsonify({'success': True, 'message': f'Puerta {puerta} cerrada'})

    return jsonify({'success': False, 'message': 'Acción no válida'})


@app.route('/api/control/luz/<habitacion>/<accion>', methods=['POST'])
@login_required
def api_control_luz(habitacion, accion):
    """Controlar luces de la casa"""
    if habitacion not in sistema_estado['luces']:
        return jsonify({'success': False, 'message': 'Habitación no válida'})

    # Mapear nombres de habitaciones para el Arduino
    habitacion_map = {
        'cuarto_principal': 'cuarto2',
        'cuarto': 'cuarto2',
        'bano': 'bano',
        'sala': 'sala',
        'comedor': 'comedor',
        'cocina': 'cocina',
        'lavanderia': 'lavanderia'
    }

    arduino_habitacion = habitacion_map.get(habitacion, habitacion)

    # Usar el endpoint del Arduino con el formato JSON exacto
    data = {
        "device": "light",
        "room": arduino_habitacion,
        "action": "on" if accion == 'encender' else "off"
    }

    print(f"Enviando a Arduino: {data}")  # Debug
    response = comunicar_esp32("api/control", data)

    if response and response.status_code == 200:
        try:
            result = response.json()
            if result.get('success', False):
                sistema_estado['luces'][habitacion] = (accion == 'encender')
                mensaje = f"Luz {habitacion.replace('_', ' ')} {accion}da"
                log_evento(mensaje)
                return jsonify({'success': True, 'message': mensaje})
            else:
                return jsonify({'success': False, 'message': 'Arduino rechazó el comando'})
        except:
            # Si no hay JSON de respuesta, asumir éxito si status 200
            sistema_estado['luces'][habitacion] = (accion == 'encender')
            mensaje = f"Luz {habitacion.replace('_', ' ')} {accion}da"
            log_evento(mensaje)
            return jsonify({'success': True, 'message': mensaje})
    else:
        return jsonify({'success': False, 'message': f'Error comunicando con Arduino en {ESP32_IP}'})


@app.route('/api/control/alarma/<tipo>', methods=['POST'])
@login_required
def api_control_alarma(tipo):
    """Controlar alarmas"""
    data = request.get_json()
    estado = data.get('estado', False)

    alarma_key = f'alarma_{tipo}'

    if tipo == 'acceso':
        # Alarma de acceso - usar endpoints específicos del Arduino
        endpoint = "alarma_acceso/encender" if estado else "alarma_acceso/apagar"
        response = comunicar_esp32(endpoint)

        if response and response.status_code == 200:
            sistema_estado[alarma_key] = estado
            mensaje = f"Alarma {tipo} {'ACTIVADA' if estado else 'DESACTIVADA'}"
            log_evento(mensaje)
            return jsonify({'success': True, 'message': mensaje})
        else:
            return jsonify({'success': False, 'message': f'Error comunicando con Arduino en {ESP32_IP}'})

    elif tipo == 'cocina':
        # Usar endpoints exactos del Arduino para alarma de cocina
        endpoint = "alarma/encender" if estado else "alarma/apagar"
        response = comunicar_esp32(endpoint)

        if response and response.status_code == 200:
            sistema_estado[alarma_key] = estado
            mensaje = f"Alarma {tipo} {'ACTIVADA' if estado else 'DESACTIVADA'}"
            log_evento(mensaje)
            return jsonify({'success': True, 'message': mensaje})
        else:
            return jsonify({'success': False, 'message': f'Error comunicando con Arduino en {ESP32_IP}'})
    else:
        return jsonify({'success': False, 'message': 'Tipo de alarma no válido'})


@app.route('/api/reconocimiento/activar', methods=['POST'])
@login_required
def api_reconocimiento_activar():
    """Activar reconocimiento facial real"""
    if len(rostros_conocidos) == 0:
        return jsonify({'success': False,
                        'message': 'No hay rostros autorizados. Agrega fotos en la carpeta rostros_autorizados/'})

    def procesar_async():
        autorizada, nombre = procesar_reconocimiento_facial()

        sistema_estado['persona_autorizada'] = autorizada

        if autorizada:
            sistema_estado['ultima_deteccion'] = datetime.now().strftime('%H:%M')
            sistema_estado['alarma_acceso'] = False

            # Autorizar en Arduino usando su endpoint
            response = comunicar_esp32("autorizar")
            if response and response.status_code == 200:
                log_evento(f"Persona autorizada en Arduino: {nombre}")
            else:
                log_evento(f"Error autorizando en Arduino: {nombre}")
        else:
            sistema_estado['ultimo_no_autorizado'] = datetime.now().strftime('%H:%M')
            sistema_estado['alarma_acceso'] = True

        sistema_estado['detecciones_hoy'] += 1
        log_evento(f"Reconocimiento facial: {nombre} ({'autorizada' if autorizada else 'no autorizada'})")

    threading.Thread(target=procesar_async, daemon=True).start()
    return jsonify({'success': True, 'message': 'Reconocimiento facial activado - Procesando...'})


@app.route('/api/reconocimiento/simular', methods=['POST'])
@login_required
def api_reconocimiento_simular():
    """Simular reconocimiento facial"""
    data = request.get_json()
    autorizada = data.get('autorizada', False)

    # Actualizar estado
    sistema_estado['persona_autorizada'] = autorizada

    if autorizada:
        sistema_estado['ultima_deteccion'] = datetime.now().strftime('%H:%M')
        sistema_estado['alarma_acceso'] = False
        mensaje = "Persona AUTORIZADA  - Acceso permitido"

        # Autorizar en Arduino
        response = comunicar_esp32("autorizar")
        if response and response.status_code == 200:
            mensaje += " (Arduino actualizado)"
        else:
            mensaje += " (Error actualizando Arduino)"
    else:
        sistema_estado['ultimo_no_autorizado'] = datetime.now().strftime('%H:%M')
        sistema_estado['alarma_acceso'] = True
        mensaje = "Persona NO AUTORIZADA - Alarma activada"

    sistema_estado['detecciones_hoy'] += 1
    log_evento(mensaje)
    return jsonify({'success': True, 'message': mensaje})


@app.route('/api/esp32/test_connection', methods=['POST'])
@login_required
def api_test_esp32():
    """Probar conexión con Arduino"""
    if verificar_conexion_esp32():
        return jsonify({'success': True, 'message': f'Conexión exitosa con Arduino en {ESP32_IP}'})
    else:
        return jsonify({'success': False,
                        'message': f'No se pudo conectar con Arduino en {ESP32_IP}. Verificar IP y estado del Arduino.'})


@app.route('/api/esp32/set_ip', methods=['POST'])
@login_required
def api_set_esp32_ip():
    """Configurar IP del Arduino"""
    data = request.get_json()
    new_ip = data.get('ip', '').strip()

    if not new_ip:
        return jsonify({'success': False, 'message': 'IP no válida'})

    global ESP32_IP
    ESP32_IP = new_ip
    return jsonify({'success': True, 'message': f'IP del Arduino configurada: {ESP32_IP}. Probando conexión...'})


@app.route('/api/camara/test_connection', methods=['POST'])
@login_required
def api_test_camara():
    """Probar conexión con cámara"""
    if verificar_conexion_camara():
        return jsonify({'success': True, 'message': 'Conexión exitosa con cámara'})
    else:
        return jsonify({'success': False, 'message': 'No se pudo conectar con la cámara'})


@app.route('/api/camara/set_url', methods=['POST'])
@login_required
def api_set_camara_url():
    """Configurar URL de la cámara"""
    data = request.get_json()
    new_url = data.get('url', '').strip()

    if not new_url:
        return jsonify({'success': False, 'message': 'URL no válida'})

    global CAMARA_URL
    CAMARA_URL = new_url
    return jsonify({'success': True, 'message': f'URL de cámara configurada: {CAMARA_URL}'})


@app.route('/api/camara/stream')
@login_required
def api_camara_stream():
    """Proxy para el stream de la cámara"""
    try:
        response = requests.get(CAMARA_URL, stream=True, timeout=5)

        def generate():
            for chunk in response.iter_content(chunk_size=1024):
                if chunk:
                    yield chunk

        return Response(generate(),
                        mimetype=response.headers.get('content-type', 'image/jpeg'),
                        headers={'Cache-Control': 'no-cache'})
    except Exception as e:
        return "Error de cámara", 500


@app.route('/api/recargar_rostros', methods=['POST'])
@login_required
def api_recargar_rostros():
    """Recargar rostros autorizados"""
    cargar_rostros_autorizados()
    return jsonify({'success': True, 'message': f'Rostros recargados: {len(rostros_conocidos)} encontrados'})


#
# INICIALIZACIÓN DEL SISTEMA
#

if __name__ == '__main__':
    print("DockemonHouse")

    crear_directorios()
    cargar_rostros_autorizados()

    print(f"Sistema de autenticación inicializado")
    print(f"Usuarios permitidos: {list(face_auth.valid_users.keys())}")
    print(f"Imagen de referencia: assets/marlen.jpg")

    print(f"\nConectando con Arduino en: {ESP32_IP}")
    if verificar_conexion_esp32():
        print("Arduino conectado y funcionando")

    else:
        print("Arduino NO responde")
        print("El sistema requiere Arduino para funcionar")
        print(f"   Verificar IP: {ESP32_IP}")
        print("   Verificar que Arduino esté encendido")
        print("   Verificar conexión WiFi del Arduino")

    if verificar_conexion_camara():
        print(f"Cámara conectada: {CAMARA_URL}")
    else:
        print(f"Cámara no disponible: {CAMARA_URL}")
        print("   Reconocimiento facial deshabilitado")

    print(f"\nLogin: http://localhost:5000/login")
    print(f"Dashboard: http://localhost:5000/dashboard (requiere autenticación)")
    print(f"Rostros autorizados: {len(rostros_conocidos)}")
    print("Puertas: Principal (con seguridad) + Trasera (independiente)")
    print("Autenticación: Usuario + Contraseña + Reconocimiento Facial")
    print("=" * 60)

    log_evento("Sistema casa inteligente con autenticación iniciado")
    app.run(debug=True, host='0.0.0.0', port=5000)