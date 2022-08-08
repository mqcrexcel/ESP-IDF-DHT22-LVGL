/* LVGL Example project
 *
 * Basic project to test LVGL on ESP32 based projects.
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#include <stddef.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "DHT22.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"


#include "esp_log.h"
#include "mqtt_client.h"

/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "lvgl_helpers.h"

#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    #if defined CONFIG_LV_USE_DEMO_WIDGETS
        #include "lv_examples/src/lv_demo_widgets/lv_demo_widgets.h"
    #elif defined CONFIG_LV_USE_DEMO_KEYPAD_AND_ENCODER
        #include "lv_examples/src/lv_demo_keypad_encoder/lv_demo_keypad_encoder.h"
    #elif defined CONFIG_LV_USE_DEMO_BENCHMARK
        #include "lv_examples/src/lv_demo_benchmark/lv_demo_benchmark.h"
    #elif defined CONFIG_LV_USE_DEMO_STRESS
        #include "lv_examples/src/lv_demo_stress/lv_demo_stress.h"
    #else
        #error "No demo application selected."
    #endif
#endif

/*********************
 *      DEFINES
 *********************/
#define TAG "demo"

#define TAG1 "MQTT"

#define LV_TICK_PERIOD_MS 1

lv_obj_t * ta1;


/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_demo_application(void);
void DHT_task(void *pvParameter);
void mqttTask(void *pvParameter);

static void lv_ex_chart_1(void);
static void lv_ex_chart_2(void);

void lv_ex_label_1(void);
void lv_ex_label_2(void);
void lv_ex_label_3(void);
void lv_ex_label_4(void);
void lv_ex_label_5(void);
void lv_ex_label_6(void);
void lv_ex_label_7(void);

void update_chart1(void);
void update_chart2(void);

void lv_ex_textarea_1(void);
static void event_handler(lv_obj_t * obj, lv_event_t event);

char dataPublish[20];        

float humidityGet = 0.;
float temperatureGet = 0.;

lv_obj_t * label1;
lv_obj_t * label2;
lv_obj_t * label3;
lv_obj_t * label4;
lv_obj_t * label5;
lv_obj_t * label6;
lv_obj_t * label7;

lv_obj_t * chart1;
lv_chart_series_t * ser1chart1;

lv_obj_t * chart2;
lv_chart_series_t * ser1chart2;


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
    ESP_LOGD(TAG1, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG1, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "Temp 30 Humi 99", 0, 1, 0);
        ESP_LOGI(TAG1, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG1, "sent subscribe successful, msg_id=%d", msg_id);

        //msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        //ESP_LOGI(TAG1, "sent subscribe successful, msg_id=%d", msg_id);

        //msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        //ESP_LOGI(TAG1, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG1, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG1, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG1, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG1, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG1, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG1, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        sprintf(dataPublish,"DHT22 %.2f %.2f",temperatureGet,humidityGet);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", dataPublish, 0, 1, 0);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG1, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG1, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG1, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.uri, "FROM_STDIN") == 0) {
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
        mqtt_cfg.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG1, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}


/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {

    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
    xTaskCreatePinnedToCore(guiTask, "gui", 4096*2, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(DHT_task, "dht", 2048, NULL, 1, NULL, 1);
    //xTaskCreate(&DHT_task, "DHT_task", 2048, NULL, 1, NULL );
    //xTaskCreate(&mqttTask, "MQTT_task", 4096, NULL, 6, NULL ); 


    //ESP_LOGI(TAG1, "[APP] Startup..");
    //ESP_LOGI(TAG1, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    //ESP_LOGI(TAG1, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());
    mqtt_app_start();

}

void mqttTask(void *pvParameter) {

    ESP_LOGI(TAG1, "[APP] Startup..");
    ESP_LOGI(TAG1, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG1, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();

}

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;

static void guiTask(void *pvParameter) {

    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();

    lv_color_t* buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);

    /* Use double buffered when not working with monochrome displays */
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    lv_color_t* buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);
#else
    static lv_color_t *buf2 = NULL;
#endif

    static lv_disp_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;

#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820         \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A    \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D     \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306

    /* Actual size in pixels, not bytes. */
    size_in_px *= 8;
#endif

    /* Initialize the working buffer depending on the selected display.
     * NOTE: buf2 == NULL when using monochrome displays. */
    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;

    /* When using a monochrome display we need to register the callbacks:
     * - rounder_cb
     * - set_px_cb */
#ifdef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    disp_drv.rounder_cb = disp_driver_rounder;
    disp_drv.set_px_cb = disp_driver_set_px;
#endif

    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Register an input device when enabled on the menuconfig */
#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);
#endif

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));
  
    lv_ex_chart_1();
    lv_ex_chart_2();

    lv_ex_label_1();
    lv_ex_label_2();
    lv_ex_label_3();
    lv_ex_label_4();
    lv_ex_label_5();
    lv_ex_label_6();
    lv_ex_label_7();

    lv_ex_textarea_1();

    /* Create the demo application */
    //create_demo_application();
    //lv_ex_chart_1();
    

    while (1) {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */

        vTaskDelay(pdMS_TO_TICKS(10));
        update_chart1();
        update_chart2();
        char tempShow[10];
        char humiShow[10];        

        /* Update temprature and humidity to chart */
        sprintf(tempShow,"%.2f *C",temperatureGet);
        sprintf(humiShow,"%.2f %%",humidityGet);        
        lv_label_set_text(label6, tempShow);
        lv_label_set_text(label7, humiShow);

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
       }
    }

    /* A task should NEVER return */
    free(buf1);
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    free(buf2);
#endif
    vTaskDelete(NULL);
}

static void lv_ex_chart_1(void)
{
    /*Create a chart*/
    chart1 = lv_chart_create(lv_scr_act(), NULL);
    lv_obj_set_size(chart1, 200, 71);
    lv_obj_align(chart1, NULL, LV_ALIGN_CENTER, -1, -78);
    lv_chart_set_type(chart1, LV_CHART_TYPE_LINE);   /*Show lines and points too*/
    lv_chart_set_y_range(chart1, LV_CHART_AXIS_PRIMARY_Y,20,40);
    /*Add two data series*/
    ser1chart1 = lv_chart_add_series(chart1, LV_COLOR_RED);
 
    /*Set the next points on 'ser1'*/
    lv_chart_set_update_mode(chart1,LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_refresh(chart1); /*Required after direct set*/
}


static void lv_ex_chart_2(void)
{
    /*Create a chart*/
    chart2 = lv_chart_create(lv_scr_act(), NULL);
    lv_obj_set_size(chart2, 200, 70);
    lv_obj_align(chart2, NULL, LV_ALIGN_CENTER, 2, 18);
    lv_chart_set_type(chart2, LV_CHART_TYPE_LINE);   /*Show lines and points too*/
    lv_chart_set_y_range(chart2, LV_CHART_AXIS_PRIMARY_Y,60,100);

    /*Add two data series*/
    ser1chart2 = lv_chart_add_series(chart2, LV_COLOR_GREEN);
    lv_chart_set_update_mode(chart2,LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_refresh(chart2); /*Required after direct set*/
}

void update_chart1(void)
{
    //printf("Chart Update %.1f\n", temperatureGet);
    lv_chart_set_next(chart1, ser1chart1, temperatureGet);
    lv_chart_refresh(chart1); 

}

void update_chart2(void)
{
    //printf("Chart Update %.1f\n", humidityGet);
    lv_chart_set_next(chart2, ser1chart2, humidityGet);
    lv_chart_refresh(chart2); 

}

void lv_ex_label_1(void)
{
    label1 = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label1, LV_LABEL_LONG_EXPAND);     /*Break the long lines*/
    lv_label_set_recolor(label1, true);                      /*Enable re-coloring by commands in the text*/
    lv_label_set_align(label1, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
    lv_label_set_text(label1, "#0000ff READ SENSOR PROJECT");
    //lv_obj_set_width(label1, 150);
    lv_obj_set_height(label1, 14);
    lv_obj_align(label1, NULL, LV_ALIGN_CENTER, 0, -147);
}

void lv_ex_label_2(void)
{
    label2 = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label2, LV_LABEL_LONG_EXPAND);     /*Break the long lines*/
    lv_label_set_recolor(label2, true);                      /*Enable re-coloring by commands in the text*/
    lv_label_set_align(label2, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
    lv_label_set_text(label2, "#ff0000 Temprature");
    lv_obj_set_height(label2, 14);
    lv_obj_align(label2, NULL, LV_ALIGN_CENTER, -58, -127);
}

void lv_ex_label_3(void)
{
    label3 = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label3, LV_LABEL_LONG_EXPAND);     /*Break the long lines*/
    lv_label_set_recolor(label3, true);                      /*Enable re-coloring by commands in the text*/
    lv_label_set_align(label3, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
    lv_label_set_text(label3, "#008000 Humidity");
    lv_obj_set_height(label3, 14);
    lv_obj_align(label3, NULL, LV_ALIGN_CENTER, -63, -29);
}

void lv_ex_label_4(void)
{
    label4 = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label4, LV_LABEL_LONG_EXPAND);     /*Break the long lines*/
    lv_label_set_recolor(label4, true);                      /*Enable re-coloring by commands in the text*/
    lv_label_set_align(label4, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
    lv_label_set_text(label4, "#008000 COM");
    lv_obj_set_height(label4, 14);
    lv_obj_align(label4, NULL, LV_ALIGN_CENTER, -77, 69);
}

void lv_ex_label_5(void)
{
    label5 = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label5, LV_LABEL_LONG_EXPAND);     /*Break the long lines*/
    lv_label_set_recolor(label5, true);                      /*Enable re-coloring by commands in the text*/
    lv_label_set_align(label5, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
    lv_label_set_text(label5, "#ff0000 DISCONECTED");
    lv_obj_set_height(label5, 14);
    lv_obj_align(label5, NULL, LV_ALIGN_CENTER, 41, 69);
}

void lv_ex_label_6(void)
{
    label6 = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label6, LV_LABEL_LONG_EXPAND);     /*Break the long lines*/
    //lv_label_set_recolor(label6, true);                      /*Enable re-coloring by commands in the text*/
    lv_label_set_align(label6, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
    lv_label_set_text(label6, "Temp");
    lv_obj_set_height(label6, 14);
    lv_obj_align(label6, NULL, LV_ALIGN_CENTER, 63, -127);
}

void lv_ex_label_7(void)
{
    label7 = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label7, LV_LABEL_LONG_EXPAND);     /*Break the long lines*/
    //lv_label_set_recolor(label7, true);                      /*Enable re-coloring by commands in the text*/
    lv_label_set_align(label7, LV_LABEL_ALIGN_CENTER);       /*Center aligned lines*/
    lv_label_set_text(label7, "Humi");
    lv_obj_set_height(label7, 14);
    lv_obj_align(label7, NULL, LV_ALIGN_CENTER, 63, -29);
}

static void create_demo_application(void)
{
    /* When using a monochrome display we only show "Hello World" centered on the
     * screen */
#if defined CONFIG_LV_TFT_DISPLAY_MONOCHROME || \
    defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7735S

    /* use a pretty small demo for monochrome displays */
    /* Get the current screen  */
    lv_obj_t * scr = lv_disp_get_scr_act(NULL);

    /*Create a Label on the currently active screen*/
    lv_obj_t * label1 =  lv_label_create(scr, NULL);

    /*Modify the Label's text*/
    lv_label_set_text(label1, "Hello\nworld");

    /* Align the Label to the center
     * NULL means align on parent (which is the screen now)
     * 0, 0 at the end means an x, y offset after alignment*/
    lv_obj_align(label1, NULL, LV_ALIGN_CENTER, 0, 0);
#else
    /* Otherwise we show the selected demo */

    #if defined CONFIG_LV_USE_DEMO_WIDGETS
        lv_demo_widgets();
    #elif defined CONFIG_LV_USE_DEMO_KEYPAD_AND_ENCODER
        lv_demo_keypad_encoder();
    #elif defined CONFIG_LV_USE_DEMO_BENCHMARK
        lv_demo_benchmark();
    #elif defined CONFIG_LV_USE_DEMO_STRESS
        lv_demo_stress();
    #else
        #error "No demo application selected."
    #endif
#endif

}

static void event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_VALUE_CHANGED) {
        printf("Value: %s\n", lv_textarea_get_text(obj));
    }
    else if(event == LV_EVENT_LONG_PRESSED_REPEAT) {
        /*For simple test: Long press the Text are to add the text below*/
        const char  * txt = "\n\nYou can scroll it if the text is long enough.\n";
        static uint16_t i = 0;
        if(txt[i] != '\0') {
            lv_textarea_add_char(ta1, txt[i]);
            i++;
        }
    }
}

void lv_ex_textarea_1(void)
{
    ta1 = lv_textarea_create(lv_scr_act(), NULL);
    lv_obj_set_size(ta1, 210, 70);
    lv_obj_align(ta1, NULL, LV_ALIGN_CENTER, 2, 116);
    lv_textarea_set_text(ta1, "Data read fom PC");    /*Set an initial text*/
    lv_obj_set_event_cb(ta1, event_handler);
}

void DHT_task(void *pvParameter)
{
	setDHTgpio(22);
	printf( "Starting DHT Task\n\n");

	while(1) {
	
		printf("=== Reading DHT ===\n" );
		int ret = readDHT();
		
		errorHandler(ret);
        temperatureGet = getTemperature();
        humidityGet = getHumidity();
		//printf("Hum %.1f\n", humidityGet);
		//printf("Tmp %.1f\n", temperatureGet);
		
		// -- wait at least 2 sec before reading again ------------
		// The interval of whole process must be beyond 2 seconds !! 
		vTaskDelay( 3000 / portTICK_RATE_MS );
	}
}

static void lv_tick_task(void *arg) {
    (void) arg;

    lv_tick_inc(LV_TICK_PERIOD_MS);
}
