#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"          // needed for esp_timer_get_time()

#define NUM_OPTIONS 6


// Pin assignments
#define BUTTON_1_GPIO   GPIO_NUM_25
#define BUTTON_2_GPIO   GPIO_NUM_27

#define DEBOUNCE_US     20000   // 20 ms debounce window (in microseconds)

static const char *TAG = "button";
static QueueHandle_t button_evt_queue = NULL;

// Interrupt Service Routine
// Generic: it just forwards whichever pin fired it. One ISR serves both
// buttons because each one is registered with its own GPIO number as arg.
static void IRAM_ATTR button_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(button_evt_queue, &gpio_num, NULL);
}


// function for menu selection for count down timer
void menu_select (int arr[], int *set_sec){
    static int states = 0;
    int present_state = arr[0];
    *set_sec = 0;
    //switch case to cycle through different times
    const char* menu[NUM_OPTIONS] = {
        "10 seconds",
        "20 seconds",
        "30 seconds",
        "40 seconds",
        "50 seconds",
        "60 seconds"
    };
        
    states = (states + 1) % NUM_OPTIONS; // cycle through states
        

        // switch statment that is determined by the current states
        switch (present_state){
            case 1:
            printf("10 seconds\n");
            *set_sec = 10;
            break;

            case 2:
            printf("20 seconds\n");
            *set_sec = 20;
            break;

            case 3:
            printf("30 seconds\n");
            *set_sec = 30;
            break;

            case 4:
            printf("40 seconds\n");
            *set_sec = 40; 
            break;

            case 5:
            printf("50 seconds\n");
            *set_sec = 50;
            break;
            
            case 6:
            printf("60 seconds\n");
            *set_sec = 60;
            break;
        }
        

    
/*
for future itterations we want to use a while for cycling between menus 
this is because we want the user to go back to the first option instead of 
exiting the loop. the user will have more flexability that way in chooseing
the desired time.
*/

}






// handles the count down feature
void timer_func (int arr[]){
    int seconds = 0; // user selection
    printf("please enter time: ");
    menu_select(arr[1], &seconds);

    // saves the state of the button for start and stop
    int set_state = arr[0];

    printf("press to start timer"); 
    
    while (set_state == 1 && seconds > 0){
        int s = seconds %60; 

        // this will eventually be printed out to the display
        printf("\r%02d", s);
        fflush(stdout);
        clock_t stop = clock() + CLOCKS_PER_SEC;
        while (clock() < stop) {}
        seconds --;
    }

}










//Task: handles events outside the ISR
static void button_task(void *arg)
{
    uint32_t gpio_num;
    int64_t last_press[] = {0, 0};   // [0] = button 1, [1] = button 2

    while (1) {
        if (xQueueReceive(button_evt_queue, &gpio_num, portMAX_DELAY)) {
            int idx;
            const char *name;

            if (gpio_num == BUTTON_1_GPIO)      { idx = 0; name = "Button 1"; }
            //else if (gpio_num == BUTTON_2_GPIO) {idx = 1;                   }    // for final logic 
            else if (gpio_num == BUTTON_2_GPIO) { idx = 1; name = "Button 2"; }
            else continue;                       // unknown pin, ignore

            int64_t now = esp_timer_get_time();
            if (now - last_press[idx] < DEBOUNCE_US) {
                continue;                        // within debounce window, drop it
            }
            last_press[idx] = now;

            if (gpio_get_level(gpio_num) == 0) { // confirm still pressed (active-low)
                ESP_LOGI(TAG, "%s pressed", name);
            }
        }
    }
}

void app_main(void)
{
    // Configure both pins at once: input, internal pull-up, falling-edge interrupt.
    // Works in one struct because both buttons want identical settings.
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_1_GPIO) | (1ULL << BUTTON_2_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

    button_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);   // call ONCE for the whole program
    gpio_isr_handler_add(BUTTON_1_GPIO, button_isr_handler, (void *)BUTTON_1_GPIO);
    gpio_isr_handler_add(BUTTON_2_GPIO, button_isr_handler, (void *)BUTTON_2_GPIO);

    ESP_LOGI(TAG, "Button monitor started");
}









