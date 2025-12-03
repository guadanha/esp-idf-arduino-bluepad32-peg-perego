#include "bts7960.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"



void ledc_init(void) {
    // 1. Configuração do Temporizador
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 2. Configuração dos Canais (4 canais para 2 motores: RPWM e LPWM para cada)
    ledc_channel_config_t ledc_channel[] = {
        { .gpio_num = M1_RPWM_PIN, .speed_mode = LEDC_MODE, .channel = M1_RPWM_CHANNEL, .intr_type = LEDC_INTR_DISABLE, .timer_sel = LEDC_TIMER, .duty = 0, .hpoint = 0 },
        { .gpio_num = M1_LPWM_PIN, .speed_mode = LEDC_MODE, .channel = M1_LPWM_CHANNEL, .intr_type = LEDC_INTR_DISABLE, .timer_sel = LEDC_TIMER, .duty = 0, .hpoint = 0 },
        { .gpio_num = M2_RPWM_PIN, .speed_mode = LEDC_MODE, .channel = M2_RPWM_CHANNEL, .intr_type = LEDC_INTR_DISABLE, .timer_sel = LEDC_TIMER, .duty = 0, .hpoint = 0 },
        { .gpio_num = M2_LPWM_PIN, .speed_mode = LEDC_MODE, .channel = M2_LPWM_CHANNEL, .intr_type = LEDC_INTR_DISABLE, .timer_sel = LEDC_TIMER, .duty = 0, .hpoint = 0 },
    };

    for (int i = 0; i < sizeof(ledc_channel) / sizeof(ledc_channel_config_t); i++) {
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel[i]));
    }

    // 3. Configuração dos pinos EN (como GPIO simples)
    gpio_set_direction(M1_REN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(M2_REN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(M1_LEN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(M2_LEN_PIN, GPIO_MODE_OUTPUT);
}

// =========================================================

/**
 * @brief Controla a direção, velocidade e freio de um motor.
 */
void motor_control(ledc_channel_t rpwm_ch, ledc_channel_t lpwm_ch, gpio_num_t ren_pin, gpio_num_t len_pin, int direction, int duty_) {
    static int duty = 0;
    if (duty_ == 0) {
        duty = 0;
    } else if (duty > duty_) {
        duty = duty_;
    } else if (duty <= (duty_ + 50)) {
        duty = duty + 50;
    } else {
        duty = duty_;
    }
    // Habilita o driver (necessário para girar e para freio ativo)
    if (direction == 1) { // FRENTE
        ledc_set_duty(LEDC_MODE, rpwm_ch, duty);
        ledc_update_duty(LEDC_MODE, rpwm_ch);
        gpio_set_level(ren_pin, 1);
        ledc_set_duty(LEDC_MODE, lpwm_ch, 0); // Desliga o outro lado
        ledc_update_duty(LEDC_MODE, lpwm_ch);
        gpio_set_level(len_pin, 0);
    } else if (direction == -1) { // RÉ
        ledc_set_duty(LEDC_MODE, rpwm_ch, 0); // Desliga o outro lado
        ledc_update_duty(LEDC_MODE, rpwm_ch);
        gpio_set_level(ren_pin, 0);
        ledc_set_duty(LEDC_MODE, lpwm_ch, duty);
        ledc_update_duty(LEDC_MODE, lpwm_ch);
        gpio_set_level(len_pin, 1);
    } else if (direction == 0) { // 0: FREIO ATIVO (Curto-circuito em GND: Duty 0 em ambos os pinos)
        ledc_set_duty(LEDC_MODE, rpwm_ch, 0);
        ledc_update_duty(LEDC_MODE, rpwm_ch);
        gpio_set_level(ren_pin, 1);
        ledc_set_duty(LEDC_MODE, lpwm_ch, 0);
        ledc_update_duty(LEDC_MODE, lpwm_ch);
        gpio_set_level(len_pin, 1);
    } else {
        ledc_set_duty(LEDC_MODE, rpwm_ch, 0);
        ledc_update_duty(LEDC_MODE, rpwm_ch);
        gpio_set_level(ren_pin, 0);
        ledc_set_duty(LEDC_MODE, lpwm_ch, 0);
        ledc_update_duty(LEDC_MODE, lpwm_ch);
        gpio_set_level(len_pin, 0);
    }
}