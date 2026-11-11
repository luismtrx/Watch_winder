#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"          // needed for esp_timer_get_time()

#define NUM_OPTIONS 6


// Pin assignments
#define BUTTON_1_GPIO   GPIO_NUM_25         // GPIO pin for button 1
#define BUTTON_2_GPIO   GPIO_NUM_27         // GPIO pin for button 2    
#define ENABLE_PIN      GPIO_NUM_18         // EN1: driven by LEDC so it can do on/off and PWM
#define DIRA_PIN        GPIO_NUM_19         // IN1
#define DIRB_PIN        GPIO_NUM_5          // IN2

// LEDC channel timer (frequency and resolution) configuration
#define PWM_MODE         LEDC_LOW_SPEED_MODE
#define PWM_TIMER        LEDC_TIMER_0
#define PWM_CHANNEL      LEDC_CHANNEL_0     // 8 bit - duty cycle resolution (0-255)
#define PWM_RESOLUTION    LEDC_TIMER_8_BIT
#define PWM_FREQUENCY_HZ    1000            // 1 kHz PWM frequency but suitable for change if we want to see how it affects the motor speed

#define DUTY_MAX         255                 // maximum duty cycle for 8-bit resolution is 255 but we dont need it that fast
#define DUTY_OFF         0                   // duty cycle for motor off



#define DEBOUNCE_US     20000   // 20 ms debounce window (in microseconds)

static const char *TAG = "button";
static const char *TAG2 = "motor";
static QueueHandle_t button_evt_queue = NULL;


static void motor_set_enable(uint32_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(PWM_MODE, PWM_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(PWM_MODE, PWM_CHANNEL));
}


// sets the direction of the motors 
static void motor_set_direction(int dir_a, int dir_b)
{
    ESP_ERROR_CHECK(gpio_set_level(DIRA_PIN, dir_a));
    ESP_ERROR_CHECK(gpio_set_level(DIRB_PIN, dir_b));
}


// blocking delay in ms
static inline void delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}


static void motor_init(void)
{
    // configure the direction pins as outputs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DIRA_PIN) | (1ULL << DIRB_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    motor_set_direction(0, 0);              // start both motors low so the motor is idle
    
    
    // configure the LEDC timer for PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = PWM_MODE,
        .timer_num        = PWM_TIMER,
        .duty_resolution  = PWM_RESOLUTION,  
        .freq_hz          = PWM_FREQUENCY_HZ,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t chan_conf = {
        .gpio_num       = ENABLE_PIN,
        .speed_mode     = PWM_MODE,
        .channel        = PWM_CHANNEL,
        .timer_sel      = PWM_TIMER,
        .duty           = DUTY_OFF,          // start with motor off
        .hpoint         = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chan_conf));
}


// Interrupt Service Routine
// Generic--> it just forwards whichever pin fired it. One ISR serves both
// buttons because each one is registered with its own GPIO number as arg.
static void IRAM_ATTR button_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(button_evt_queue, &gpio_num, NULL);
}


// function for menu selection for count down timer
void menu_select (int volatile arr[], int volatile *set_sec){
    static int states = 0;
    arr[0] = states; // static variable to hold the current state
    *set_sec = 0;


    /*
    place holder for when we use the display to show the current state of the menu selection
    //switch case to cycle through different times
    
    const char* menu[NUM_OPTIONS] = {
        "10 seconds",
        "20 seconds",
        "30 seconds",
        "40 seconds",
        "50 seconds",
        "60 seconds"
    };
      */
     
              

        // switch statment that is determined by the current states
        switch (states) {
            case 0:
            printf("10 seconds\n");
            *set_sec = 10;
            break;

            case 1:
            printf("20 seconds\n");
            *set_sec = 20;
            break;

            case 2:
            printf("30 seconds\n");
            *set_sec = 30;
            break;

            case 3:
            printf("40 seconds\n");
            *set_sec = 40; 
            break;

            case 4:
            printf("50 seconds\n");
            *set_sec = 50;
            break;
            
            case 5:
            printf("60 seconds\n");
            *set_sec = 60;
            break;
        }
        
    states = (states + 1) % NUM_OPTIONS; // cycle through states

    
/*
for future itterations we want to use a while for cycling between menus 
this is because we want the user to go back to the first option instead of 
exiting the loop. the user will have more flexability that way in chooseing
the desired time.
*/
}






/* handles the count down feature
    arr[0] --> holds the time selected by the user
    arr[1] --> holds the state of the button for start and stop
*/

void timer_func (int volatile arr[], int volatile seconds){
    int sec = seconds; // variable to hold the time selected by the user
    printf("Slected Time: %d seconds\n", sec);
    printf("Timer has Started\n"); 
    
    while (sec > 0){
        int s = sec %60; 

        if (arr[1] == 1){
            //printf("\r%02d", s);  // counts down on the same line and dosns't replace the replaced text
            printf("%02d\n", s);    
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(1000));// delay for 1 second
            sec --; 
        }else{
            vTaskDelay(pdMS_TO_TICKS(100)); // check again 
        }
    }
    printf("\r00\n");
    
}

static volatile int menu_data[2] = {0, 0};
static volatile int set_sec = 0; // variable to hold the time selected by the user  for test pruposes

void countdown_task (void *arg){
    while (1) {
        if (menu_data[1] == 1 && set_sec > 0) { // check if the timer is started
            timer_func(menu_data, set_sec);
            menu_data[1] = 0; // reset the state after the timer finishes
        }else {
        vTaskDelay(pdMS_TO_TICKS(50)); // check every 50 ms
    }
    }
}


static void motor_task(void *arg){
    while (1) {
        if (menu_data[1] == 1) { // check if the start button is pressed 
            motor_set_enable(DUTY_MAX); // turn on the motor at full speed
            motor_set_direction(1,0); // set the motor direction
        } else {
            motor_set_enable(DUTY_OFF); // turn off the motor
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // check every 50 ms
    }
}

//Task: handles events outside the ISR
static void button_task(void *arg){
    uint32_t gpio_num;
    int64_t last_press[] = {0, 0};   // [0] = button 1, [1] = button 2
  
    while (1) {
        if (xQueueReceive(button_evt_queue, &gpio_num, portMAX_DELAY)) {
            int idx;
            const char *name;

            if (gpio_num == BUTTON_1_GPIO)      { idx = 0; name = "Button 1"; }
            else if (gpio_num == BUTTON_2_GPIO) { idx = 1; name = "Button 2"; }
            else continue;                       // unknown pin, ignore

            int64_t now = esp_timer_get_time();
            if (now - last_press[idx] < DEBOUNCE_US) {
                continue;                        // within debounce window, drop it
            }
            last_press[idx] = now;

            if (gpio_get_level(gpio_num) == 0) { // confirm still pressed (active-low)
                ESP_LOGI(TAG, "%s pressed", name);

                // call the menu select function to cycle through the different time options
                if (idx == 0){
                    menu_select(menu_data, &set_sec);
                    ESP_LOGI(TAG, "index=%d seconds=%d",menu_data[0], set_sec);
                }
                else if (idx == 1){
                    if (set_sec > 0){
                        menu_data[1] = !menu_data[1]; // toggle the state of the button for start and stop
                        ESP_LOGI(TAG, "state=%d second=%d", menu_data[1], set_sec);
                        
                    }
            }
        }
    }
}
}

void app_main(void)
{
    motor_init();   // configure the motor pins and PWM
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

    // raised the stack size to 4096 to avoid stack overflow error
    xTaskCreate(button_task, "button_task", 4096, NULL, 10, NULL);
    xTaskCreate(countdown_task, "countdown_task", 4096, NULL, 5, NULL);
    xTaskCreate(motor_task, "motor_task", 4096, NULL, 5, NULL);

    gpio_install_isr_service(0);   // call ONCE for the whole program
    gpio_isr_handler_add(BUTTON_1_GPIO, button_isr_handler, (void *)BUTTON_1_GPIO);
    gpio_isr_handler_add(BUTTON_2_GPIO, button_isr_handler, (void *)BUTTON_2_GPIO);

    ESP_LOGI(TAG, "Button monitor started");
}









