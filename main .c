#include <stdio.h>
#include <stdlib.h>

//kernel header files
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

#define CCM_RAM __attribute__((section(".ccmram")))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

#define STACK_SIZE						1000
#define BUFFER_SIZE						25
#define QUEUE_SIZE					 	2

QueueHandle_t myQueue; //global queue
TimerHandle_t sender_1_Timer, sender_2_Timer, receiver_Timer; //three global timers to manage sleep operation of the three tasks
SemaphoreHandle_t sender_1_Semaphore = NULL, sender_2_Semaphore = NULL, receiver_Semaphore = NULL;

int sent_messages, blocked_messages, recieved_messages;
int Treceiver = 100, Tsender; // receiver and sender time periods in milliseconds

//for uniformly distributed random numbers
int LowerBounds[6] = {50, 80, 110, 140, 170, 200};
int UpperBounds[6] = {150, 200, 250, 300,350, 400};
//declaring the lower and higher bounds for the  500 sent messages
int upper,lower;



int uniform_distribution(int LowerBound, int UpperBound); // uniform distribution generator
void Reset(void);
void handle_timer_1_period(void);
void handle_timer_2_period(void);
void sender_1_Task(void *p);
void sender_2_Task(void *p);
void receiver_Task(void *p);
void sender_1_TimerCallback(TimerHandle_t xTimer);
void sender_2_TimerCallback(TimerHandle_t xTimer);
void receiver_TimerCallback(TimerHandle_t xTimer);
trace_initialize();


int main(int argc, char* argv[])
{


    myQueue = xQueueCreate( QUEUE_SIZE, BUFFER_SIZE * sizeof(char) );
	BaseType_t status;
	if (!myQueue)
	{
		trace_puts ("something went wrong, Queue could not be created!");
		exit(0);
	}

	/////////////////////////////////////////////////////////////////////////////////////
	status = xTaskCreate(sender_1_Task, "Sender1", STACK_SIZE, ( void * ) 0, tskIDLE_PRIORITY, &sender_1_Task);
	if (status != pdPASS)
	{
		trace_puts ("something went wrong, Sender1 task could not be created!");
		exit(0);
	}
	/////////////////////////////////////////////////////////////////////////////////////

	status = xTaskCreate(sender_2_Task, "Sender2", STACK_SIZE, ( void * ) 0, tskIDLE_PRIORITY, &sender_2_Task);
	if (status != pdPASS)
	{
		trace_puts ("something went wrong, Sender2 task could not be created!");
		exit(0);
	}
	/////////////////////////////////////////////////////////////////////////////////////

	status = xTaskCreate( receiver_Task, "Receiver", STACK_SIZE, ( void * ) 0, tskIDLE_PRIORITY + 1, &receiver_Task);
	if (status != pdPASS)
	{
		trace_puts ("something went wrong, Receiver task could not be created!");
		exit(0);
	}
	/////////////////////////////////////////////////////////////////////////////////////

    lower=LowerBounds[0];
    upper=UpperBounds[0];
    Tsender=uniform_distribution(lower,upper);

	sender_1_Timer = xTimerCreate("Sender1 Timer", pdMS_TO_TICKS(Tsender), pdTRUE, 0, sender_1_TimerCallback);
	if (!sender_1_Timer )
	{
		trace_puts ("something went wrong, Sender1 timer could not be created!");
		exit(0);
	}
	/////////////////////////////////////////////////////////////////////////////////////

	sender_2_Timer = xTimerCreate("Sender2 Timer", pdMS_TO_TICKS(Tsender), pdTRUE, 0, sender_2_TimerCallback);
	if (!sender_2_Timer)
	{
		trace_puts ("something went wrong, Sender2 timer could not be created!");
		exit(0);
	}
	/////////////////////////////////////////////////////////////////////////////////////

	receiver_Timer = xTimerCreate( "Receiver Timer", pdMS_TO_TICKS(Treceiver), pdTRUE, 0, receiver_TimerCallback);
	if (!receiver_Timer)
	{
		trace_puts ("something went wrong, Receiver timer could not be created!");
		exit(0);
	}
	/////////////////////////////////////////////////////////////////////////////////////

	Reset();
	/////////////////////////////////////////////////////////////////////////////////////


	status = xTimerStart(sender_1_Timer, 0);
	if (status != pdPASS)
	{
		trace_puts ("Sender1 timer could not be started!");
		exit(0);
	}
	/////////////////////////////////////////////////////////////////////////////////////

	status = xTimerStart(sender_2_Timer, 0);
	if (status != pdPASS)
	{
		trace_puts ("Sender2 timer could not be started!");
		exit(0);
	}
	/////////////////////////////////////////////////////////////////////////////////////

	status = xTimerStart(receiver_Timer, 0);
	if (status != pdPASS)
	{
		trace_puts ("Receiver timer could not be started!");
		exit(0);
	}
	/////////////////////////////////////////////////////////////////////////////////////

	vTaskStartScheduler();
}


///////////////////////////////////////////// function definitions ///////////////////////////////////////////////////////////

int uniform_distribution(int LowerBound, int UpperBound) {
    int range = UpperBound - LowerBound + 1;
    double random_number = rand()/(1.0 + RAND_MAX);
    int value = (random_number * range) + LowerBound;
    return value;
}

void Reset(void)
{
	static int position = 0; //to know where we are regarding the number of bounds previously defined above


	//if this is not the first time we run the program
	if (position){

		trace_printf("number of successfully sent Messages = %d\n", sent_messages);
		trace_printf("number of Blocked Messages = %d\n\n", blocked_messages);
		sent_messages = 0;
					blocked_messages = 0;
					recieved_messages = 0;

	}

	xQueueReset(myQueue);


	if(position <= (sizeof(UpperBounds)/sizeof(UpperBounds[0])-1))
	{
        upper=UpperBounds[position];
        lower=LowerBounds[position];
        position++;

	}
	else{
        xTimerDelete(sender_1_Timer, 0);
        xTimerDelete(sender_2_Timer, 0);
		xTimerDelete(receiver_Timer, 0);
		trace_puts("Game Over!\n");
		exit(0);

	}
	return;
}

void handle_timer_1_period(void){
   // draw a random period from the uniform distribution
		Tsender=uniform_distribution(lower,upper);
		xTimerChangePeriod(sender_1_Timer, pdMS_TO_TICKS(Tsender) , 0);
}

void handle_timer_2_period(void){
   // draw a random period from the uniform distribution
		Tsender=uniform_distribution(lower,upper);
		xTimerChangePeriod(sender_2_Timer, pdMS_TO_TICKS(Tsender) , 0);
}

void sender_1_Task(void *p)
{
	char Message[BUFFER_SIZE];
	sender_1_Semaphore = xSemaphoreCreateBinary();
	BaseType_t status;

	while(1)
	{
		xSemaphoreTake( sender_1_Semaphore, portMAX_DELAY );
		sprintf(Message, "Time is %d", xTaskGetTickCount() );
		status = xQueueSend(myQueue, Message, 0);
		if ( status == pdPASS ){
			sent_messages++;
		}

		else{
			blocked_messages++;
		}


	}
}

void sender_2_Task(void *p)
{
	char Message[BUFFER_SIZE];
	sender_2_Semaphore = xSemaphoreCreateBinary();
	BaseType_t status;

	while(1)
	{
        xSemaphoreTake( sender_2_Semaphore, portMAX_DELAY );
		sprintf(Message, "Time is %d", xTaskGetTickCount() );
		status = xQueueSend(myQueue, Message, 0);
		if ( status == pdPASS )
		{
			sent_messages++;
		}

		else
		{
			blocked_messages++;
		}


	}
}


void receiver_Task(void *p)
{
	char recieved_message [BUFFER_SIZE];
	BaseType_t status;
	receiver_Semaphore = xSemaphoreCreateBinary();

	while(1)
	{
		xSemaphoreTake( receiver_Semaphore, portMAX_DELAY );
		status = xQueueReceive( myQueue, recieved_message, 0);
		if( status == pdPASS )
		{
			recieved_messages ++;
		}
	}
}


void sender_1_TimerCallback(TimerHandle_t xTimer)
{
	// Create a boolean xTaskWoken to check if releasing the semaphore allowed the sender task to be unblocked
	static BaseType_t xTask_awake = pdFALSE;
	xSemaphoreGiveFromISR( sender_1_Semaphore, &xTask_awake );
	handle_timer_1_period();
}

void sender_2_TimerCallback(TimerHandle_t xTimer)
{
	// Create a boolean xTaskWoken to check if releasing the semaphore allowed the sender task to be unblocked
	static BaseType_t xTask_awake = pdFALSE;
	xSemaphoreGiveFromISR( sender_2_Semaphore, &xTask_awake );
	handle_timer_2_period();

}

void receiver_TimerCallback(TimerHandle_t xTimer)
{
	static BaseType_t xTask_awake = pdFALSE;
	xSemaphoreGiveFromISR( receiver_Semaphore, &xTask_awake );
		if (recieved_messages == 500)
	{
		Reset();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////


#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------

void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amout of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}

void vApplicationTickHook(void) {
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
  /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
  state will be stored. */
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

  /* Pass out the array that will be used as the Idle task's stack. */
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;

  /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
  Note that, as the array is necessarily of type StackType_t,
  configMINIMAL_STACK_SIZE is specified in words, not bytes. */
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
