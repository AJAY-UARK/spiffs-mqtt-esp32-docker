# spiffs-mqtt-esp32-docker

ESP32 (QEMU) + SPIFFS + MQTT example with Docker.  
This repo contains an ESP-IDF project (`flash-mqtt/`), pre-built flash images (`flash_device1.bin`, `flash_device2.bin`, `flash_device3.bin`, `flash_image.bin`), and Docker + compose files to run one or multiple simulated ESP32 devices.

---

## ⚠️ Quick links
- **Risky / destructive commands** (pull/push/erase): see [Troubleshooting & Safety](#troubleshooting--safety)
- **If you change the SPIFFS contents**: see [Edit SPIFFS content](#2---edit-spiffs-content)

---

## Quick summary
- Build firmware with `idf.py` (pass `DEVICE_ID` and `TOPIC` at build time).
- Generate SPIFFS image (`build/storage.bin`) from `spiffs_image/` (contains `data.txt`).
- Merge app + bootloader + partition + spiffs into `flash_image.bin` with `esptool.py`.
- Run locally with QEMU or via Docker containers (Dockerfile currently copies one `flash_deviceX.bin` into the image).

---

## Prerequisites (WSL / Ubuntu)
- **ESP-IDF v6.x** (`idf.py` in PATH)
- Python 3 (`python`)
- `esptool.py` (pip install esptool)
- `qemu-system-xtensa` (the Dockerfile bundles QEMU for containers)
- Docker & docker-compose (if using Docker)
- An MQTT client like `mosquitto_sub` for testing

> This README is a Markdown file — paste it into `README.md` in repo root so GitHub renders it.

---

## Repo layout (what’s important)
```
.
├─ Dockerfile
├─ docker-compose.yml
├─ flash-mqtt/                # ESP-IDF project (source)
│  ├─ main/
│  ├─ CMakeLists.txt
│  ├─ spiffs_image/           # source files (data.txt) - keep this
│  └─ build/                  # build outputs (can be large)
├─ flash_device1.bin
├─ flash_device2.bin
├─ flash_device3.bin
├─ flash_image.bin
└─ README.md
```

> Note: there is **no `tools/` directory** used here — this repo uses the `spiffsgen.py` that lives inside your ESP-IDF tree: `$IDF_PATH/components/spiffs/spiffsgen.py` (see SPIFFS section).

---

## 1 — Build firmware (with dynamic DEVICE_ID / TOPIC)
From repo root:

```bash
cd ~/spiffs-mqtt-esp32-docker/flash-mqtt

# build with device id and topic
idf.py -DDEVICE_ID="device3" -DTOPIC="device3/data" build

# optionally override broker
# idf.py -DDEVICE_ID="device3" -DTOPIC="device3/data" -DBROKER_URL="broker.hivemq.com" build
```

Output: `flash-mqtt/build/` contains `mqtt_tcp.bin`, `bootloader/bootloader.bin`, `partition_table/partition-table.bin`, etc.

---

## 2 — Edit SPIFFS content (data.txt)
If you want different payloads, edit:

```
flash-mqtt/spiffs_image/data.txt
```

Format (CSV `device,value`) — matches the project code:
```
device3,321
device3,654
device3,987
```

**Important:** regenerate the `storage.bin` *before* you run the `esptool.py merge_bin` step. See next section.

---

## 3 — Generate SPIFFS image (`build/storage.bin`)
Use the `spiffsgen.py` that comes with your ESP-IDF (the path you used earlier):

```bash
python $IDF_PATH/components/spiffs/spiffsgen.py 0x0B0000 ./flash-mqtt/spiffs_image flash-mqtt/build/storage.bin
```

- `0x0B0000` = SPIFFS partition **offset** used in this repo — verify with `flash-mqtt/build/partition_table/partition-table.csv` (or your `partitions.csv`) and use the offset shown there.
- If the partition **size** is needed by your tool, use the `Size` column from the same CSV.

**Do this step every time you change `spiffs_image/data.txt`** (before merge).

---

## 4 — Merge into single flash image
From repo root (creates `flash_image.bin`):

```bash
esptool.py --chip esp32 merge_bin -o flash_image.bin --fill-flash-size 2MB \
  0x1000  flash-mqtt/build/bootloader/bootloader.bin \
  0x8000  flash-mqtt/build/partition_table/partition-table.bin \
  0x10000 flash-mqtt/build/mqtt_tcp.bin \
  0xB0000  flash-mqtt/build/storage.bin
```

- Make sure `0xB0000` (the SPIFFS offset) matches the one in your partition table file. If you edited the partition table, use the updated offset/size values.

---

## 5 — Run in QEMU (local)
Run exactly the QEMU command you use in this repo (keeps flow identical to your setup):

```bash
qemu-system-xtensa -nographic -machine esp32 -drive file=flash_image.bin,if=mtd,format=raw
```

- This command boots the merged flash image and prints logs to the terminal. Look for `ESP_LOGI` messages showing `DEVICE_ID` and `TOPIC` for debugging.
- (No telnet instructions here — you can read the logs directly in the terminal where QEMU runs.)

---

## 6 — Docker: bake per-device flash into images and run
Your Dockerfile copies a chosen `flash_deviceX.bin` into the image. **Before running `docker build`**, edit the Dockerfile `COPY` line to point to the specific device binary you want baked into that image.

Example Dockerfile line (current):
```dockerfile
COPY flash_device2.bin /opt/flash_image.bin
```

### Build per-device images (concise flow)
```bash
# build image that contains flash_device2.bin
docker build -t docbuster/esp32-qemu-spiffs-mqtt:device-2 .

# if you want device1 image, update Dockerfile COPY to flash_device1.bin, then:
docker build -t docbuster/esp32-qemu-spiffs-mqtt:device-1 -f Dockerfile .
```

**After** building the images, update `docker-compose.yml` to reference the image tags you created (the compose file uses the image names). Then run:

```bash
docker-compose up
# or
docker-compose up -d
```

**Note:** You can also avoid rebuilding the Docker image each time by mounting a flash file into the container at runtime (advanced). For now, modifying `COPY` then `docker build` keeps things simple and reproducible.

---

## 7 — If you change `spiffs_image/data.txt` (summary)
1. Edit `flash-mqtt/spiffs_image/data.txt`.  
2. Regenerate `flash-mqtt/build/storage.bin` with `$IDF_PATH/components/spiffs/spiffsgen.py` (see section 3).  
3. Run `esptool.py merge_bin` (section 4) to create a fresh `flash_image.bin`.  
4. If your Docker images embed flash files, **rebuild** the Docker image(s) that should include the updated file and update `docker-compose.yml` if necessary.

A link: see [Edit SPIFFS content](#2---edit-spiffs-content) above.

---

## 8 — Test MQTT externally
Subscribe to the topic your simulated device uses (example with HiveMQ public broker):

```bash
mosquitto_sub -h broker.hivemq.com -t "device3/data" -v
```

Replace broker and topic as needed.

---

## Troubleshooting & Safety
- **Check partition table offsets/sizes:** open `flash-mqtt/build/partition_table/partition-table.csv` (or your `partitions.csv`) and use the `Offset` and `Size` values for the SPIFFS partition in `spiffsgen.py` and `esptool` commands.
- **spiffsgen.py path:** use the ESP-IDF copy at `$IDF_PATH/components/spiffs/spiffsgen.py` (that’s what this repo uses).
- **MQTT messages not appearing:** verify `DEVICE_ID` and `TOPIC` printed in QEMU logs; then confirm you subscribe to the exact same topic (topic strings are case-sensitive).
- **Docker image wrong file baked:** edit `COPY` line in Dockerfile to the correct `flash_deviceX.bin`, rebuild the image, update `docker-compose.yml` to use that image tag, then `docker-compose up`.
- **Risky commands:** `git rm --cached`, `git push --force`, `esptool.py --port ... write_flash --erase-all` can remove or overwrite data. If you are unsure, check here first and post the command and full output.

---

