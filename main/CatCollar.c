#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/param.h>

#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/gpio.h"
// #include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_dev.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_websocket_client.h"
#include "esp_event.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "./ADXL343.h"
#include <arpa/inet.h> // For socket functions
#include <unistd.h>

// GPIO for button press
#define BUTTON_GPIO GPIO_NUM_15

// Buzzer GPIO
#define BUZZER_GPIO GPIO_NUM_33

// Master I2C
#define I2C_MASTER_SCL_PIN 22               // GPIO number for I2C CLK
#define I2C_MASTER_SDA_PIN 23               // GPIO number for I2C DATA
#define I2C_EXAMPLE_MASTER_NUM I2C_NUM_0    // i2c port
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE 0 // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE 0 // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_FREQ_HZ 100000   // i2c master clock freq
#define WRITE_BIT I2C_MASTER_WRITE          // i2c master write
#define READ_BIT I2C_MASTER_READ            // i2c master read
#define ACK_CHECK_EN true                   // i2c master will check ack
#define ACK_CHECK_DIS false                 // i2c master will not check ack
#define ACK_VAL 0x00                        // i2c ack value

// ADXL343
#define SLAVE_ADXL ADXL343_ADDRESS // 0x53
#define ACCEL_NACK_VAL 0x01        // i2c nack value (Was FF)

// 14-Segment Display
#define SLAVE_DISPLAY 0x70           // alphanumeric address
#define OSC 0x21                     // oscillator cmd
#define HT16K33_BLINK_DISPLAYON 0x01 // Display on cmd
#define HT16K33_BLINK_OFF 0          // Blink off cmd
#define HT16K33_BLINK_CMD 0x80       // Blink cmd
#define HT16K33_CMD_BRIGHTNESS 0xE0  // Brightness cmd
#define DISPLAY_NACK_VAL 0xFF
#define MAX_MESSAGE_LENGTH 16 // Maximum number of characters in a message
#define VISIBLE_CHARACTERS 4  // Number of characters visible on the display
#define SCROLL_THRESHOLD 4    // Threshold to decide whether to scroll

#define UART_NUM UART_NUM_0 // Using UART0
#define BUF_SIZE (1024)     // UART buffer size

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define EXAMPLE_ESP_WIFI_SSID "Group_6"
#define EXAMPLE_ESP_WIFI_PASS "smartsys"
#define EXAMPLE_ESP_MAXIMUM_RETRY 5

#define HOST_IP_ADDR "192.168.1.103"
#define PORT 3333

// cat collar definitions

bool is_leader = false;

typedef enum
{
    CAT_SLEEP = 0,         // Idle (sleeps on his back so roll & -Z)
    CAT_WANDER = 1,        // Active (X > 1)
    CAT_SPEED_MOONWALK = 2 // Vertical position (Pitch > 80 & Z +/- 1)
} CatState;

CatState previousState = CAT_SLEEP; // Initialize to CAT_SLEEP or another default state
TickType_t stateStartTime = 0;      // Initialize to zero
CatState current_cat_state = CAT_SLEEP;
SemaphoreHandle_t data_mutex;
int64_t reset_time = 0;
static EventGroupHandle_t s_wifi_event_group; /* FreeRTOS event group to signal when we are connected*/

static const char *TAG = "wifi station";

static int s_retry_num = 0;

esp_websocket_client_handle_t client;
bool isBuzzing = false;
const char *catId = "1"; // Set this ID for each device (change for each tracker)



#define MAX_LEADER_ID_LEN 40  // Adjust as needed

// Global variables
char current_leader_id[MAX_LEADER_ID_LEN] = "";
char previous_leader_id[MAX_LEADER_ID_LEN] = "";

void buzz(bool isBuzzing, const char *received_leader_id)
{
    xSemaphoreTake(data_mutex, portMAX_DELAY);
    // Compare the received leader ID with the current leader ID
    if (isBuzzing)
        {
            gpio_set_level(BUZZER_GPIO, 1); // Turn on the buzzer
            vTaskDelay(500 / portTICK_PERIOD_MS); // Buzz for 500ms
            ESP_LOGI(TAG, "Buzzing on");
            gpio_set_level(BUZZER_GPIO, 0); // Turn off the buzzer
            vTaskDelay(500 / portTICK_PERIOD_MS); // Pause for 500ms
    }
    else if (strcmp(received_leader_id, current_leader_id) != 0)
    {
        // Leader has changed, buzz
        ESP_LOGI(TAG, "Leader has changed from %s to %s", current_leader_id, received_leader_id);
        // Leader has changed
        strcpy(previous_leader_id, current_leader_id);
        strcpy(current_leader_id, received_leader_id);

        gpio_set_level(BUZZER_GPIO, 1); // Turn on the buzzer
        vTaskDelay(500 / portTICK_PERIOD_MS); // Buzz for 500ms
        ESP_LOGI(TAG, "Buzzing once");
        gpio_set_level(BUZZER_GPIO, 0); // Turn off the buzzer
        vTaskDelay(500 / portTICK_PERIOD_MS); // Pause for 500ms
    }
    else
    {
        // Ensure the buzzer is off
        gpio_set_level(BUZZER_GPIO, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    xSemaphoreGive(data_mutex);

}

static void websocket_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id)
    {
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "Received data: %.*s", data->data_len, (char *)data->data_ptr);

        // Ensure data_len does not exceed buffer size - 1 for null terminator
    if (data->data_len < MAX_LEADER_ID_LEN)
    {
        char received_leader_id[MAX_LEADER_ID_LEN];
        memcpy(received_leader_id, data->data_ptr, data->data_len);
        received_leader_id[data->data_len] = '\0'; // Null-terminate the string
        buzz(isBuzzing, received_leader_id);
    }


        // Null-terminate the received data
        char received_leader_id[MAX_LEADER_ID_LEN];
        memcpy(received_leader_id, data->data_ptr, data->data_len);
        received_leader_id[data->data_len] = '\0';
        ESP_LOGI(TAG, "Received leader ID: %s", received_leader_id);

        // Check if the received leader ID matches this device's catId
        if (strcmp(received_leader_id, catId) == 0)
        {
            isBuzzing = true; // Start buzzing if this device is the leader
            ESP_LOGI(TAG, "This device is the leader. Buzzing started.");
        }
        else
        {
            isBuzzing = false; // Stop buzzing if another cat is the leader
            ESP_LOGI(TAG, "This device is not the leader. Buzzing stopped.");
        }
        break;

    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected");
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WebSocket disconnected");
        break;

    default:
        break;
    }
}


static void initialize_websocket_client()
{
    esp_websocket_client_config_t websocket_cfg = {
        .uri = "ws://192.168.1.103:3000/buzz", // Replace with your Raspberry Pi IP and correct port
    };

    client = esp_websocket_client_init(&websocket_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket client");
        return;
    }

    esp_err_t ret = esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WebSocket events: %s", esp_err_to_name(ret));
        esp_websocket_client_destroy(client);
        return;
    }

    ret = esp_websocket_client_start(client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket client: %s", esp_err_to_name(ret));
        esp_websocket_client_destroy(client);
        return;
    }

    ESP_LOGI(TAG, "WebSocket client initialized and started");
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

float roll = 0, pitch = 0, x = 0, y = 0, z = 0;

// Function to get the current time as a string
void get_timestamp(char *buffer, size_t max_len)
{
    int64_t current_time = esp_timer_get_time();
    int64_t time_us = current_time - reset_time;
    int64_t seconds = time_us / 1000000;
    int minutes = (seconds / 60) % 60;
    int hours = (seconds / 3600) % 24;
    int day_seconds = seconds % 60;

    snprintf(buffer, max_len, "%02d:%02d:%02d", hours, minutes, day_seconds);
}

void print_status()
{
    char timestamp[16];
    get_timestamp(timestamp, sizeof(timestamp));

    const char *state_str = (current_cat_state == CAT_SLEEP) ? "Sleepy Time" : (current_cat_state == CAT_WANDER) ? "Wander Time"
                                                                                                                 : "Moonwalk Time";

    char message[64]; // Buffer for the message
    snprintf(message, sizeof(message), "%s, Cat state: %s\n", timestamp, state_str);

    // Create a UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return;
    }

    // Set up the server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST_IP_ADDR, &server_addr.sin_addr);

    // Send the message
    ssize_t sent_bytes = sendto(sockfd, message, strlen(message), 0,
                                (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (sent_bytes < 0)
    {
        perror("Failed to send message");
    }

    // Close the socket
    close(sockfd);
}

// UART configuration parameters
#define UART_NUM UART_NUM_0 // Using UART0
#define BUF_SIZE (1024)     // UART buffer size

void init_uart()
{
    // Install UART driver, using the default UART0
    uart_driver_install(UART_NUM, BUF_SIZE, BUF_SIZE, 0, NULL, 0);

    // Tell VFS to use UART0 driver for standard I/O (printf, etc.)
    esp_vfs_dev_uart_use_driver(UART_NUM);
}

CatState getCatState(float roll, float pitch, float x, float z, float y)
{
    // CAT_SLEEP: if roll is significant and Z is close to -1 (cat on its back)
    if (fabs(z) < 11 && fabs(y) < 2.0 && fabs(x) < 2.0)
    {
        return CAT_SLEEP;
    }
    // CAT_WANDER: if X-axis acceleration is greater than 1 (cat is moving)
    else if ((fabs(x) > 2.0 || fabs(y) > 2.0) && fabs(pitch) < 70)
    {
        return CAT_WANDER;
    }
    else if (fabs(pitch) >= 70)
    {
        return CAT_SPEED_MOONWALK;
    }
    return CAT_SLEEP;
}

// Function to track time and cat state, and print them on the same line
void trackStateTime(CatState currentState)
{
    if (currentState != current_cat_state)
    {
        // Lock the mutex and update the cat state
        xSemaphoreTake(data_mutex, portMAX_DELAY);
        CatState prev_state = current_cat_state;
        current_cat_state = currentState;
        print_status(); // Print time and cat state on the same line
        if (prev_state != current_cat_state)
        {
            reset_time = esp_timer_get_time();
        }
        xSemaphoreGive(data_mutex);
    }
}

// Button Logic

// Global variable to track the display mode
int display_mode = 0; // Start with mode 1 by default

void task_button_presses(void *pvParameters)
{
    int lastButtonState = 1; // Previous state of the button

    while (1)
    {
        // Read the current button state
        int currentState = gpio_get_level(BUTTON_GPIO);

        // Detect if the button is pressed (transition from HIGH to LOW)
        if (currentState == 0 && lastButtonState == 1)
        {

            // Cycle through display modes (1, 2, 3)
            display_mode = (display_mode + 1) % 3;
        }

        // Update last button state for debounce handling
        lastButtonState = currentState;

        vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay to avoid bouncing
    }
}

// Function to initiate i2c -- note the MSB declaration!
static void i2c_master_init()
{
    // Debug

    printf("\n>> i2c Config\n");

    int err;

    // Port configuration
    int i2c_master_port = I2C_EXAMPLE_MASTER_NUM;

    /// Define I2C configurations
    i2c_config_t conf = {0};
    conf.mode = I2C_MODE_MASTER;                        // Master mode
    conf.sda_io_num = I2C_MASTER_SDA_PIN;               // Default SDA pin
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;            // Internal pullup
    conf.scl_io_num = I2C_MASTER_SCL_PIN;               // Default SCL pin
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;            // Internal pullup
    conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ; // CLK frequency
    err = i2c_param_config(i2c_master_port, &conf);     // Configure
    if (err == ESP_OK)
    {
        printf("- parameters: ok\n");
    }

    // Install I2C driver
    err = i2c_driver_install(i2c_master_port, conf.mode,
                             I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
                             I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);
    if (err == ESP_OK)
    {
        printf("- initialized: yes\n");
    }

    // Data in MSB mode
    i2c_set_data_mode(i2c_master_port, I2C_DATA_MODE_MSB_FIRST, I2C_DATA_MODE_MSB_FIRST);
}

// Utility  Functions //////////////////////////////////////////////////////////

// Utility function to test for I2C device address -- not used in deploy
int testConnection(uint8_t devAddr, int32_t timeout)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int err = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return err;
}

// Utility function to scan for i2c device
static void i2c_scanner()
{
    int32_t scanTimeout = 1000;
    printf("\n>> I2C scanning ..."
           "\n");
    uint8_t count = 0;
    for (uint8_t i = 1; i < 127; i++)
    {
        // printf("0x%X%s",i,"\n");
        if (testConnection(i, scanTimeout) == ESP_OK)
        {
            printf("- Device found at address: 0x%X%s", i, "\n");
            count++;
        }
    }
    if (count == 0)
    {
        printf("- No I2C devices found!"
               "\n");
    }
}

////////////////////////////////////////////////////////////////////////////////

// Display Functions ///////////////////////////////////////////////////////////

// Font Table
// Updated Font Table from Adafruit's Library
static const uint16_t alphafonttable[] = {
    0b0000000000000000, //  (space)
    0b0000000000000110, //  !
    0b0000001000100000, //  "
    0b0001001011001110, //  #
    0b0001001011101101, //  $
    0b0000110000100100, //  %
    0b0010001101011101, //  &
    0b0000010000000000, //  '
    0b0010010000000000, //  (
    0b0000100100000000, //  )
    0b0011111111000000, //  *
    0b0001001011000000, //  +
    0b0000100000000000, //  ,
    0b0000000011000000, //  -
    0b0100000000000000, //  .
    0b0000110000000000, //  /
    0b0000110000111111, //  0
    0b0000000000000110, //  1
    0b0000000011011011, //  2
    0b0000000010001111, //  3
    0b0000000011100110, //  4
    0b0010000001101001, //  5
    0b0000000011111101, //  6
    0b0000000000000111, //  7
    0b0000000011111111, //  8
    0b0000000011101111, //  9
    0b0001001000000000, //  :
    0b0000101000000000, //  ;
    0b0010010000000000, //  <
    0b0000000011001000, //  =
    0b0000100100000000, //  >
    0b0001000010000011, //  ?
    0b0000001010111011, //  @
    0b0000000011110111, //  A
    0b0001001010001111, //  B
    0b0000000000111001, //  C
    0b0001001000001111, //  D
    0b0000000011111001, //  E
    0b0000000001110001, //  F
    0b0000000010111101, //  G
    0b0000000011110110, //  H
    0b0001001000001001, //  I
    0b0000000000011110, //  J
    0b0000010101110000, //  K
    0b0000000000111000, //  L
    0b0000010000110110, //  M
    0b0000000100110110, //  N
    0b0000000000111111, //  O
    0b0000000011110011, //  P
    0b0000000011111111, //  Q
    0b0000000011110011, //  R
    0b0000000011101101, //  S
    0b0001001000000001, //  T
    0b0000000000111110, //  U
    0b0000110000110000, //  V
    0b0010100000110110, //  W
    0b0010110100000000, //  X
    0b0001010100000000, //  Y
    0b0000110000001001, //  Z
    0b0000000000111001, //  [
    0b0000000000000000, //  (backslash)
    0b0000000000001111, //  ]
    0b0000110000000011, //  ^
    0b0000000000001000, //  _
    0b0000000100000000, //
    0b0000000011011111, //  a
    0b0010000001111000, //  b
    0b0000000011011000, //  c
    0b0000100010001110, //  d
    0b0000100001011000, //  e
    0b0000000001110001, //  f
    0b0000010010001110, //  g
    0b0001000001110000, //  h
    0b0001000000000000, //  i
    0b0000000000001110, //  j
    0b0011011000000000, //  k
    0b0000000000110000, //  l
    0b0001000011010100, //  m
    0b0001000001010000, //  n
    0b0000000011011100, //  o
    0b0000000011110011, //  p
    0b0000000011100111, //  q
    0b0000000001010000, //  r
    0b0000000011101101, //  s
    0b0000000001111000, //  t
    0b0000000000011100, //  u
    0b0010000000000100, //  v
    0b0010100000010100, //  w
    0b0010110100000000, //  x
    0b0001010100000000, //  y
    0b0000110000001001, //  z
    0b0000100101001001, //  {
    0b0001001000000000, //  |
    0b0010010010001001, //  }
    0b0000010100100000, //  ~
    0b0011111111111111, //  DEL
};

// AI Generated
//  Function to map a character to its 16-bit code
uint16_t encode_character(char c)
{
    if (c >= ' ' && c <= '~')
    {
        return alphafonttable[c - ' '];
    }
    else
    {
        return 0x0000; // For unsupported characters, use 0x0000
    }
}

// Turn on oscillator for alpha display
int alpha_oscillator()
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SLAVE_DISPLAY << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, OSC, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    return ret;
}

// Set blink rate to off
int no_blink()
{
    int ret;
    i2c_cmd_handle_t cmd2 = i2c_cmd_link_create();
    i2c_master_start(cmd2);
    i2c_master_write_byte(cmd2, (SLAVE_DISPLAY << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd2, HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | (HT16K33_BLINK_OFF << 1), ACK_CHECK_EN);
    i2c_master_stop(cmd2);
    ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd2, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd2);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    return ret;
}

// Set Brightness
int set_brightness_max(uint8_t val)
{
    int ret;
    i2c_cmd_handle_t cmd3 = i2c_cmd_link_create();
    i2c_master_start(cmd3);
    i2c_master_write_byte(cmd3, (SLAVE_DISPLAY << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd3, HT16K33_CMD_BRIGHTNESS | val, ACK_CHECK_EN);
    i2c_master_stop(cmd3);
    ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd3, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd3);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    return ret;
}
void test_alpha_display(void *arg)
{
    // Debug
    int ret;
    printf(">> Test Alphanumeric Display: \n");

    // Set up routines
    ret = alpha_oscillator(); // Turn on alpha oscillator
    if (ret == ESP_OK)
    {
        printf("- oscillator: ok \n");
    }
    ret = no_blink(); // Set display blink off
    if (ret == ESP_OK)
    {
        printf("- blink: off \n");
    }
    ret = set_brightness_max(0xF); // Set brightness to max
    if (ret == ESP_OK)
    {
        printf("- brightness: max \n");
    }

    uint16_t displaybuffer[VISIBLE_CHARACTERS];

    // Declare the message buffer here so it's available throughout the function
    char message[MAX_MESSAGE_LENGTH + 1]; // Buffer to store the message

    while (1)
    {
        // Prepare the message based on the current display mode
        if (display_mode == 0)
        {
            snprintf(message, MAX_MESSAGE_LENGTH + 1,"Cats");
        }
        else if (display_mode == 1)
        {
            // Use current sensor data to set the message
            CatState currentState = getCatState(roll, pitch, x, z, y); // Ensure you update roll, pitch, x, y, z in your main task
            trackStateTime(currentState);
            if (currentState == CAT_SLEEP)
            {
                snprintf(message, MAX_MESSAGE_LENGTH + 1, "Sleepy Time");
            }
            else if (currentState == CAT_WANDER)
            {
                snprintf(message, MAX_MESSAGE_LENGTH + 1, "Wander Time");
            }
            else if (currentState == CAT_SPEED_MOONWALK)
            {
                snprintf(message, MAX_MESSAGE_LENGTH + 1, "Moonwalk Time");
            }
        }
        else if (display_mode == 2)
        {
            int64_t current_time = esp_timer_get_time();
            int64_t time_us = current_time - reset_time; // Calculate elapsed time in ticks
            // upload to canvas JS
            float elapsedTimeSeconds = time_us / 1000000.0f; // Convert to seconds

            // Format the message to show the elapsed time
            snprintf(message, MAX_MESSAGE_LENGTH + 1, "%d.%01d", (int)elapsedTimeSeconds, (int)((elapsedTimeSeconds - (int)elapsedTimeSeconds) * 10));
        }

        // Ensure the message is null-terminated
        message[MAX_MESSAGE_LENGTH] = '\0';

        // Determine the message length
        size_t message_length = strlen(message);
        if (message_length > MAX_MESSAGE_LENGTH)
        {
            message_length = MAX_MESSAGE_LENGTH;
            message[MAX_MESSAGE_LENGTH] = '\0'; // Truncate the message if needed
            printf("Message truncated to 16 characters.\n");
        }

        if (message_length <= SCROLL_THRESHOLD)
        {
            memset(displaybuffer, 0x0000, sizeof(displaybuffer));

            // Convert message characters to 16-bit codes
            for (int i = 0; i < message_length && i < VISIBLE_CHARACTERS; i++)
            {
                displaybuffer[i] = encode_character(message[i]);
            }

            // Send characters to display over I2C
            i2c_cmd_handle_t cmd4 = i2c_cmd_link_create();
            i2c_master_start(cmd4);
            i2c_master_write_byte(cmd4, (SLAVE_DISPLAY << 1) | WRITE_BIT, ACK_CHECK_EN);
            i2c_master_write_byte(cmd4, (uint8_t)0x00, ACK_CHECK_EN);
            for (uint8_t i = 0; i < VISIBLE_CHARACTERS; i++)
            {
                i2c_master_write_byte(cmd4, displaybuffer[i] & 0xFF, ACK_CHECK_EN);
                i2c_master_write_byte(cmd4, displaybuffer[i] >> 8, ACK_CHECK_EN);
            }
            i2c_master_stop(cmd4);
            ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd4, 1000 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd4);
        }
        else
        {
            // Handle scrolling messages longer than 4 characters
            size_t total_length = message_length + VISIBLE_CHARACTERS;
            uint16_t message_buffer[total_length];

            // Initialize message buffer with spaces
            for (size_t i = 0; i < total_length; i++)
            {
                if (i < VISIBLE_CHARACTERS || i >= VISIBLE_CHARACTERS + message_length)
                {
                    message_buffer[i] = encode_character(' ');
                }
                else
                {
                    message_buffer[i] = encode_character(message[i - VISIBLE_CHARACTERS]);
                }
            }

            int offset = 0;

            // Scroll the message
            while (offset < total_length - VISIBLE_CHARACTERS + 1)
            {
                // Update displaybuffer with VISIBLE_CHARACTERS starting from offset
                for (int i = 0; i < VISIBLE_CHARACTERS; i++)
                {
                    displaybuffer[i] = message_buffer[offset + i];
                }

                // Send characters to display over I2C
                i2c_cmd_handle_t cmd4 = i2c_cmd_link_create();
                i2c_master_start(cmd4);
                i2c_master_write_byte(cmd4, (SLAVE_DISPLAY << 1) | WRITE_BIT, ACK_CHECK_EN);
                i2c_master_write_byte(cmd4, (uint8_t)0x00, ACK_CHECK_EN);
                for (uint8_t i = 0; i < VISIBLE_CHARACTERS; i++)
                {
                    i2c_master_write_byte(cmd4, displaybuffer[i] & 0xFF, ACK_CHECK_EN);
                    i2c_master_write_byte(cmd4, displaybuffer[i] >> 8, ACK_CHECK_EN);
                }
                i2c_master_stop(cmd4);
                ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd4, 1000 / portTICK_PERIOD_MS);
                i2c_cmd_link_delete(cmd4);

                vTaskDelay(pdMS_TO_TICKS(300));
                offset++;
            }

            // Clear the display after scrolling
            memset(displaybuffer, 0x0000, sizeof(displaybuffer));
            i2c_cmd_handle_t cmd_clear = i2c_cmd_link_create();
            i2c_master_start(cmd_clear);
            i2c_master_write_byte(cmd_clear, (SLAVE_DISPLAY << 1) | WRITE_BIT, ACK_CHECK_EN);
            i2c_master_write_byte(cmd_clear, (uint8_t)0x00, ACK_CHECK_EN);
            for (uint8_t i = 0; i < VISIBLE_CHARACTERS; i++)
            {
                i2c_master_write_byte(cmd_clear, displaybuffer[i] & 0xFF, ACK_CHECK_EN);
                i2c_master_write_byte(cmd_clear, displaybuffer[i] >> 8, ACK_CHECK_EN);
            }
            i2c_master_stop(cmd_clear);
            ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd_clear, 1000 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd_clear);
        }

        // Add delay before checking the display mode again
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// ADXL343 Functions ///////////////////////////////////////////////////////////

// Get Device ID
int getDeviceID(uint8_t *data)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SLAVE_ADXL << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, ADXL343_REG_DEVID, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SLAVE_ADXL << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data, ACK_CHECK_DIS);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

// Write one byte to register
int writeRegister(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SLAVE_ADXL << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN); // Device address + Write
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);                                  // Register address
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);                                 // Data to write
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS); // Execute
    i2c_cmd_link_delete(cmd);
    return ret; // Return result
}

// Read register
uint8_t readRegister(uint8_t reg)
{
    uint8_t data;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SLAVE_ADXL << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN); // Write device address
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);                                  // Send register address
    i2c_master_start(cmd);                                                          // Repeat start
    i2c_master_write_byte(cmd, (SLAVE_ADXL << 1) | I2C_MASTER_READ, ACK_CHECK_EN);  // Device address + read
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);                              // Read data
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS); // Execute command
    i2c_cmd_link_delete(cmd);
    return data; // Return the byte read
}

// read 16 bits (2 bytes)
int16_t read16(uint8_t reg)
{
    uint8_t reg1, reg2;
    reg1 = readRegister(reg);
    reg2 = readRegister(reg + 1);
    int16_t result = (int16_t)((reg2 << 8) | reg1);
    return result;
}

void setRange(range_t range)
{
    /* Red the data format register to preserve bits */
    uint8_t format = readRegister(ADXL343_REG_DATA_FORMAT);

    /* Update the data rate */
    format &= ~0x0F;
    format |= range;

    /* Make sure that the FULL-RES bit is enabled for range scaling */
    format |= 0x08;

    /* Write the register back to the IC */
    writeRegister(ADXL343_REG_DATA_FORMAT, format);
}

range_t getRange(void)
{
    /* Red the data format register to preserve bits */
    return (range_t)(readRegister(ADXL343_REG_DATA_FORMAT) & 0x03);
}

dataRate_t getDataRate(void)
{
    return (dataRate_t)(readRegister(ADXL343_REG_BW_RATE) & 0x0F);
}

////////////////////////////////////////////////////////////////////////////////

void set_cat_leader_status(bool is_currently_leader)
{
    // Lock the mutex
    xSemaphoreTake(data_mutex, portMAX_DELAY);

    is_leader = is_currently_leader;

    // Release the mutex
    xSemaphoreGive(data_mutex);
}

// Task to listen for leader notifications
void network_listener_task(void *pvParameters)
{
    // Create a UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0)
    {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    // Bind the socket to the specified port
    struct sockaddr_in local_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sockfd);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Listening for leader notifications on port %d", PORT);

    char rx_buffer[128];

    while (1)
    {
        // Listen for messages (this is just an example, adjust to your needs)
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sockfd, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len > 0)
        {
            rx_buffer[len] = '\0'; // Null-terminate received data
            ESP_LOGI(TAG, "Received message: %s", rx_buffer);

            // Assuming the received message is the new leader ID
            char received_leader_id[MAX_LEADER_ID_LEN];
            strncpy(received_leader_id, rx_buffer, MAX_LEADER_ID_LEN - 1);
            received_leader_id[MAX_LEADER_ID_LEN - 1] = '\0'; // Ensure null-terminated

            // Call buzz function with all parameters
            buzz(isBuzzing, received_leader_id);
        }
        else
        {
            ESP_LOGE(TAG, "Error receiving data");
        }

        vTaskDelay(100 / portTICK_PERIOD_MS); // Adjust delay as needed
    }

    close(sockfd);
    vTaskDelete(NULL);
}

////////////////////////////////////////////////////////////////////////////////

// function to get acceleration
void getAccel(float *xp, float *yp, float *zp)
{
    *xp = read16(ADXL343_REG_DATAX0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    *yp = read16(ADXL343_REG_DATAY0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    *zp = read16(ADXL343_REG_DATAZ0) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
    // printf("X: %.2f \t Y: %.2f \t Z: %.2f\n", *xp, *yp, *zp);
}

// equation from https://forum.arduino.cc/t/getting-pitch-and-roll-from-acceleromter-data/694148
// Task to continuously poll acceleration and calculate roll and pitch
static void test_adxl343()
{
    printf("\n>> Polling ADXL343\n");
    while (1)
    {
        // Get acceleration data and calculate roll and pitch as before
        float xSum = 0, ySum = 0, zSum = 0;
        int numSamples = 0;

        // Collect data for 2 seconds (4 samples with 500ms delay)
        for (int i = 0; i < 4; i++)
        {
            float xVal, yVal, zVal;
            getAccel(&xVal, &yVal, &zVal);

            // Accumulate readings
            xSum += xVal;
            ySum += yVal;
            zSum += zVal;
            numSamples++;

            // Delay for 500ms before the next reading
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

        // Calculate average values
        x = xSum / numSamples;
        y = ySum / numSamples;
        z = zSum / numSamples;

        // Calculate roll and pitch using the averaged values
        roll = atan2(y, z) * 57.3;
        pitch = atan2(-x, sqrt(y * y + z * z)) * 57.3;
        // printf("z: %f \t roll: %.2f \t pitch: %.2f \n", z, roll, pitch);
        // Determine the cat state and update the shared state
        CatState currentState = getCatState(roll, pitch, x, z, y);
        trackStateTime(currentState);
    }
}

void app_main()
{
    // Initialize the mutex
    data_mutex = xSemaphoreCreateMutex();

    // Routine
    i2c_master_init();
    i2c_scanner();

    // Initialize UART
    init_uart();

    // Set up the button GPIO
    gpio_reset_pin(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);

    // Initialize the buzzer GPIO
    gpio_reset_pin(BUZZER_GPIO);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BUZZER_GPIO, 0); // Ensure the buzzer is off initially

    // Connect to network
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta(); // Initialize Wi-Fi

    // Check for ADXL343
    uint8_t deviceID;
    getDeviceID(&deviceID);
    if (deviceID == 0xE5)
    {
        printf("\n>> Found ADAXL343\n");
    }

    // Disable interrupts
    writeRegister(ADXL343_REG_INT_ENABLE, 0);

    // Enable measurements
    writeRegister(ADXL343_REG_POWER_CTL, 0x08);

    // Create task to poll ADXL343 (accelerometer)
    xTaskCreate(test_adxl343, "test_adxl343", 4096, NULL, 5, NULL);

    // Create task for handling button presses (to switch display modes)
    xTaskCreate(task_button_presses, "task_button_presses", 2048, NULL, 5, NULL);

    // Create task for alphanumeric display
    xTaskCreate(test_alpha_display, "test_alpha_display", 4096, NULL, 5, NULL);

    // Create task for network listener for leader status updates
    xTaskCreate(network_listener_task, "network_listener_task", 4096, NULL, 5, NULL);

    // Initialize WebSocket connection and start receiving leader updates
    initialize_websocket_client();
}
