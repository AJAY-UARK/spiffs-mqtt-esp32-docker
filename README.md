# spiffs-mqtt-esp32-docker

This repository allows you to simulate **ESP32 devices** running MQTT applications inside Docker using QEMU. It is designed for testing and scaling multiple virtual ESP32 devices without requiring physical hardware.

 **Note:** If you already have a **pre-built firmware binary** (e.g., `flash_image.bin`) follow the instructions below. If you want to build the firmware from source, you will need to install ESP-IDF separately.

---

### Install Docker & Docker Compose

* **Linux (Ubuntu/Debian):**

  ```bash
  sudo apt-get update
  sudo apt-get install docker.io docker-compose -y
  sudo systemctl enable docker
  sudo systemctl start docker
  ```


* **Windows/Mac:**
  Install [Docker Desktop](https://www.docker.com/products/docker-desktop/).

---

## Getting Started

### 1. Clone the Repository

```bash
git clone git@github.com:AJAY-UARK/spiffs-mqtt-esp32-docker.git
cd spiffs-mqtt-esp32-docker
```

### 2. Add Your Firmware Binary

* Copy your built firmware binary into the repo directory:

  ```bash
  cp /path/to/build/flash_image.bin ./flash_device1.bin
  ```
* For multiple devices, duplicate with unique names:

  ```bash
  cp ./flash_device1.bin ./flash_device2.bin
  cp ./flash_device1.bin ./flash_device3.bin
  ```

### 3. Run Docker Compose

Start the containers:

```bash
docker-compose up --build
```

View logs:

```bash
docker-compose logs -f
```

---

## Scaling Devices

You can scale the number of devices by editing `docker-compose.yml` and adding services for each `flash_deviceX.bin`. Each container will simulate a separate ESP32 device.

---

## Summary

* This readme is for running **pre-built ESP32 firmware binaries** in Docker/QEMU.
* No ESP-IDF setup is required unless you want to build new binaries.
* Supports scaling multiple virtual devices using Docker Compose.
