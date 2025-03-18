/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #include "hardware/rtc.h"
 #include "pico/util/datetime.h"
 #include "pico/stdlib.h"
 #include "hardware/gpio.h"
 #include "hardware/timer.h"
 #include <stdio.h>
 #include <string.h>
 
 const int trig_pin = 5;
 const int echo_pin = 15;
 
 typedef struct {
     alarm_id_t alarm_id;
     bool esperando_subida;
     absolute_time_t inicio;
     absolute_time_t fim;
     int descida;
 } sensor_state_t;
 
 // Encapsula o estado do sensor em uma única variável global estática
 volatile static sensor_state_t sensor_state = {0};
 
 static const uint TIMEOUT_MS = 50;
 
 int64_t alarme(alarm_id_t id, void *user_data) {
     if (sensor_state.esperando_subida) {
         datetime_t now;
         rtc_get_datetime(&now);
         // Formatação correta para exibir hora, minuto e segundo
         printf("%02d:%02d:%02d - Falha\n", now.hour, now.min, now.sec);
         sensor_state.esperando_subida = false;
     }
     return 0; // Não repete 
 }
 
 void echo_callback(uint gpio, uint32_t events) {
     if (events & GPIO_IRQ_EDGE_RISE) {
         sensor_state.inicio = get_absolute_time();
         if (sensor_state.esperando_subida) {
             cancel_alarm(sensor_state.alarm_id);
             sensor_state.esperando_subida = false;
         }
     } else if (events & GPIO_IRQ_EDGE_FALL) {
         sensor_state.fim = get_absolute_time();
         sensor_state.descida = 1;
     }
 }
 
 int main() {
     stdio_init_all();
     
     datetime_t t = {
         .year  = 2025,
         .month = 3,
         .day   = 18,
         .dotw  = 0,  // 0 = domingo
         .hour  = 0,
         .min   = 0,
         .sec   = 0
     };
     rtc_init();
     rtc_set_datetime(&t);
     
     gpio_init(trig_pin);
     gpio_set_dir(trig_pin, GPIO_OUT);
     
     gpio_init(echo_pin);
     gpio_set_dir(echo_pin, GPIO_IN);
     gpio_set_irq_enabled_with_callback(echo_pin,
                                        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                        true,
                                        echo_callback);
     
     int medindo = 0;
     
     while (true) {
         // Checa entrada do usuário (não bloqueante)
         int caracter = getchar_timeout_us(500);
         if (caracter == 's') {
             medindo = !medindo;
             if (medindo) {
                 printf("Medicoes iniciadas\n");
             } else {
                 printf("Medicoes pausadas\n");
             }
         }
         
         if (medindo) {
             // Dispara o pulso do sensor
             gpio_put(trig_pin, 1);
             sleep_us(10);
             gpio_put(trig_pin, 0);
             
             sensor_state.esperando_subida = true;
             sensor_state.alarm_id = add_alarm_in_ms(TIMEOUT_MS, alarme, NULL, false);
         
             if (sensor_state.descida == 1) {
                 sensor_state.descida = 0;
                 int diferenca = absolute_time_diff_us(sensor_state.inicio, sensor_state.fim);
                 double distancia = (diferenca * 0.0343) / 2.0;
                 datetime_t t2 = {0};
                 rtc_get_datetime(&t2);
         
                 char datetime_buf[256];
                 char *datetime_str = datetime_buf;
                 datetime_to_str(datetime_str, sizeof(datetime_buf), &t2);
                 printf("%s - %.2f cm\n", datetime_str, distancia);
             }
         }
         
         sleep_ms(1000);
     }
 }
 