// FreeRTOS system call/mutex stress test

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#define DELAY 1
#define BUSY_MALLOC 0
#define BUSY_LOOP   1
#define SERIAL_DEBUG Serial1
#define STACK_SIZE 512
#define CORE_0 (1 << 0)
#define CORE_1 (1 << 1)

void semphrTakeConditional(bool useMutexOn, SemaphoreHandle_t xSemaphore);
void semphrGiveConditional(bool useMutexOn, SemaphoreHandle_t xSemaphore);

/*
  I want to keep the possibility of using different and independent
  mutexes for each of the functions that operate on the heap.

  If you want to enable the use of these mutexes remove the defines
  on lines 30 and 31 and enable the their initialization in setup()
*/
SemaphoreHandle_t xSemaphoreMalloc = NULL;
// SemaphoreHandle_t xSemaphoreRealloc = NULL;
// SemaphoreHandle_t xSemaphoreFree = NULL;

/*
  A lazy way to use the same mutex for malloc, realloc and free
  in order to bring us back to the same situation as the MCVE
  posted here: https://github.com/earlephilhower/arduino-pico/issues/795#issuecomment-1227122082
*/
#define xSemaphoreRealloc xSemaphoreMalloc
#define xSemaphoreFree xSemaphoreMalloc

const bool useMutexOnMalloc = false;
const bool useMutexOnRealloc = false;
const bool useMutexOnFree = false;

/*
  Enabling this, a realloc will be performed and the string "_realloc"
  will be concateneted to *tmp
*/
const bool tryRealloc = true;

TaskHandle_t loop2Handle = NULL;
TaskHandle_t loop3Handle = NULL;
TaskHandle_t loop4Handle = NULL;
TaskHandle_t loop5Handle = NULL;
TaskHandle_t loop6Handle = NULL;
TaskHandle_t loop7Handle = NULL;

void loop2(void *pvPramaters);
void loop3(void *pvPramaters);
void loop4(void *pvPramaters);
void loop5(void *pvPramaters);
void loop6(void *pvPramaters);
void loop7(void *pvPramaters);

void busyLoop();

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  vTaskDelay(2000); // Just a wait for serial so I don't miss the first lines.

  xSemaphoreMalloc = xSemaphoreCreateMutex();
  // xSemaphoreRealloc = xSemaphoreCreateMutex();
  // xSemaphoreFree = xSemaphoreCreateMutex();

  xTaskCreate(loop2, "loop2", STACK_SIZE, NULL, 1, &loop2Handle);
  vTaskCoreAffinitySet(loop2Handle, CORE_0);
  xTaskCreate(loop3, "loop3", STACK_SIZE, NULL, 1, &loop3Handle);
  vTaskCoreAffinitySet(loop3Handle, CORE_1);
  xTaskCreate(loop4, "loop4", STACK_SIZE, NULL, 1, &loop4Handle);
  vTaskCoreAffinitySet(loop4Handle, CORE_0);
  xTaskCreate(loop5, "loop5", STACK_SIZE, NULL, 1, &loop5Handle);
  vTaskCoreAffinitySet(loop5Handle, CORE_1);
   xTaskCreate(loop6, "loop6", STACK_SIZE, NULL, 1, &loop6Handle);
   vTaskCoreAffinitySet(loop6Handle, CORE_0);
   xTaskCreate(loop7, "loop7", STACK_SIZE, NULL, 1, &loop7Handle);
   vTaskCoreAffinitySet(loop7Handle, CORE_1);
}
static int _loop0[8];
static int _loop1[8];

void loop()
{
  vTaskDelay(1000);
  while (1)
  {
    rp2040.cpuid() == 0 ? _loop0[0]++ : _loop1[0]++;
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    for (int i=0; i<8; i++) Serial.printf("%d,%d ", _loop0[i],_loop1[i]);
    Serial.println("");

    if (_loop0[0] >= 10 && _loop0[0] % 5 == 0) {
        // This causes loop 2 to switch it's affinity every 10 seconds. This is the event to trigger the bug.
        vTaskCoreAffinitySet(loop2Handle, _loop0[0] % 10 == 0 ? CORE_1: CORE_0);
    }
  }
}

void loop1()
{
  vTaskDelay(1000);
  while (1)
  {
    //_loop[1]++;
    rp2040.cpuid() == 0 ? _loop0[1]++ : _loop1[1]++;
    char *tmp;

    semphrTakeConditional(useMutexOnMalloc, xSemaphoreMalloc);
    tmp = (char *)malloc(10 * sizeof(char));
    semphrGiveConditional(useMutexOnMalloc, xSemaphoreMalloc);

    strcpy(tmp, "foo");

    if (tryRealloc)
    {
      semphrTakeConditional(useMutexOnRealloc, xSemaphoreRealloc);
      tmp = (char *)realloc(tmp, 20 * sizeof(char));
      semphrGiveConditional(useMutexOnRealloc, xSemaphoreRealloc);
      strcat(tmp, "_realloc");
    }

    semphrTakeConditional(useMutexOnFree, xSemaphoreFree);
    free(tmp);
    semphrGiveConditional(useMutexOnFree, xSemaphoreFree);

    delay(DELAY);
  }
}

void loop2(void *pvPramaters)
{
  vTaskDelay(1000);
  (void) pvPramaters;
  while (1)
  {
    //_loop[2]++;
    rp2040.cpuid() == 0 ? _loop0[2]++ : _loop1[2]++;
    
    char *tmp;

    semphrTakeConditional(useMutexOnMalloc, xSemaphoreMalloc);
#if BUSY_MALLOC == 1
    tmp = (char *)malloc(10 * sizeof(char));
#endif
#if BUSY_LOOP == 1
    busyLoop();
#endif
    semphrGiveConditional(useMutexOnMalloc, xSemaphoreMalloc);

#if BUSY_MALLOC == 1
    strcpy(tmp, "bar");
#endif
#if BUSY_LOOP == 1
    busyLoop();
#endif
    
    if (tryRealloc)
    {
      semphrTakeConditional(useMutexOnRealloc, xSemaphoreRealloc);
#if BUSY_MALLOC == 1
      tmp = (char *)realloc(tmp, 20 * sizeof(char));
#endif
#if BUSY_LOOP == 1
    busyLoop();
#endif
      semphrGiveConditional(useMutexOnRealloc, xSemaphoreRealloc);
#if BUSY_MALLOC == 1
      strcat(tmp, "_realloc");
#endif
#if BUSY_LOOP == 1
    busyLoop();
#endif
    }
    
    semphrTakeConditional(useMutexOnFree, xSemaphoreFree);
#if BUSY_MALLOC == 1
    free(tmp);
#endif
#if BUSY_LOOP == 1
    busyLoop();
#endif
    semphrGiveConditional(useMutexOnFree, xSemaphoreFree); 
    
    delay(DELAY);
    
  }
}

void loop3(void *pvPramaters)
{
  vTaskDelay(1000);
  (void) pvPramaters;
  while (1)
  {
    //_loop[3]++;
    rp2040.cpuid() == 0 ? _loop0[3]++ : _loop1[3]++;

    char *tmp;

    semphrTakeConditional(useMutexOnMalloc, xSemaphoreMalloc);
    tmp = (char *)malloc(10 * sizeof(char));
    semphrGiveConditional(useMutexOnMalloc, xSemaphoreMalloc);

    strcpy(tmp, "yeah");

    if (tryRealloc)
    {
      semphrTakeConditional(useMutexOnRealloc, xSemaphoreRealloc);
      tmp = (char *)realloc(tmp, 20 * sizeof(char));
      semphrGiveConditional(useMutexOnRealloc, xSemaphoreRealloc);
      strcat(tmp, "_realloc");
    }

    semphrTakeConditional(useMutexOnFree, xSemaphoreFree);
    free(tmp);
    semphrGiveConditional(useMutexOnFree, xSemaphoreFree);

    delay(DELAY);
  }
}

void loop4(void *pvPramaters)
{
  vTaskDelay(1000);
  (void) pvPramaters;
  while (1)
  {
    //_loop[4]++;
    rp2040.cpuid() == 0 ? _loop0[4]++ : _loop1[4]++;

    char *tmp;

    semphrTakeConditional(useMutexOnMalloc, xSemaphoreMalloc);
    tmp = (char *)malloc(10 * sizeof(char));
    semphrGiveConditional(useMutexOnMalloc, xSemaphoreMalloc);

    strcpy(tmp, "baz");

    if (tryRealloc)
    {
      semphrTakeConditional(useMutexOnRealloc, xSemaphoreRealloc);
      tmp = (char *)realloc(tmp, 20 * sizeof(char));
      semphrGiveConditional(useMutexOnRealloc, xSemaphoreRealloc);
      strcat(tmp, "_realloc");
    }

    semphrTakeConditional(useMutexOnFree, xSemaphoreFree);
    free(tmp);
    semphrGiveConditional(useMutexOnFree, xSemaphoreFree);

    delay(DELAY);
  }
}

void loop5(void *pvPramaters)
{
  vTaskDelay(1000);
  (void) pvPramaters;
  while (1)
  {
    //_loop[5]++;
    rp2040.cpuid() == 0 ? _loop0[5]++ : _loop1[5]++;

    char *tmp;

    semphrTakeConditional(useMutexOnMalloc, xSemaphoreMalloc);
    tmp = (char *)malloc(10 * sizeof(char));
    semphrGiveConditional(useMutexOnMalloc, xSemaphoreMalloc);

    strcpy(tmp, "asd");

    if (tryRealloc)
    {
      semphrTakeConditional(useMutexOnRealloc, xSemaphoreRealloc);
      tmp = (char *)realloc(tmp, 20 * sizeof(char));
      semphrGiveConditional(useMutexOnRealloc, xSemaphoreRealloc);
      strcat(tmp, "_realloc");
    }

    semphrTakeConditional(useMutexOnFree, xSemaphoreFree);
    free(tmp);
    semphrGiveConditional(useMutexOnFree, xSemaphoreFree);

    delay(DELAY);
  }
}

void loop6(void *pvPramaters)
{
  vTaskDelay(1000);
  (void) pvPramaters;
  while (1)
  {
    //_loop[6]++;
    rp2040.cpuid() == 0 ? _loop0[6]++ : _loop1[6]++;

    char *tmp;

    semphrTakeConditional(useMutexOnMalloc, xSemaphoreMalloc);
    tmp = (char *)malloc(10 * sizeof(char));
    semphrGiveConditional(useMutexOnMalloc, xSemaphoreMalloc);

    strcpy(tmp, "lol");

    if (tryRealloc)
    {
      semphrTakeConditional(useMutexOnRealloc, xSemaphoreRealloc);
      tmp = (char *)realloc(tmp, 20 * sizeof(char));
      semphrGiveConditional(useMutexOnRealloc, xSemaphoreRealloc);
      strcat(tmp, "_realloc");
    }

    semphrTakeConditional(useMutexOnFree, xSemaphoreFree);
    free(tmp);
    semphrGiveConditional(useMutexOnFree, xSemaphoreFree);

    delay(DELAY);
  }
}

void loop7(void *pvPramaters)
{
  vTaskDelay(1000);
  (void) pvPramaters;
  while (1)
  {
    //_loop[7]++;
    rp2040.cpuid() == 0 ? _loop0[7]++ : _loop1[7]++;

    char *tmp;

    semphrTakeConditional(useMutexOnMalloc, xSemaphoreMalloc);
    tmp = (char *)malloc(10 * sizeof(char));
    semphrGiveConditional(useMutexOnMalloc, xSemaphoreMalloc);

    strcpy(tmp, "yay");

    if (tryRealloc)
    {
      semphrTakeConditional(useMutexOnRealloc, xSemaphoreRealloc);
      tmp = (char *)realloc(tmp, 20 * sizeof(char));
      semphrGiveConditional(useMutexOnRealloc, xSemaphoreRealloc);
      strcat(tmp, "_realloc");
    }

    semphrTakeConditional(useMutexOnFree, xSemaphoreFree);
    free(tmp);
    semphrGiveConditional(useMutexOnFree, xSemaphoreFree);

    delay(DELAY);
  }
}

void semphrTakeConditional(bool useMutexOn, SemaphoreHandle_t xSemaphore)
{
  if (useMutexOn)
  {
    xSemaphoreTake(xSemaphore, TickType_t(portMAX_DELAY));
  }
}

void semphrGiveConditional(bool useMutexOn, SemaphoreHandle_t xSemaphore)
{
  if (useMutexOn)
  {
    xSemaphoreGive(xSemaphore);
  }
}

void busyLoop() {
  int i;
  float j = 1.5f;
  for (i = 0; i++; i < 1000) {
    j = j * 2.0f;
  }
}
