# Build a new binary file

This part demonstrates how to build and run an **ESP32 application** that reads files from SPIFFS and publishes them to an MQTT broker.

 **Build with ESP-IDF** → Generate firmware binary (`flash_image.bin`).

---

## 1. Build Firmware with ESP-IDF

### System packages & basic prerequisites
```bash
# update + essentials
sudo apt update && sudo apt upgrade -y

# install required build and runtime packages used by ESP-IDF and this repo
sudo apt install -y git wget xz-utils python3 python3-pip python3-venv unzip \
  cmake ninja-build ccache libffi-dev libssl-dev flex bison gperf libusb-1.0-0-dev \
  libncurses-dev build-essential socat mosquitto-clients

# esptool for merging or flashing images
pip3 install --user esptool
```

### Install ESP-IDF

```bash
# Clone ESP-IDF
cd ~
git clone -b v6.0 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Set environment variable for IDF_PATH
export IDF_PATH="$(pwd -P)"

# run the installer (this installs python packages & toolchain per ESP-IDF instructions)
./install.sh

# export environment for current shell (makes idf.py available)
. ./export.sh

# verify
idf.py --version
```
Note: This export.sh file needs to be executed for each terminal. If you want the environment available in every new shell:
```bash
echo '. "$IDF_PATH/export.sh"' >> ~/.bashrc
source ~/.bashrc
```

### Clone This Repository

```bash
cd ~
git clone git@github.com:AJAY-UARK/spiffs-mqtt-esp32-docker.git
cd spiffs-mqtt-esp32-docker/flash-mqtt
```

### Configure Build

Use `idf.py` with device-specific build options:

```bash
idf.py -DDEVICE_ID="device1" -DTOPIC="device1/data" build
```

Confirm build artifacts:

```bash
ls -lh build/*.bin build/bootloader/*.bin build/partition_table/*.bin || true
cat build/partition_table/partition-table.csv || true
```

### Edit SPIFFS content (if you need to change payloads)
```bash
# edit file
nano spiffs_image/data.txt
# (make your changes, then save)
```
If you changed spiffs_image/data.txt, regenerate SPIFFS below. If not, skip regeneration.

### Generate SPIFFS image (use ESP-IDF's spiffsgen.py)

First verify SPIFFS partition offset from the partition CSV:
```bash
# find the spiffs row and show offset
grep -i spiffs build/partition_table/partition-table.csv || true
# to show nicely:
column -t -s, build/partition_table/partition-table.csv | grep -i spiffs || true
```

Then run spiffsgen.py with the offset shown (the repo README uses 0x0B0000). Example:

```bash
python $IDF_PATH/components/spiffs/spiffsgen.py 0x0B0000 ./spiffs_image build/storage.bin
# confirm
ls -lh build/storage.bin
```

### Merge bins into a single flash_image.bin (esptool)

```bash
cd ~/spiffs-mqtt-esp32-docker
esptool.py --chip esp32 merge_bin -o flash_image.bin --fill-flash-size 2MB \
  0x1000  flash-mqtt/build/bootloader/bootloader.bin \
  0x8000  flash-mqtt/build/partition_table/partition-table.bin \
  0x10000 flash-mqtt/build/mqtt_tcp.bin \
  0xB0000  flash-mqtt/build/storage.bin
```

Verify: The flash image size usually around 2MB
```bash
ls -lh flash_image.bin
sha256sum flash_image.bin
```


---

## 2. Run the Pre-Built Binary in Docker

If you already have a `flash_image.bin`, you can skip ESP-IDF and only use Docker. If you don't have the binary follow the above steps and get the binary to the root 'spiffs-mqtt-esp32-docker/'

### Add Firmware Binary

Copy the binary into the repo root:

```bash
cp flash-mqtt/build/flash_image.bin ./flash_device1.bin
```

For multiple devices:

```bash
cp ./flash_device1.bin ./flash_device2.bin
cp ./flash_device1.bin ./flash_device3.bin
```

### Run with Docker Compose

```bash
docker-compose up --build
```

View logs:

```bash
docker-compose logs -f
```

---

## 3. Scaling Devices

To simulate multiple ESP32 devices, update `docker-compose.yml` and add services for each binary (e.g., `flash_device2.bin`, `flash_device3.bin`). Each container runs independently and publishes to its configured topic.

---

## Notes

* Use **ESP-IDF workflow** if you want to modify the application and rebuild.
* Use **Docker workflow** if you just want to run pre-built binaries.
* The repo uses SPIFFS to store data files and publishes them to MQTT topics defined during the build.
