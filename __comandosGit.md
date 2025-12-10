Actualizar desde origen remoto
Abre la terminal en VS Code dentro de la carpeta del repositorio.

Ejecuta:

bash
git fetch origin
git pull origin main
git fetch origin descarga los cambios del remoto sin aplicarlos.

git pull origin main descarga y aplica los cambios de la rama main (si tu rama principal se llama master, cámbialo).

✅ Versión rápida (solo aplicar cambios)
bash
git pull
Este comando hace fetch + merge automáticamente sobre la rama en la que estés.

📌 Notas importantes
Asegúrate de estar en la rama correcta:

bash
git branch
Si necesitas cambiar:

bash
git checkout main
Si tienes cambios locales sin guardar, haz commit o stash antes de hacer pull para evitar conflictos.

👉 ¿Quieres que te muestre también cómo hacer que tu rama local se mantenga siempre sincronizada automáticamente con el remoto (origin/main) para no tener que escribir git pull cada vez?


WLED/INSTALACION_ESP8266_ES.md at main · erpepe2004/WLED, Commits · erpepe2004/WLED+1
github.com
@erpepe2004 ➜ /workspaces/WLED (main) $ git fetch origin git pull origin main From https://github.com/erpepe2004/WLED * branch main -> FETCH_HEAD Updating 54e53c20..b80de827 error: Your local changes to the following files would be overwritten by merge: INSTALACION_ESP8266_ES.md Please commit your changes or stash them before you merge. Aborting
Ese error ocurre porque tienes cambios locales sin guardar en el archivo INSTALACION_ESP8266_ES.md. Git te protege para que no se sobrescriban al traer las actualizaciones del remoto. Tienes tres caminos posibles:

🔧 Opciones para resolverlo
1. Guardar tus cambios con un commit
Si quieres conservar lo que editaste:

bash
git add INSTALACION_ESP8266_ES.md
git commit -m "Actualización en instalación ESP8266"
git pull origin main
Esto primero guarda tus cambios en el historial y luego aplica los del remoto.

2. Guardar temporalmente con stash
Si aún no quieres hacer commit pero tampoco perder lo que escribiste:

bash
git stash
git pull origin main
git stash pop
git stash guarda tus cambios en una pila temporal.

git pull actualiza tu rama.

git stash pop recupera tus cambios encima de la nueva versión.

3. Descartar tus cambios
Si no necesitas lo que editaste y quieres quedarte con la versión del remoto:

bash
git checkout -- INSTALACION_ESP8266_ES.md
git pull origin main
⚠️ Esto borra tus modificaciones locales en ese archivo.