#include <stdio.h>
#include "driver/ledc.h"
#include "driver/gpio.h"

/*
motor 1
Pino BTS7960	Pino ESP32 (do código)	Notas
RPWM	GPIO 25	Entrada de velocidade Sentido 1
LPWM	GPIO 26	Entrada de velocidade Sentido 2
R_EN	GPIO 27	Habilita o lado direito
L_EN	GPIO 27	Habilita o lado esquerdo


motor 2
Pino BTS7960,Pino ESP32 (do código),Notas
RPWM,GPIO 13,Entrada de velocidade Sentido 1
LPWM,GPIO 12,Entrada de velocidade Sentido 2
R_EN,GPIO 14,Habilita o lado direito
L_EN,GPIO 14,Habilita o lado esquerdo

*/


// --- Configurações do PWM (LEDC) ---
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_FREQUENCY          (20000)       // 20 KHz
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT // 10 bits de resolução (0-1023)
#define MAX_DUTY                ((1 << LEDC_DUTY_RES) - 1) // 1023

// --- Pinos de Controle e Canais LEDC ---
// Motor 1 (Ex: Direita)
#define M1_REN_PIN              GPIO_NUM_32
#define M1_LEN_PIN              GPIO_NUM_33
#define M1_RPWM_PIN             GPIO_NUM_25
#define M1_LPWM_PIN             GPIO_NUM_26
#define M1_RPWM_CHANNEL         LEDC_CHANNEL_0
#define M1_LPWM_CHANNEL         LEDC_CHANNEL_1

// Motor 2 (Ex: Esquerda)
#define M2_REN_PIN              GPIO_NUM_27
#define M2_LEN_PIN              GPIO_NUM_14
#define M2_RPWM_PIN             GPIO_NUM_13
#define M2_LPWM_PIN             GPIO_NUM_12
#define M2_RPWM_CHANNEL         LEDC_CHANNEL_2
#define M2_LPWM_CHANNEL         LEDC_CHANNEL_3


#ifdef __cplusplus
extern "C" {
#endif

void ledc_init(void);
void motor_control(ledc_channel_t rpwm_ch, ledc_channel_t lpwm_ch, gpio_num_t ren_pin, gpio_num_t len_pin, int direction, int duty);

#ifdef __cplusplus
}
#endif