# spiffs-mqtt-esp32-docker

ESP32 (QEMU) + SPIFFS + MQTT example (QEMU-sim + Docker).
This repository contains an ESP-IDF project (`flash-mqtt/`), prebuilt flash images (`flash_device1.bin`, `flash_device2.bin`, `flash_device3.bin`, `flash_image.bin`), and a Dockerfile + `docker-compose.yml` to run simulated ESP32 devices.

**Repo root (example files present):**

```
Dockerfile  docker-compose.yml  flash_device1.bin  flash_device3.bin  qemu.log
README.md   flash-mqtt          flash_device2.bin  flash_image.bin    single_device_spiffs_Dockerfile
```

---

## Important safety note (read first)

Some commands can overwrite or erase data (for example: `esptool.py write_flash --erase-all`, `git push --force`). If you are unsure, copy command output and ask before running destructive commands.

---

## Quick summary of what you can do

* Run a prebuilt image in QEMU locally (fast).
* Rebuild firmware in `flash-mqtt/` with custom `DEVICE_ID` and `TOPIC`.
* Regenerate SPIFFS (from `flash-mqtt/spiffs_image/data.txt`) and merge into a new `flash_image.bin`.
* Build Docker images that embed a chosen `flash_device*.bin`, and run with `docker-compose`.

---

## 0. Files & locations to know (your repo)

* Root-level `flash_image.bin` — already merged image (QEMU / Docker can use this).
* Root-level `flash_device1.bin` `flash_device2.bin` `flash_device3.bin` — per-device binaries you can bake into Docker images.
* `flash-mqtt/` — the ESP-IDF project; contains:

  * `spiffs_image/` (source files, e.g. `data.txt`)
  * `build/` (build output after `idf.py build`)
  * `my_partitions.csv` or `build/partition_table/partition-table.csv` — contains SPIFFS offset/size

---

## 1. Prerequisites & environment setup

### A — Ubuntu / WSL (recommended)

```bash
# Update and essentials
sudo apt update && sudo apt upgrade -y
sudo apt install -y git python3 python3-pip build-essential cmake ninja-build ccache libffi-dev libssl-dev

# Install esptool (merge/flash)
pip3 install --user esptool

# MQTT test client
sudo apt install -y mosquitto-clients

# Docker
sudo apt install -y docker.io docker-compose
sudo usermod -aG docker $USER   # then log out and back in (or reboot)

# NOTE: QEMU xtensa binaries are not always in distro repos — you can use the Docker images in this repo (they bundle QEMU).
```

### B — macOS (Homebrew)

```bash
brew update
brew install git python cmake ninja ccache mosquitto
pip3 install --user esptool
# Install Docker Desktop from https://www.docker.com/get-started
```

### C — Windows

* Recommended: install **WSL2** + Ubuntu and follow the Ubuntu steps.
* Alternatively install Docker Desktop (includes docker-compose), Git for Windows, Python, and pip on native Windows.

---

## 2. Validate key tools

```bash
# Verify IDF only if you plan to build from source:
echo "$IDF_PATH"       # must be set if building from source
idf.py --version       # fails if IDF not installed

# Check esptool
~/.local/bin/esptool.py --version  # or `esptool.py --version` if in PATH

# Docker
docker --version
docker-compose --version
```

---

## 3. Running a prebuilt image (fast way — QEMU)

If you just want to let others reproduce behavior with your existing `flash_image.bin`:

```bash
cd /path/to/spiffs-mqtt-esp32-docker

# run QEMU with the merged flash image (this is the exact command used in this repo)
qemu-system-xtensa -nographic -machine esp32 -drive file=flash_image.bin,if=mtd,format=raw
```

* The QEMU terminal will show ESP logs (the program prints `DEVICE_ID` and `TOPIC` — useful to verify).
* If QEMU is missing locally, collaborators can use the Docker images provided here which include the QEMU binary.

---

## 4. Rebuild firmware from source (optional — for collaborators who want to modify code)

**A: Prepare ESP-IDF**
(If collaborators want to rebuild firmware, they must install ESP-IDF and run `export.sh` as described in official docs: [https://docs.espressif.com/projects/esp-idf/](https://docs.espressif.com/projects/esp-idf/))

Minimal steps (example, run once):

```bash
# clone esp-idf (pick appropriate tag/version)
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
# optionally check out a stable version tag, e.g. v6.0:
# git checkout v6.0
./install.sh
. ./export.sh           # in each shell where you use idf.py
idf.py --version
```

**B: Build the firmware in repo**

```bash
cd flash-mqtt
# Build and set device id & topic at compile time
idf.py -DDEVICE_ID="device3" -DTOPIC="device3/data" build
```

* `flash-mqtt/build/` will contain build outputs: `mqtt_tcp.bin`, `bootloader/bootloader.bin`, `partition_table/partition-table.bin`, etc.

---

## 5. Edit SPIFFS content (data files for publishing)

If you want to change what the simulated device publishes, edit:

```text
flash-mqtt/spiffs_image/data.txt
# Format: CSV lines like:
device3,321
device3,654
device3,987
```

**Important:** Regenerate the SPIFFS binary **before** merging flash images.

---

## 6. Generate SPIFFS image (`storage.bin`) — use the ESP-IDF script

This repo uses the `spiffsgen.py` from your ESP-IDF installation. Run the version from your IDF:

```bash
# from repo root
python $IDF_PATH/components/spiffs/spiffsgen.py 0x0B0000 ./flash-mqtt/spiffs_image flash-mqtt/build/storage.bin
```

* `0x0B0000` is the SPIFFS partition **offset** used here — verify by opening `flash-mqtt/my_partitions.csv` or `flash-mqtt/build/partition_table/partition-table.csv` and using the **Offset** column for the spiffs partition.
* If your partition table uses a different offset or size, substitute the correct offset and size.

---

## 7. Merge binaries into a single `flash_image.bin`

From repo root (this recreates `flash_image.bin`):

```bash
esptool.py --chip esp32 merge_bin -o flash_image.bin --fill-flash-size 2MB \
  0x1000  flash-mqtt/build/bootloader/bootloader.bin \
  0x8000  flash-mqtt/build/partition_table/partition-table.bin \
  0x10000 flash-mqtt/build/mqtt_tcp.bin \
  0xB0000  flash-mqtt/build/storage.bin
```

* Confirm `0xB0000` matches the SPIFFS offset in your partition table (if you changed partitions, update the offset accordingly).

---

## 8. Run QEMU locally (after merge)

```bash
# from repo root
qemu-system-xtensa -nographic -machine esp32 -drive file=flash_image.bin,if=mtd,format=raw
```

* Watch the console for `ESP_LOGI` lines that show `DEVICE_ID` and `TOPIC`.
* Use `mosquitto_sub` on host to confirm messages:

```bash
mosquitto_sub -h broker.hivemq.com -t "device3/data" -v
```

---

## 9. Docker workflow (bake flash into image & run)

**Important** — your repo’s `Dockerfile` lives at root and currently copies one flash image into the container. Edit the `COPY` line to pick the flash you want to bake.

Example in `Dockerfile`:

```dockerfile
# modify to choose the binary you want baked into the container:
COPY flash_device2.bin /opt/flash_image.bin
ENTRYPOINT ["qemu-system-xtensa", "-nographic", "-machine", "esp32", "-drive", "file=/opt/flash_image.bin,if=mtd,format=raw"]
```

**Build image for a device:**

```bash
# (edit Dockerfile COPY as needed, then:)
docker build -t docbuster/esp32-qemu-spiffs-mqtt:device-2 .
```

**Alternative: create per-device Dockerfiles quickly**

```bash
cp Dockerfile Dockerfile.device1
sed -i 's/flash_device2.bin/flash_device1.bin/' Dockerfile.device1
docker build -f Dockerfile.device1 -t docbuster/esp32-qemu-spiffs-mqtt:device-1 .

cp Dockerfile Dockerfile.device3
sed -i 's/flash_device2.bin/flash_device3.bin/' Dockerfile.device3
docker build -f Dockerfile.device3 -t docbuster/esp32-qemu-spiffs-mqtt:device-3 .
```

**Run with docker-compose**

* Update `docker-compose.yml` to reference the image tags you built, then:

```bash
docker-compose up
# or run in background
docker-compose up -d
```

**View logs**

```bash
docker ps
docker logs -f <container-id-or-name>
```

---

## 10. Quick “minimal” test script (single line)

This builds, regenerates SPIFFS, merges (assumes IDF and offsets correct):

```bash
cd flash-mqtt && idf.py -DDEVICE_ID="device3" -DTOPIC="device3/data" build && \
python $IDF_PATH/components/spiffs/spiffsgen.py 0x0B0000 ./spiffs_image build/storage.bin && \
cd .. && \
esptool.py --chip esp32 merge_bin -o flash_image.bin --fill-flash-size 2MB \
  0x1000 flash-mqtt/build/bootloader/bootloader.bin \
  0x8000 flash-mqtt/build/partition_table/partition-table.bin \
  0x10000 flash-mqtt/build/mqtt_tcp.bin \
  0xB0000 flash-mqtt/build/storage.bin
```

---

## 11. What to include in the repo (for collaborators)

To make this repo reproducible, include:

* `flash-mqtt/` (source), including `spiffs_image/data.txt`
* `flash-mqtt/my_partitions.csv` or generated `flash-mqtt/build/partition_table/partition-table.csv` (so offsets/sizes are visible)
* `Dockerfile`, `docker-compose.yml`
* `README.md` (this file)
* (Optional) `TESTING.md` with the short test plan

---

## 12. Troubleshooting & common errors

**spiffsgen.py: No such file**

* Check `echo $IDF_PATH` and `ls $IDF_PATH/components/spiffs/`. The script must exist there; if not, instruct collaborators to install the same ESP-IDF version or copy `spiffsgen.py` into the repo.

**esptool.py: command not found**

* Ensure `pip3 install --user esptool` was run and `$HOME/.local/bin` is in PATH; or install `esptool.py` system-wide.

**QEMU not found or incompatible**

* Use the Docker images provided (they bundle QEMU) or install Espressif's QEMU release from [https://github.com/espressif/qemu/releases](https://github.com/espressif/qemu/releases).

**Merged image boots but no MQTT messages**

* Verify the DEVICE\_ID & TOPIC printed in QEMU logs.
* Verify you subscribe to the exact topic (topics are case-sensitive).
* If using a public broker (eg `broker.hivemq.com`) ensure no firewall prevents network access.

**Docker image baked wrong binary**

* Edit `COPY` line in Dockerfile to the correct `flash_deviceX.bin`; rebuild image and update `docker-compose.yml`.

**Partition offsets mismatch**

* Always check `flash-mqtt/build/partition_table/partition-table.csv` or `flash-mqtt/my_partitions.csv` for offsets and sizes; use those values when running `spiffsgen.py` and `esptool merge_bin`.

---

## 13. Useful links (for collaborators)

* ESP-IDF get-started: [https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
* esptool: [https://github.com/espressif/esptool](https://github.com/espressif/esptool)
* Espressif QEMU releases (prebuilt xtensa QEMU): [https://github.com/espressif/qemu/releases](https://github.com/espressif/qemu/releases)
* Mosquitto client: [https://mosquitto.org/download/](https://mosquitto.org/download/)

---

## 14. If something fails

Copy the exact command you ran and paste the full terminal output into an issue or message; include:

* output of `echo $IDF_PATH`
* `idf.py --version` (if building)
* contents of `flash-mqtt/build/partition_table/partition-table.csv`

---

If you want, I can now commit this document into your repo (I will provide the exact `git add/commit/push` commands) or create a `flash-mqtt/README.md` with the focused ESP-IDF & QEMU install steps. Tell me which and I will produce the commands.
