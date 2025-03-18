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
 
 volatile absolute_time_t inicio;
 volatile absolute_time_t fim;
 
 volatile int descida = 0;
 
 static alarm_id_t alarm_id;             
 volatile bool esperando_subida = false;  
 static const uint TIMEOUT_MS = 50;       
 
 int64_t alarme(alarm_id_t id, void *user_data) {
     if (esperando_subida) {
         datetime_t now;
         rtc_get_datetime(&now);
         printf("%02d:%02d:%02d - Falha\n", now.hour, now.min, now.sec);
         esperando_subida = false;
     }
     return 0; // Não repete 
 }
 
 void echo_callback(uint gpio, uint32_t events) {
     if (events & GPIO_IRQ_EDGE_RISE) { 
         inicio = get_absolute_time();
         
         if (esperando_subida) {
             cancel_alarm(alarm_id);
             esperando_subida = false;
         }
     } else if (events & GPIO_IRQ_EDGE_FALL) {
         fim = get_absolute_time();
         descida = 1; 
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
             gpio_put(trig_pin, 1);
             sleep_us(10);
             gpio_put(trig_pin, 0);
             
             esperando_subida = true;
             alarm_id = add_alarm_in_ms(TIMEOUT_MS, alarme, NULL, false);
         
             if (descida == 1) {
                 descida = 0;
                 int diferenca = absolute_time_diff_us(inicio, fim);
                 double distancia = (diferenca * 0.0343) / 2.0;
                 datetime_t t2 = {0};
                 rtc_get_datetime(&t);
         
                 char datetime_buf[256];
                 char *datetime_str = &datetime_buf[0];
                 datetime_to_str(datetime_str, sizeof(datetime_buf), &t2);
                 // printf("Alarm Fired At %s\n", datetime_str);
                 rtc_get_datetime(&t2);
                 printf("%s - %.2f cm\n", datetime_str, distancia);
             }
         }
         
         sleep_ms(1000);
     }
 }
 