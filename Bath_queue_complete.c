#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

//Type definitions
typedef unsigned char uint8;
typedef uint8   boolean;

#define cTrue   (boolean)(1 == 1)
#define cFalse  (boolean)(1 == 0)

typedef enum GroupType
{
    eInit = 0,
    eMale,
    eFemale,
    eGroupCount
}eBathGroupType;

//Enum for the possible queue processing cases
typedef enum ReturnType
{
    eOk = 0,
    eFull,
    eEmpty,
    eInvalid,
    eQueueCount
}eReturnType;

/* Definition of Task based parameters */
//the time needed for N people to take a bath
#define BATH_TIME   (uint8)(1)

//The period to wait in case the queue is full or empty, applicable for both threads
#define SLEEP_TIME (uint8)(2)

//For testing purposes
#define BATH_TIME_M (uint8)(10)

//The bath capacity will be randomly generated and will have a value between 5 and 20
#define BATH_MIN    (uint8)(5)
#define BATH_MAX    (uint8)(10)

//The number of students male and female will also be randomly generated with range between 15 and 50
//combined male + female will always be more than the maximal Bath capacity
#define STUDENTS_MIN    (uint8)(15)
#define STUDENTS_MAX    (uint8)(50)

//The maximal capacity of the Queue
#define QUEUE_CAPACITY (uint8)(10)

/* Function Prototypes */
//brief Function generates random number in the min - max range
//param min the minimal value to be generated
//param max the maximal value to be generated
//return returns the generated random number
uint8 getRand(const uint8 minP, const uint8 maxP);

//brief Function executed by the threadBath. Bathing and decreasing the number of male or female students
//param bathingParam none
//return none
void *doBathing(void *bathingParam);

//brief Function executed by the threadDecide. Decides which group time will bath next based on the feminist rule
//param calcParam none
//return none
void *doDecide(void *calcParam);

//brief Calculates how many people from the current group will take a bath
//param current the type of the current group
//return either bath the same number of people as the bath capacity or the remaining ones
uint8 prepareNextPortion(const eBathGroupType current);

//brief Checks if the queue is full
//param none
//return cTrue if the queue is full, cFalse otherwise
boolean isFull(void);

//brief Checks if the queue is empty
//param none
//return cTrue if the queue is empty, cFalse otherwise
boolean isEmpty(void);

//brief checks if the next value extracted from the queue is valid for the task
//param current the element which is being extracted and processed
//return cTrue if the element is valid and won't break the Feminist rule, cFalse otherwise
boolean isGroupValid(eBathGroupType current);

//brief Adds an element to the queue 
//param nextGroup the type of group to be added to the queue at rearPointer position
//return returns status of the enqueu operation
eReturnType enqueue(const eBathGroupType nextGroup);

//brief Removes an element from the queue
//param nextGroup pointer to the value which will store the next bathing group
//return returns status of the dequeue operatiion
eReturnType dequeue(eBathGroupType *nextGroup);

//brief Queue error handling function
//param errorType the type of error in the queue
//return none
void queueErrorHandle(const eReturnType errorType);


/* Variables in the data segment */
//Variables to store the later generated count for men and women
uint8 MaleCount = 0;
uint8 FemaleCount = 0;

//Variable to store the capacity generated value
uint8 BathCapacity = 0;

//Mutex
pthread_mutex_t mutexBath = PTHREAD_MUTEX_INITIALIZER;

//Queue implementation needed variables - a queue of the bath group types
eBathGroupType groupQueue[QUEUE_CAPACITY] = { eInit };
uint8 frontPointer = 0;
uint8 rearPointer = QUEUE_CAPACITY - 1;
uint8 queueSize = 0;

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
{
    pthread_t threadBathL;
    pthread_t threadDecideL;

    //Use current time as seed for random num generation
    srand(time(0));

    //Generate the random numbers for men/women count + bath capacity
    MaleCount = getRand(STUDENTS_MIN, STUDENTS_MAX);
    FemaleCount = getRand(STUDENTS_MIN, STUDENTS_MAX);
    BathCapacity = getRand(BATH_MIN, BATH_MAX);

    //Print the generated data
    printf("Number of men: %d\n", MaleCount);
    printf("Number of women: %d\n", FemaleCount);
    printf("Bath capacity: %d\n", BathCapacity);

    if(0 != pthread_create(&threadDecideL, NULL, doDecide, NULL))
    {
        printf("Thread creation error.\n");
    }

    if(0 != pthread_create(&threadBathL, NULL, doBathing, NULL))
    {
        printf("Thread creation error.\n");
    }

    pthread_join(threadDecideL, NULL);
    pthread_join(threadBathL, NULL);

    return 0;
}

//-----------------------------------------------------------------------------
uint8 getRand(const uint8 minP, const uint8 maxP)
{
    uint8 randNumberL = 0;

    randNumberL = ( rand() % (maxP - minP + 1)) + minP;

    return randNumberL;
}

//-----------------------------------------------------------------------------
void *doBathing(void *bathingParam)
{
    //a local variable used to store the calculated number of people for the current bathing
    uint8 currentBathingCountL = 0;
    eReturnType resL = eOk;
    eBathGroupType current = eInit;

    while(cTrue)
    {
        //Infinite loop prevention
        if((MaleCount == 0) && (FemaleCount == 0))
        {
            break;
        }

        pthread_mutex_lock(&mutexBath);

        //Get the next bath group
        resL = dequeue(&current);

        pthread_mutex_unlock(&mutexBath);
        
        //in case there is an error with the queue, handle it
        if(resL != eOk)
        {
            queueErrorHandle(resL);
            //Give time to the writting thread to write in the queue
            sleep(SLEEP_TIME);
        }
        else
        {
            pthread_mutex_lock(&mutexBath);

            currentBathingCountL = prepareNextPortion(current);

            if(current == eMale)
            {
                MaleCount -= currentBathingCountL;
            }
            else
            {
                FemaleCount -= currentBathingCountL;
            }
            

            printf("\nNumber of men after bath: %d\n", MaleCount);
            printf("Number of women after bath: %d\n", FemaleCount);

            pthread_mutex_unlock(&mutexBath);
        }

        //The bath process takes 1 second according to the task
        //sleep(BATH_TIME);

        //For testing purposes
        usleep(BATH_TIME_M);
    }
}

//-----------------------------------------------------------------------------
void *doDecide(void *calcParam)
{
    eBathGroupType nextL = eInit;
    eReturnType resL = eOk;

    while(cTrue)
    {
        //Infinite loop prevention
        if((MaleCount == 0) && (FemaleCount == 0))
        {
            break;
        }

        pthread_mutex_lock(&mutexBath);

        //The feminist rule!
        if( MaleCount > FemaleCount )
        {
            nextL = eMale;
        }
        else
        {
            nextL = eFemale;
        }

        //enqueue the next bath group
        resL = enqueue(nextL);

        pthread_mutex_unlock(&mutexBath);

        //in case there is an error with the queue, handle it
        if(resL != eOk)
        {
            queueErrorHandle(resL);
            //Give time to the reading thread to empty the queue
            sleep(SLEEP_TIME*2);
        }

        //For testing purpose
        sleep(BATH_TIME);
        //usleep(BATH_TIME_M);
    }
}

//-----------------------------------------------------------------------------
uint8 prepareNextPortion(const eBathGroupType current)
{
    uint8 bathCountL = 0;

    //check the current group type
    if(current == eMale)
    {
        //determine the number of men to take a bath
        if(MaleCount >= BathCapacity)
        {
            bathCountL = BathCapacity;
        }
        else
        {
            bathCountL = MaleCount;
        }
    }
    else
    {
        //determine the number of women to take a bath
        if(FemaleCount >= BathCapacity)
        {
            bathCountL = BathCapacity;
        }
        else
        {
            bathCountL = FemaleCount;
        }
    }
    
    return bathCountL;
}

//-----------------------------------------------------------------------------
boolean isFull(void)
{
    return (queueSize == QUEUE_CAPACITY);
}

//-----------------------------------------------------------------------------
boolean isEmpty(void)
{
    return (queueSize == 0);
}

boolean isGroupValid(eBathGroupType current)
{
    boolean resL = cTrue;

    if(current == eMale)
    {
        if( MaleCount <= FemaleCount )
        {
            resL = cFalse;
        }
    }
    else if(current == eFemale)
    {
        if( FemaleCount < MaleCount )
        {
            resL = cFalse;
        }
    }
    

    return resL;
}

//-----------------------------------------------------------------------------
eReturnType enqueue(const eBathGroupType nextGroup)
{
    eReturnType retL = eOk;

    if(cFalse != isFull())
    {
        retL = eFull;
    }
    else
    {
        //printf("Enqeueueing %d.\n", nextGroup);
        rearPointer = (rearPointer + 1) % QUEUE_CAPACITY;
        groupQueue[rearPointer] = nextGroup;
        queueSize++;
    }

    return retL;    
}

//-----------------------------------------------------------------------------
eReturnType dequeue(eBathGroupType *nextGroup)
{
    eReturnType retL = eOk;

    if(cFalse != isEmpty())
    {
        retL = eEmpty;
    }
    else
    {
        //printf("Dequeueing %d\n", groupQueue[frontPointer]);
        if(cFalse != isGroupValid(groupQueue[frontPointer]))
        {
            *nextGroup = groupQueue[frontPointer];
        }
        else
        {
            retL = eInvalid;
        }

        frontPointer = (frontPointer + 1) % QUEUE_CAPACITY;
        queueSize--;
        
    }

    return retL;
}

//-----------------------------------------------------------------------------
void queueErrorHandle(const eReturnType errorType)
{
    //If the queue is full wait some time for it to be freed.
    if(errorType == eFull)
    {
        printf("The queue is full. The enqueueing thread will wait.\n");
    }
    //If the queue is empty wait for some data.
    else if(errorType == eEmpty)
    {
        printf("The queue is empty. The dequeueing thread will wait.\n");        
    }
    else if (errorType == eInvalid)
    {
        //The enqueueing thread is filling the queue too fast before the bathing is finished
        //so the values are wrong and won't be according to the rule
        //printf("Invalid element.\n");
    }
    else
    {
        //nothing
    }
    
       
}