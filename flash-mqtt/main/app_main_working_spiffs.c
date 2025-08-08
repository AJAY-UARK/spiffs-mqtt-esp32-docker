/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/unistd.h> //for file open and close
#include <sys/stat.h> //file system interaction
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "esp_spiffs.h" //spiffs initialization
#include "esp_log.h"
#include "mqtt_client.h"

#include "esp_eth.h"

/* Forward declaration so we can call it before the full definition */
static void publisher_task(void *arg);

static const char *TAG = "mqtt_example";
/* ---------- NEW LINES ---------- */
static esp_mqtt_client_handle_t s_client     = NULL;
static volatile int             s_msg_count = 0;
static TaskHandle_t             pub_handle  = NULL;   /* <-- NEW guard */
/* -------------------------------- */

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    char* client_id = (char*) handler_args;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        //s_client = client;                               /* <-- save handle   */
        //xTaskCreate(publisher_task, "publisher_task", 4096, (void *)client_id, 5, NULL);   /* <-- spawn loop */
        s_client = client;                               /* save handle */

        if (pub_handle == NULL){
            xTaskCreate(publisher_task, "publisher_task", 4096, (void *)client_id, 5, &pub_handle);
        }

        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

/* ---------- NEW FUNCTION ---------- */
static void publisher_task(void *arg)
{
    //const char *topic = "/esp32/qemu/heartbeat";
    //char        payload[32];

    //while (1) {
        /* build a simple payload that increments */
    //    snprintf(payload, sizeof(payload), "beat_%d", s_msg_count++);
    //    esp_mqtt_client_publish(s_client, topic, payload,
    //                            0 /*len*/, 0 /*qos*/, 0 /*retain*/);

    //    ESP_LOGI(TAG, "Heartbeat published: %s", payload);
    //    vTaskDelay(pdMS_TO_TICKS(5000));      /* every 5 s */
    //}

    char *client_id = (char *)arg;
    char topic[64];
    snprintf(topic, sizeof(topic), "%s/data", client_id);

    FILE *f = fopen("/spiffs/data.txt", "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open /spiffs/data.txt");
        vTaskDelete(NULL);  // Stop the task
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f) != NULL) {
        // Remove newline (optional)
        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') line[len - 1] = '\0';

        int msg_id = esp_mqtt_client_publish(s_client, topic, line, 0, 1, 0);
        ESP_LOGI(TAG, "Published from file: %s (msg_id=%d)", line, msg_id);
        //vTaskDelay(pdMS_TO_TICKS(2000));  // 2 seconds delay between lines
        vTaskDelay(pdMS_TO_TICKS(200));   // feed WDT & yield
    }

    fclose(f);
    ESP_LOGI(TAG, "All lines from /spiffs/data.txt published.");
    vTaskDelete(NULL);  // Kill task after finishing

}
/* ------------------------------------ */


static void mqtt_app_start(void)
{
    
    // ---------- INSERT THIS BLOCK ----------
    //const char* env_client_id = getenv("CLIENT_ID");
    //ESP_LOGI(TAG, "CLIENT_ID from env: %s", env_client_id ? env_client_id : "(null)");

    //if (env_client_id && strlen(env_client_id) > 0) {
    //    mqtt_cfg.credentials.client_id = env_client_id;
    //}
    //else {
    //    mqtt_cfg.credentials.client_id = "esp32-default";
    //}
    // ---------- END INSERT ----------

    // ----------- MAC-BASED CLIENT ID -----------
    static char client_id[32] = {0};
    uint8_t mac[6] = {0};
    //esp_eth_handle_t eth_handle = NULL;
    // Get the default Ethernet netif and handle (assumes one ETH interface)
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
    if (netif) {
        esp_netif_get_mac(netif, mac);
        snprintf(client_id, sizeof(client_id), "esp32-%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        ESP_LOGI(TAG, "MAC-based CLIENT_ID: %s", client_id);
    } else {
        ESP_LOGE(TAG, "Could not get Ethernet netif! Using fallback client_id.");
        strcpy(client_id, "esp32_default");
    }
    // ----------- END MAC-BASED CLIENT ID ----------

    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .credentials.client_id = client_id,
    };


#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    //esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    /* pass client_id so the event-handler can hand it to publisher_task */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client_id);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    //SPIFFS
    ESP_LOGI(TAG, "Initializing SPIFFS");
 
    // Configure SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",      // mount point in VFS
        .partition_label = NULL,     // NULL since we use the default SPIFFS partition
        .max_files = 5, 
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "SPIFFS mounted successfully");
	// Test reading a file from SPIFFS
	FILE *f = fopen("/spiffs/data.txt", "r");
	if (f == NULL) {
	    ESP_LOGE(TAG, "Failed to open /spiffs/data.txt");
	} else {
	    char buffer[128];
	    if (fgets(buffer, sizeof(buffer), f)) {
	        ESP_LOGI(TAG, "Read from file: '%s'", buffer);
	    } else {
                ESP_LOGW(TAG, "File is empty or read error");
            }
            fclose(f);
        }

    }

        mqtt_app_start();
    } 
