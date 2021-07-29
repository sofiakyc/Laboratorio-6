#include "system_tm4c1294.h" // CMSIS-Core
#include "driverleds.h" // device drivers
#include "cmsis_os2.h" // CMSIS-RTOS
#include "driverbuttons.h" // Projects/drivers
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "inc/hw_gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/pwm.h"
#include "inc/hw_types.h"
#include <stdint.h>

#define BUFFER_SIZE_ACIONADORA 8
#define BUFFER_SIZE_CONTROLADORA 8

typedef struct Led{
  int tempo;
  int led;
  int id;
}Led;

void initLed();

osThreadId_t controladora_id;
osThreadId_t acionadora_id[4];

osMessageQueueId_t filaControladora_id;
osMessageQueueId_t filaAcionadora_id[4];

Led led1, led2, led3, led4;

osSemaphoreId_t cheio_id, vazio_id;

int tickAnterior = 0;

osMutexId_t mutexLed;

typedef enum {selecionaLed=0, mudaIntensidade, nada} Mensagem;

void GPIOJ_Handler(void){
  uint32_t status=0;

  status = GPIOIntStatus(GPIO_PORTJ_BASE, true);
  GPIOIntClear(GPIO_PORTJ_BASE, status);
  
  int tick = osKernelGetTickCount();
  if(tick - tickAnterior >= 172) {
    Mensagem tipoMensagem;
    if( (status & GPIO_INT_PIN_0) == GPIO_INT_PIN_0){
      tipoMensagem = selecionaLed;
      osMessageQueuePut (filaControladora_id, &tipoMensagem, 0, 0);
    }
      
    if( (status & GPIO_INT_PIN_1) == GPIO_INT_PIN_1){
      tipoMensagem = mudaIntensidade;
      osMessageQueuePut (filaControladora_id, &tipoMensagem, 0, 0);
    }
    tickAnterior = tick;
  } 
}

void controladora(void *arg){
  Mensagem mensagem;
  Led ledAnterior;
  
  int selecionado = 0;
  ledAnterior.id = selecionado;
  ledAnterior.tempo = 0;
  while (1)
  {
    osMessageQueueGet (filaControladora_id, &mensagem, NULL, osWaitForever); //a mensagem pega eh armazenada em &mensagem
    
    if (mensagem == selecionaLed)
    {
      osMessageQueuePut (filaAcionadora_id[ledAnterior.id], &ledAnterior.tempo, 0, osWaitForever);
      ledAnterior.id = selecionado;
      if (selecionado == 0)
        ledAnterior.tempo = led1.tempo;
      if (selecionado == 1)
        ledAnterior.tempo = led2.tempo;
      if (selecionado == 2)
        ledAnterior.tempo = led3.tempo;
      if (selecionado == 3)
        ledAnterior.tempo = led4.tempo;
      
      if (selecionado < 3)
        selecionado = selecionado + 1;
      else
        selecionado = 0;
      
      int tempo = 0;
      
      osMessageQueuePut (filaAcionadora_id[selecionado], &tempo, 0, osWaitForever);
      mensagem = nada;
    }
    else if (mensagem == mudaIntensidade)
    {
      int tempo = 0;
      if (selecionado == 0)
        tempo = led1.tempo;
      if (selecionado == 1)
        tempo = led2.tempo;
      if (selecionado == 2)
        tempo = led3.tempo;
      if (selecionado == 3)
        tempo = led4.tempo;
      if (tempo <90)
        tempo = tempo+10;
      else
        tempo = 10;
      osMessageQueuePut (filaAcionadora_id[selecionado], &tempo, 0, osWaitForever);
      mensagem = nada;
    }
  }
}

void acionadora(void *arg){
  Led *led = (Led *)arg;

  while (1)
  {
    int tempo;
    osMessageQueueGet (filaAcionadora_id[led->id], &tempo, NULL, 0); //a mensagem pega eh armazenada em &mensagem
    led->tempo = tempo;
    if (led->tempo != 0)
    {
      osMutexAcquire(mutexLed, osWaitForever);
      LEDOn(led->led);
      osMutexRelease(mutexLed);
    }
    
    osDelayUntil(led->tempo);
    
    osMutexAcquire(mutexLed, osWaitForever);
    LEDOff(led->led);
    osMutexRelease(mutexLed);
    osDelayUntil(led->tempo);
  }
}

void main(void){
  SystemInit();
  
  initLed();
  const osMutexAttr_t Mutex_attr = {"piscaLed",osMutexRecursive | osMutexPrioInherit,NULL, 0};
  mutexLed = osMutexNew(&Mutex_attr);
    
  for(int i = 0; i < 1000000; i++);
  
  osKernelInitialize();

  controladora_id = osThreadNew(controladora, NULL, NULL);
  filaControladora_id = osMessageQueueNew (BUFFER_SIZE_CONTROLADORA, sizeof(int), NULL);
  
  led1.led = LED1;
  led1.tempo = 0;
  led1.id = 0;

  led2.led = LED2;
  led2.tempo = 10;
  led2.id = 1;  

  led3.led = LED3;
  led3.tempo = 10;
  led3.id = 2;

  led4.led = LED4;
  led4.tempo = 10;
  led4.id = 3; 
  
  acionadora_id[0] = osThreadNew(acionadora, &led1, NULL);
  acionadora_id[1] = osThreadNew(acionadora, &led2, NULL);
  acionadora_id[2] = osThreadNew(acionadora, &led3, NULL);
  acionadora_id[3] = osThreadNew(acionadora, &led4, NULL);
  
  filaAcionadora_id[0] = osMessageQueueNew (BUFFER_SIZE_ACIONADORA, sizeof(int), NULL);
  filaAcionadora_id[1] = osMessageQueueNew (BUFFER_SIZE_ACIONADORA, sizeof(int), NULL);
  filaAcionadora_id[2] = osMessageQueueNew (BUFFER_SIZE_ACIONADORA, sizeof(int), NULL);
  filaAcionadora_id[3] = osMessageQueueNew (BUFFER_SIZE_ACIONADORA, sizeof(int), NULL);
    
  vazio_id = osSemaphoreNew(BUFFER_SIZE_CONTROLADORA, BUFFER_SIZE_CONTROLADORA, NULL);
  cheio_id = osSemaphoreNew(BUFFER_SIZE_CONTROLADORA, 0, NULL);
  
  if(osKernelGetState() == osKernelReady)
    osKernelStart();

  while(1){
  }
} // main

void initLed()
{
  ButtonInit(USW1 | USW2);
  ButtonIntDisable(USW1 | USW2);
  ButtonIntEnable(USW1 | USW2);
  GPIOIntRegister(GPIO_PORTJ_BASE, GPIOJ_Handler);
  
  LEDInit(LED4 | LED3 | LED2 | LED1);
}