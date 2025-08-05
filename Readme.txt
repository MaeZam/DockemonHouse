DockemonHouse – Tu Casa Inteligente a Otro Nivel

Este proyecto es un sistema de casa inteligente que usa reconocimiento facial, control de luces, sensores, cámaras y más. Es perfecto si quieres tener todo bajo control desde un mismo lugar.

¿Qué contiene el proyecto?
Dentro de la carpeta principal vas a encontrar: (Bueno las tienes que ajustar)

app.py: el código principal que corre con Flask.
requirements.txt: la lista de librerías que necesita Python.
templates/: las páginas web que verás (HTML).
static/: aquí están los estilos y scripts (CSS y JavaScript).
assets/: imagen de referencia para el reconocimiento facial.
rostros_autorizados/: fotos de las personas que sí tienen permiso.
rostros_desconocidos/: capturas de los que intenten entrar sin autorización.
logs/: registros de lo que pasa en el sistema.

Cómo instalarlo
Tener instalado Python 3.7 o superior.
Tener tu Arduino o ESP32 con el firmware configurado.
Contar con una cámara IP o USB compatible.
Descargar o clonar el proyecto.
Entrar a la carpeta del proyecto y ejecutar:
pip install flask flask-cors requests opencv-python face-recognition numpy pillow
O si prefieres usar el archivo de requisitos:
pip install -r requirements.txt
Crear la carpeta assets si no existe y agregar tu foto de referencia, por ejemplo:
mkdir -p assets
cp tu_foto.jpg assets/mar.jpg
Crear la carpeta de rostros autorizados y poner ahí las fotos de las personas con acceso:
mkdir -p rostros_autorizados
cp foto_persona1.jpg rostros_autorizados/juan.jpg
cp foto_persona2.jpg rostros_autorizados/maria.jpg

Configurar tu Arduino o ESP32 en la red WiFi y anotar su IP.
Configurar tu cámara IP y tener la URL del stream.

Cómo iniciar el sistema
En la terminal escribe:
python app.py
Después, abre tu navegador y entra a http://localhost:5000

Usuarios por defecto
El sistema ya viene con estos usuarios:
marlen / 123456 (Administrador)

Cómo funciona la autenticación
Primero escribes tu usuario y contraseña. Luego la cámara verifica tu rostro. Si todo coincide, entras al panel de control.

Qué puedes hacer con el sistema
Activar o desactivar el sistema de seguridad.
Controlar la puerta principal.
Usar reconocimiento facial para validar accesos.
Ver alertas de intentos de acceso no autorizados.
Encender y apagar luces de varias áreas como sala, cuartos, comedor, cocina, lavandería y baño.
Monitorear sensores de temperatura, humedad y distancia.
Recibir notificaciones en tiempo real.
Ver el video en vivo de la cámara.
Capturar imágenes cuando pasa algo.

Configuraciones que puedes cambiar
Si quieres modificar la IP del Arduino, abre app.py y cambia la línea que dice:
ESP32_IP = "192.----"
Para cambiar la URL de la cámara, en la línea de abajo:
CAMARA_URL = "http://192.----/videofeed"
Si necesitas agregar más usuarios, en app.py dentro de la clase SimpleFaceAuth añade:
"nuevo_usuario": "nueva_contraseña"

Si algo falla
Si el Arduino no responde:
Verifica que esté conectado a la red con ping.
Prueba los endpoints con curl.

Si la cámara no funciona:
Verifica que la URL funcione con curl.
Si usas DroidCam, revisa que esté bien instalado en tu teléfono y PC.
Si no reconoce rostros:
Confirma que la imagen de referencia existe en assets.
Asegúrate de que los rostros autorizados estén bien puestos.
Si sigue fallando, reinstala la librería face_recognition.
Si te salen errores de permisos en Linux o macOS:
Da permisos de ejecución al script:
chmod +x app.py
Ajusta permisos de las carpetas:
chmod 755 templates static assets rostros_autorizados

Dónde se guardan los registros
El sistema guarda todo en:
logs/sistema.log: registro principal de eventos.
logs/autorizada_NOMBRE_TIMESTAMP.jpg: fotos de accesos permitidos.
rostros_desconocidos/no_autorizada_TIMESTAMP.jpg: fotos de accesos no permitidos.

Personalización
Si quieres cambiar colores, edita los archivos CSS en static/css.
Para agregar más habitaciones o sensores, debes modificar app.py y las plantillas HTML correspondientes.

Soporte
Si tienes problemas:
Revisa los logs en logs/sistema.log.
Mira si hay errores en la consola de Python.
Confirma que la cámara y el Arduino estén bien conectados.
Junta capturas de pantalla y toda la información que puedas antes de pedir ayuda.
