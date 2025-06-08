# POC OCR 'java is a massive fail JNI use something else god'

# Prepare
wsl
cd /mnt/c/Users/guesa/lvm-back-truck-manager
dos2unix Containerfile gradlew build.gradle gradle.properties
chmod +x gradlew

# Build and run
podman build -t plate-inspection:latest -f Containerfile .
podman run -d -p 8080:8080 --name plate-inspection plate-inspection:latest

# Limpiar imágenes anteriores
podman rmi plate-inspection:latest

# Construir con la nueva imagen base
podman build -t plate-inspection:latest -f Containerfile .
# Test
curl -X POST -F "file=@plate.jpg" "http://localhost:8080/inspect-plate?width=1920&height=1080"

# Debug
podman logs plate-inspection
podman cp plate-inspection:/tmp/plateGray.png C:\Users\guesa\plateGray.png


# Reconstruir asap babyyyyyy
podman stop plate-inspection && podman rm plate-inspection
podman build -t plate-inspection:latest -f Containerfile .
podman run -d -p 8080:8080 --name plate-inspection plate-inspection:latest
