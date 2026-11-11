
#define F_CPU 16000000UL

/*--------------------Libraries---------------------------*/
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <avr/io.h>
#include <avr/sleep.h>

/* Scheduler include files. */
#include "AVR_FreeRTOS/FreeRTOS.h"
#include "AVR_FreeRTOS/task.h"
#include "AVR_FreeRTOS/queue.h"
#include "AVR_FreeRTOS/semphr.h"

/* system time include file. */
#include "time.h"

/*-----------------------------------------------------*/

#define TASK_C_MS     450U    /* CHALLENGE: change to 1400U */
#define TASK_STACK    256U    /* FreeRTOS stack depth (words, not bytes) */
#define ENABLE_UART_LOG 1    /* 0 for cleanest oscilloscope timing */
#define UART_BAUD     38400UL

#define LED_A          (1U << PB0)
#define LED_B          (1U << PB1)
#define LED_C          (1U << PB2)
#define ALL_LEDS       (LED_A | LED_B | LED_C)

typedef struct {
    uint8_t  pin_mask;
    char     name;
    uint16_t work_ms;
    uint16_t period_ms;
} TaskSpec;

static const TaskSpec specA = { LED_A, 'A', 150U,  750U };
static const TaskSpec specB = { LED_B, 'B', 300U, 1200U };
static const TaskSpec specC = { LED_C, 'C', TASK_C_MS, 1500U };

/* Avoid 16-bit overflow in some versions of pdMS_TO_TICKS() on AVR.
 * Round to the NEAREST whole RTOS tick. For 16 ms ticks, output will
 * be quantized; oscillator and WDT accuracy also affect wall time.
 */
static TickType_t ms_to_ticks(uint16_t ms)
{
    uint32_t n = (uint32_t)ms * (uint32_t)configTICK_RATE_HZ;
    n = (n + 500UL) / 1000UL;
    if (n == 0UL && ms != 0U) n = 1UL;
    return (TickType_t)n;
}

/* AVR FreeRTOS often uses a 16-bit TickType_t. Compare deadlines
 * using a signed difference so normal tick rollover still works
 * for intervals shorter than half a 16-bit tick-count range.
 */
static uint8_t passed_deadline(TickType_t finished, TickType_t deadline)
{
    return ((int16_t)((uint16_t)finished - (uint16_t)deadline) > 0);
}

#if ENABLE_UART_LOG
static void uart_init(void)
{
    /* USART0: 8N1, TX only. PD1 is USART0 TX on ATmega328PB. */
    const uint16_t ubrr = (uint16_t)((F_CPU / (8UL * UART_BAUD)) - 1UL);
    UCSR0A = (uint8_t)(1U << U2X0);
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0B = (uint8_t)(1U << TXEN0);
    UCSR0C = (uint8_t)((1U << UCSZ01) | (1U << UCSZ00));
}

static void uart_char(char c)
{
    while (!(UCSR0A & (1U << UDRE0))) { }
    UDR0 = (uint8_t)c;
}

static void uart_str(const char *s)
{
    while (*s != '\0') uart_char(*s++);
}

static void uart_uint(uint16_t value)
{
    char buf[5];
    uint8_t i = 0U;
    do {
        buf[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U && i < sizeof(buf));
    while (i > 0U) uart_char(buf[--i]);
}

/* Print once per completed job, AFTER the LED goes LOW.
 * Minimal synchronous diagnostic output; disable for final measurements.
 */
static void log_job(char task_name, TickType_t finished, uint8_t missed)
{
    uart_char(task_name);
    uart_str(missed ? " MISS t=" : " OK t=");
    uart_uint((uint16_t)finished);
    uart_str(" ticks\r\n");
}
#else
#define uart_init() ((void)0)
#define uart_str(x) ((void)0)
#define uart_uint(x) ((void)0)
#define log_job(a,b,c) ((void)0)
#endif


static void vLedTask(void *pvParameters)
{
    const TaskSpec *spec = (const TaskSpec *)pvParameters;
    const TickType_t period = ms_to_ticks(spec->period_ms);
    const TickType_t work_ticks = ms_to_ticks(spec->work_ms);

    /* All tasks use the same initial release origin (tick zero),
     * even if a lower-priority task starts later than another task.
     */
    TickType_t last_wake = (TickType_t)0;

    for (;;) {
        TickType_t deadline = (TickType_t)(last_wake + period);
        /* Atomic GPIO read-modify-write: task switches cannot cause
         * another LED's bit to be accidentally overwritten.
         */
        taskENTER_CRITICAL();
        PORTB |= spec->pin_mask;
        taskEXIT_CRITICAL();

        /* This DELAYS the task, freeing the CPU for other tasks.
         * Several LEDs can be high at once; LED high is NOT CPU load.
         */
        vTaskDelay(work_ticks);

        taskENTER_CRITICAL();
        PORTB &= (uint8_t)~spec->pin_mask;
        taskEXIT_CRITICAL();

        TickType_t finished = xTaskGetTickCount();
        uint8_t missed = passed_deadline(finished, deadline);
        log_job(spec->name, finished, missed);

        /* Periodic RELEASES, not a relative delay from job completion.
         * If execution overruns, this may return without sleeping.
         */
        vTaskDelayUntil(&last_wake, period);
    }
}

int main(void)
{
    DDRB |= (uint8_t)ALL_LEDS;
    PORTB &= (uint8_t)~ALL_LEDS;
    uart_init();


#if ENABLE_UART_LOG
    uart_str("ESE5190 B1 RMS\r\nTick Hz: ");
    uart_uint((uint16_t)configTICK_RATE_HZ);
    uart_str("\r\nC work: ");
    uart_uint((uint16_t)TASK_C_MS);
    uart_str(" ms\r\n");
#endif

    /* RMS: shorter period -> higher FreeRTOS priority. */
    BaseType_t ok_a = xTaskCreate(vLedTask, "A", TASK_STACK,
                                  (void *)&specA, 3U, NULL);
    BaseType_t ok_b = xTaskCreate(vLedTask, "B", TASK_STACK,
                                  (void *)&specB, 2U, NULL);
    BaseType_t ok_c = xTaskCreate(vLedTask, "C", TASK_STACK,
                                  (void *)&specC, 1U, NULL);

    if (ok_a != pdPASS || ok_b != pdPASS || ok_c != pdPASS) {
        uart_str("xTaskCreate FAILED (check FreeRTOS heap!)\r\n");
        for (;;) { /* error; leave LEDs off */ }
    }

    vTaskStartScheduler();
    uart_str("Scheduler FAILED (check heap / port)\r\n");
    for (;;) { }
}
