FROM ubuntu:22.04

# 1. Install dependencies (no unnecessary bloat)
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        wget ca-certificates xz-utils \
        libpixman-1-0 libglib2.0-0 libfdt1 \
        libsdl2-2.0-0 libslirp0 && \
    rm -rf /var/lib/apt/lists/*

# 2. Download and install Espressif’s QEMU
RUN set -eux; \
    QEMU_URL="https://github.com/espressif/qemu/releases/download/esp-develop-9.2.2-20250228/qemu-xtensa-softmmu-esp_develop_9.2.2_20250228-x86_64-linux-gnu.tar.xz"; \
    wget -O /tmp/qemu.tar.xz "$QEMU_URL"; \
    tar -xJf /tmp/qemu.tar.xz -C /usr/local; \
    ln -s $(find /usr/local -name qemu-system-xtensa -type f | head -n1) /usr/local/bin/qemu-system-xtensa; \
    rm /tmp/qemu.tar.xz

# 3. Copy the correct flash image for this device
COPY flash_device2.bin /opt/flash_image.bin

# 4. Run QEMU on boot
ENTRYPOINT ["qemu-system-xtensa", "-nographic", "-machine", "esp32", "-drive", "file=/opt/flash_image.bin,if=mtd,format=raw"]
