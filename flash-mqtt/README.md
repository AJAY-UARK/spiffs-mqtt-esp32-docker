# Creating new binary for applications

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
Note: This export.sh file needs to be executed for each terminal and also after activates environment variables.  If you want the environment available in every new shell:
```bash
echo ". '$IDF_PATH/export.sh'" >> ~/.bashrc
source ~/.bashrc
```

### Clone This Repository

```bash
cd ~
git clone git@github.com:AJAY-UARK/spiffs-mqtt-esp32-docker.git
cd spiffs-mqtt-esp32-docker/flash-mqtt
```
  Note: If you find any previous builds CMake tries to create a symlink with that build. This creates nuisance, just clear the build.
```bash
rm -rf build/
```
### Configure Build

Use `idf.py` with device-specific build options:

```bash
idf.py -DDEVICE_ID="device1" -DTOPIC="device1/data" build
```
>Note: Update the device ID and Topics depends on the application.

Confirm build artifacts:

```bash
ls -lh build/*.bin build/bootloader/*.bin build/partition_table/*.bin || true
cat my_partitions.csv || true
```

### Edit SPIFFS content (if you need to change payloads)
```bash
# edit file
nano spiffs_image/data.txt
# (make your changes, then save)
```


### Generate SPIFFS image (use ESP-IDF's spiffsgen.py)

First verify SPIFFS partition offset from the partition CSV:
```bash
#check for spiffs, it is named as storage.
cat my_partitions.csv || true
```

Then run spiffsgen.py with the size of the storage shown (the repo README uses 0x50000). Example:

```bash
python $IDF_PATH/components/spiffs/spiffsgen.py 0x50000 ./spiffs_image build/storage.bin
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

>Verify: The flash image size usually around 2MB.
```bash
ls -lh flash_image.bin
```


---

## 2. Run the Pre-Built Binary in Docker

If you already have a `flash_image.bin`, you can skip ESP-IDF and only use Docker. If you don't have the binary follow the above steps and get the binary to the root ['spiffs-mqtt-esp32-docker/'](https://github.com/AJAY-UARK/spiffs-mqtt-esp32-docker/blob/main/README.md)

### Add Firmware Binary

If you just want to check the binary before scaling up. Utilise the Dockerfile, build with a tag and just run it. In order to do that install the basic docker needs from the spiffs-mqtt-esp32-docker/readme.
```bash
# build the image with a tag: flash-test:latest(release)
docker build -t flash-test:latest .
```
```bash
# Run the build image
docker run --rm -it --name flash-test flash-test:latest
```


#### For multiple devices:
Follow the instructions for building the binary, If creating multiple binaries at the same time, change the names of the binary file. If names are changed, update the Dockerfile before building at this command:
```Dockerfile
# 3. Copy the correct flash image for this device
COPY flash_image.bin /opt/flash_image.bin
```
>Update this only for the binaries with names different than flash_image.bin

Create the binaries and update the compose file named docker-compose.yml

```yaml
services:

  esp32-device-1:
    image: docbuster/esp32-qemu-spiffs-mqtt:device-1

  esp32-device-2:
    image: docbuster/esp32-qemu-spiffs-mqtt:device-2

  esp32-device-3:
    image: docbuster/esp32-qemu-spiffs-mqtt:device-3

```
>docker id - docbuster,
> 
>name of image - esp32-qemu-spiffs-mqtt:device-1

### Run with Docker Compose

```bash
docker-compose up -d
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
