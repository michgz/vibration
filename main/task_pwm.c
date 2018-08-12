#include "task_pwm.h"
#include "main.h"

#include <math.h>


#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"

#include "sdkconfig.h"

#include "esp_attr.h"
#include "soc/rtc.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"

#include "task_wifi.h"

const static char *TAG = "TASK_PWM";



const float SINE_128 [127];

// If =1: use RTOS timer to determine flash times
// If =0: use hardware timer to determine flash times.
//
const int FlashFromRtos = 0;

#define BLINK_GPIO 23   // the pin right next to GND.

#define TIMER_DIVIDER         2  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (0.001)   // sample test interval for the first timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload
#define TEST_CONTINUOUS       2



static void mcpwm_example_config(void);



float GetDuty(int y, float ampl, float freq_hz)
{
    float h = 0.0;

    float period = 1/freq_hz/((float) TIMER_INTERVAL0_SEC);

    int y_whole = (int) (((float) y) / period);

    float z = 512.0 * (((float) y) - (((float) y_whole) * period)) / period;

    int zz = (int) (z);

    if      (zz < 128) h =  100.0 * ampl * SINE_128[zz-0];
    else if (zz < 256) h =  100.0 * ampl * SINE_128[255-zz];
    else if (zz < 384) h = -100.0 * ampl * SINE_128[zz-256];
    else if (zz < 512) h = -100.0 * ampl * SINE_128[511-zz];
    else               h =  0.0;

    return h;
}

#define MAX_Y  (10*1000)

float TukeyWindow(int y)
{
    // Define a Tukey windowing function. Currently is approximated by a rectangular...

    if      (y < (int) (0.25* MAX_Y)) return 0.0;
    else if (y > (int) (0.75* MAX_Y)) return 0.0;
    else return 1.0;
}



/*
 * A sample structure to pass events
 * from the timer interrupt handler to the main program.
 */
typedef struct {
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;

xQueueHandle timer_queue;

/*
 * Timer group0 ISR handler
 *
 * Note:
 * We don't call the timer API here because they are not declared with IRAM_ATTR.
 * If we're okay with the timer irq not being serviced while SPI flash cache is disabled,
 * we can allocate this interrupt without the ESP_INTR_FLAG_IRAM flag and use the normal API.
 */
void IRAM_ATTR timer_group0_isr(void *para)
{
    int timer_idx = (int) para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    TIMERG0.hw_timer[timer_idx ^ 1].update = 1;
    uint64_t timer_counter_value = 
        ((uint64_t) TIMERG0.hw_timer[timer_idx ^ 1].cnt_high) << 32
        | TIMERG0.hw_timer[timer_idx ^ 1].cnt_low;

    /* Prepare basic event data
       that will be then sent back to the main program task */
    timer_event_t evt;
    evt.timer_group = 0;
    evt.timer_idx = timer_idx;
    evt.timer_counter_value = timer_counter_value;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_1) {
        TIMERG0.int_clr_timers.t1 = 1;
        //timer_counter_value += (uint64_t) (TIMER_INTERVAL0_SEC * TIMER_SCALE);
        //TIMERG0.hw_timer[timer_idx].alarm_high = (uint32_t) (timer_counter_value >> 32);
        //TIMERG0.hw_timer[timer_idx].alarm_low = (uint32_t) timer_counter_value;
    } else if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        //TIMERG0.int_clr_timers.t0 = 1;
    } else {
    }

    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

    portBASE_TYPE wokenHigherPriority;

    /* Now just send the event data back to the main program task */
    xQueueSendFromISR(timer_queue, &evt, &wokenHigherPriority);

    if (wokenHigherPriority)
    {
        portYIELD_FROM_ISR ();
    }
}


/*
 * Initialize selected timer of the timer group 0
 *
 * timer_idx - the timer number to initialize
 * auto_reload - should the timer auto reload on alarm?
 * timer_interval_sec - the interval of alarm to set
 */
static void example_tg0_timer_init(int timer_idx, 
    int auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = (auto_reload != TEST_CONTINUOUS) ? TIMER_ALARM_EN : 0;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = (auto_reload == TEST_WITH_RELOAD);
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    if (auto_reload != TEST_CONTINUOUS)
    {
        timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
        timer_enable_intr(TIMER_GROUP_0, timer_idx);
        timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr, 
            (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);
    }

    timer_start(TIMER_GROUP_0, timer_idx);
}

static void example_tg0_timer_deinit(int timer_idx)
{
    timer_pause(TIMER_GROUP_0, timer_idx);
}

/*
 * A simple helper function to print the raw timer counter value
 * and the counter value converted to seconds
 */
static void inline print_timer_counter(uint64_t counter_value)
{
    printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
                                    (uint32_t) (counter_value));
    printf("Time   : %.8f s\n", (double) counter_value / TIMER_SCALE);
}

typedef struct {
    float freq;
    float ampl;
} PWM_OPERATION_t;

static PWM_OPERATION_t pwmOp;

/*
 * The main task of the timer group
 */
static void do_a_pwm_operation(PWM_OPERATION_t *arg)
{
    static float dutyToSet = 0.0;

    static int x = 0;

    x = 0;

    float currentAmpl, currentFreq;
    
    if (arg == NULL)
    {
        currentAmpl = 1.000;
        currentFreq = 1.0;
    }
    else
    {
        currentAmpl = pow10f(arg->ampl / 20.0);  // dB to linear conversion
        currentFreq = arg->freq;
    }

    ESP_LOGI(TAG, "Vibration with frequency %f Hz, amplitude %f", currentFreq, currentAmpl);

    while (x < MAX_Y) {
        timer_event_t evt;
        xQueueReceive(timer_queue, &evt, portMAX_DELAY);

        if (evt.timer_idx == TIMER_1) {

            if (FlashFromRtos == 0) {
                if (dutyToSet >= 0.0) {
                    // Positive phase, place on A output
                    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, dutyToSet);
                    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 0.0);
                }
                else {
                    // Negative phase, place on B output
                    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0.0);
                    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, -dutyToSet);
                }
            }

            /* Print the timer values passed by event */
            //printf("------- EVENT TIME --------\n");
            //print_timer_counter(evt.timer_counter_value);

            /* Print the timer values as visible by this task */
            //printf("-------- TASK TIME --------\n");
            uint64_t task_counter_value;
            timer_get_counter_value(evt.timer_group, evt.timer_idx ^ 1, &task_counter_value);
            //print_timer_counter(task_counter_value);

            x += 1;

            // Calculate the duty for the next go-around
            dutyToSet = GetDuty(x, currentAmpl, currentFreq  ) * TukeyWindow(x);

        }
    }

}

void pwm_task(void *pvParameter)
{
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    
    mcpwm_example_config();
    
    do {

        example_tg0_timer_init(TIMER_0, TEST_CONTINUOUS,  0);
        example_tg0_timer_init(TIMER_1, TEST_WITH_RELOAD, TIMER_INTERVAL0_SEC);

        do_a_pwm_operation((PWM_OPERATION_t *) pvParameter);

        vTaskDelay(500 / portTICK_PERIOD_MS);

        example_tg0_timer_deinit(TIMER_0);
        example_tg0_timer_deinit(TIMER_1);

    } while(0);

    vTaskDelete(NULL);

}

#define GPIO_PWM0A_OUT 23
#define GPIO_PWM0B_OUT 22

static void mcpwm_example_gpio_initialize()
{
    //printf("initializing mcpwm gpio...\n");

    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_PWM0A_OUT);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, GPIO_PWM0B_OUT);
}

/**
 * @brief Configure whole MCPWM module
 */
static void mcpwm_example_config(void)
{
    //1. mcpwm gpio initialization
    mcpwm_example_gpio_initialize();

    //2. initialize mcpwm configuration
    //printf("Configuring Initial Parameters of mcpwm...\n");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 2000;    //frequency = 1000Hz
    pwm_config.cmpr_a = 0.0;       //duty cycle of PWMxA = 60.0%
    pwm_config.cmpr_b = 0.0;       //duty cycle of PWMxb = 50.0%
    pwm_config.counter_mode = MCPWM_UP_DOWN_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);   //Configure PWM0A & PWM0B with above settings
}


void app_main_do_pwm(OPERATION_t * op)
{
    ESP_LOGI(TAG, "Entering PWM task...");

    if (op != NULL)
    {
        pwmOp.freq = op->freq_used;
        pwmOp.ampl = op->ampl_used;
    }

    timer_queue = xQueueCreate(10, sizeof(timer_event_t));

    xTaskCreate(&pwm_task, "pwm_task", 2048, &pwmOp, 5, NULL);

}

void prepare_operation(OPERATION_t * op)
{
    if (op == NULL)
    {
        // BAD
    }
    else
    {
        if (op->freq_requested < 1.0)
        {
            op->freq_used = 1.0;
        }
        else if (op->freq_requested > 60.0)
        {
            op->freq_used = 60.0;
        }
        else
        {
            op->freq_used = op->freq_requested;
        }
        
        if (op->ampl_requested < -60.0)
        {
            op->ampl_used = -60.0;
        }
        else if (op->ampl_requested > -20.0)
        {
            op->ampl_used = -20.0;
        }
        else
        {
            op->ampl_used = op->ampl_requested;
        }

    }


}
