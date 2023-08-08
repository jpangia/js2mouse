/*
   Author: James Pangia
  
   usage: ./js2mouse [deviceName] [L]
  
    deviceName: the name of the joystick device to read; expects a js* device name
                If no device is specified, /dev/input/js0 is used.
    L: specify that the left joystick should move the cursor (default uses right)
  
   Description:
   reads the inputs from the specified joystick device and uses the joystick for
   keyboard/mouse inputs
  
   Notes:
    Designed for use with an XBox 360 controller; behavior for other joystick devices
    is undefined.
  
   Dependencies:
    xdotool 
        standalone in Debian-based systems; installed with `sudo apt install xdotool`
        standalone for Arch-based as well; installed with `sudo pacman -S[yu] xdotool`s
*/

#include <stdlib.h> //for system()
#include <stdio.h>
#include <stdbool.h> //bool, true/false
#include <string.h> //strcat, strcpy
#include <unistd.h> //for file interface functions like read(), access() and execl
#include <linux/joystick.h> //for js_event struct and related constants
#include <fcntl.h> //for open() function
#include <time.h> //for time()

/*preprocessor constants*/

//debug flag: 0 for no debug mode, 1 for debug
#define DEBUG 0

//config constants
#define DEVICE_N_LEN 256 //an arbitrary length that should be big enough; change if necessary
#define CMD_LEN 256 //an arbitrary length that should be big enough; change if necessary
//deadzones are applied ad hoc using a simple if statement in the appropriate switch cases in the main loop
#define R_STICK_DEADZ 1000 //deadzone for right stick
#define L_STICK_DEADZ R_STICK_DEADZ //default the left deadzone to the right deadzone
#define R_TRIGGER_DEADZ 1000 //deadzone for right trigger
#define L_TRIGGER_DEADZ R_TRIGGER_DEADZ //default the left deadzone to the right deadzone
#define D_PAD_DEADZ 1000

#define TIME_OUT 5 //the time in seconds it takes for the device to time out

//button identifier constants
#define A_BTN 0
#define B_BTN 1
#define X_BTN 2
#define Y_BTN 3
#define LB_BTN 4
#define RB_BTN 5
#define BACK_BTN 6
#define START_BTN 7
#define XBOX_BTN 8 //(the middle "home" button)

//axes identifier constants
#define L_STICK_H 0 // Horizontal left stick (left is negative, right positive)
#define L_STICK_V 1 // vertical left stick (up is negative, down positive)
#define L_TRIGGER 2 // left trigger (pressed is positive)
#define R_STICK_H 3 // Horizontal right stick(left is negative, right positive)
#define R_STICK_V 4 // vertical right stick (up is negative, down positive)
#define R_TRIGGER 5 // right trigger (pressed is positive)
#define D_PAD_H 6   // Horizontal D-pad (left is negative, right positive)
#define D_PAD_V 7   // vertical D-pad (up is negative, down positive)

//keyboard/mouse constants
#define CLICK_L  1  //left click
#define CLICK_M  2  //middle click
#define CLICK_R  3  //right click
#define SCROLL_U 4  //scroll up
#define SCROLL_D 5  //scroll down

#define ARROW_U 111   //up arrow key
#define ARROW_L 113   //left arrow key
#define ARROW_R 114   //right arrow key
#define ARROW_D 116   //down arrow key

int handleDpadH(int value);
int handleDpadV(int value);

int handleStick(const int* axes, int axes_len, int hAxisNum, int vAxisNum, int deadZone);

int main(int argc, char* argv[])
{
    //device directory
    const char DEV_DIR[] = "/dev/input/";

    //full device path initialization
    char devicePath[DEVICE_N_LEN];
    strcpy(devicePath, DEV_DIR);

    //left-hand flag
    bool lefty = false;

    //time elapsed since last event
    time_t timeSince = time(NULL); printf("moo\n");//debug
    // printf("%d\n", timeSince); //debug

#if DEBUG
    printf("Running in debug mode. . .\n");
    sleep(2);
#endif

    //TODO: make sure that xdotool is available
    //check config file; it should specify ydotool or xdotool
    //dummy call to tool using execl
    //if execl sends fail, send an error and quit

    //check the first argument
    if(argc == 2)
    {
        if(0 == strcmp(argv[1], "L"))
        {
            printf("Running in left-handed mode. . .\n");
            lefty = true;
        }
        else
        {
            printf("Using device [%s] to control mouse and keyboard inputs. . .\n", argv[1]);
            //set device path
            strcat(devicePath, argv[1]);
        }
    }
    //check for an L argument after a device
    else if(argc >= 3 && 0 == strcmp(argv[2], "L"))
    {
        printf("Running in left-handed mode. . .\n");
        lefty = true;
    }
    //default message
    else
    {
        printf("Using device js0 to control mouse and keyboard inputs. . .\n");
        strcat(devicePath, "js0");
    }

    //TODO: move lots of the constants to a config
    printf("Using deadzone values:\n");
    printf("\tR_STICK_DEADZ: %d\n\tL_STICK_DEADZ: %d\n", R_STICK_DEADZ, L_STICK_DEADZ);
    printf("\tD_PAD_DEADZ: %d\n", D_PAD_DEADZ);

    //struct for the event that gets read
    struct js_event event;

    //open the device for reading
//     int js = open(devicePath, O_RDONLY); //blocking read: blocks program if nothing to read
    int js = open(devicePath, O_NONBLOCK); // nonblocking; just moves on if there's nothing to read
    if(js < 0)
    {
        printf("Error: failed to open device %s\nExiting....", devicePath);
        return -1;
    }
    

    //get the number of axes in the device
    int axisCount = 0;
    ioctl(js, JSIOCGAXES, &axisCount);

    printf("axisCount: %d\n", axisCount); //debug
    printf("sizeof(int): %ld\n", sizeof(int)); //debug

    //dynamically allocate an array of axis values, with one cell per axis
    int* axes = (int*) calloc(axisCount, sizeof(int)); //gives an array filled with 0's

    // getchar(); //put a getchar here and the memory bug from reading stdin stops....

    //read first event
    /*size_t numRead = */read(js, &event, sizeof(struct js_event));

    //command string that gets prepared using sprintf and executed using system()
    char cmd[CMD_LEN];

    //a flag to quit the loop; gets set when XBOX_BTN is pressed
    bool quit = false;

    //begin loop to read all the events until it's time to quit
    //TODO: for some reason read() is returning 18,446,744,073,709,551,615 = (2^64)-1, not -1 when the joystick disconnects
    while(!quit)
    {
        //check the timeout
        if( (time(NULL) - timeSince) > TIME_OUT)
        {
            printf("It has been %d seconds since last input.\n", TIME_OUT);
            printf("Do you want to quit (y/n): ");
            // printf("\n");
            
            char inC = getchar(); //TODO: crashes as soon as I try to read user input....
            if('y' == inC)
            {
                printf("Closing. . . .\n");
                break;
            }

            timeSince = time(NULL); //reset timeSince
        }

        #if DEBUG
            printf("\nnumRead: %lu\n\n", numRead);
            printf("Event time: %d\n", event.time);
            printf("Event value: %d\n", event.value);
            printf("Event type: %d\n", event.type);
            if(JS_EVENT_AXIS == event.type)
            {
                printf("Axis number: %d\n", event.number);
            }
            if(JS_EVENT_BUTTON == event.type)
            {
                printf("button number: %d\n", event.number);
            }
        #endif

        int success = 0;
        int hStick = 0; //just used in error reporting
        int vStick = 0; //just used in error reporting
        // time_t lastTimeSince = timeSince;

        //constantly update mouse
        axes[event.number] = event.value;
        if(lefty) //left-hand mode
        {
            hStick = L_STICK_H;
            vStick = L_STICK_V;
            success = handleStick(axes, axisCount, L_STICK_H, L_STICK_V, L_STICK_DEADZ);
            
        }
        else //right hand mode
        {
            hStick = R_STICK_H;
            vStick = R_STICK_V;
            success = handleStick(axes, axisCount, R_STICK_H, R_STICK_V, R_STICK_DEADZ);
        }

        if(-1 == timeSince) //report if function errored
        {
            printf("Error: Tried to move an axis the device does not have\n");
            printf("\tAxes to move: %d, %d\n\tAxis count: %d\n", hStick, vStick, axisCount);
            timeSince = time(NULL); //reset the time
        }
        //if the values were in the deadzone
        else if(1 == success)
        {
            timeSince = time(NULL); printf("moo\n");//debug //reset the time to before the joystick was handled
        }

        //handle buttons, taking button press events, excluding button release events
        if(JS_EVENT_BUTTON == event.type && true == event.value)
        {
            timeSince = time(NULL);
            //TODO: move this logic into a button handler function
            switch(event.number)
            {
                case A_BTN: //A is left click
                    printf("left click!\n");
                    sprintf(cmd, "xdotool click %d", CLICK_L);
                    system(cmd);
                    break;
                case B_BTN: //B is right click
                    printf("right click!\n");
                    sprintf(cmd, "xdotool click %d", CLICK_R);
                    system(cmd);
                    break;
                case X_BTN: //X is middle click
                    printf("middle click!\n"); //gonna have to fix my middle-click functionality before working on this....
                    sprintf(cmd, "xdotool click %d", CLICK_M);
                    system(cmd);
                    break;
                case RB_BTN: //RB is scroll down (unless option L is specified)
                    printf("scroll down!\n");
                    //TODO: make RB scroll up if L is specified in run command
                    break;
                case LB_BTN: //LB is scroll up (unless option L is specified)
                    printf("scroll up!\n");
                    //TODO: make LB scroll down if L is specified in run command
                    break;
                case XBOX_BTN: //exits the program
                    quit = true;
                    printf("quit!\n");
                    break;
                default:
                    printf("Unhandled event number: %d\n", event.number); //maybe remove if this is too annoying
                    break;
            }
        }

        //handle dpad
        else if(JS_EVENT_AXIS == event.type)
        {
            int success = -1;
            switch(event.number)
            {
                //d-pad moves arrow keys
                case D_PAD_H:
                    success = handleDpadH(event.value);
                    if(0 == success)
                    {
                        timeSince = time(NULL); printf("moo\n");//debug
                    }
                    break;
                case D_PAD_V:
                    success = handleDpadV(event.value);
                    if(0 == success)
                    {
                        timeSince = time(NULL); printf("moo\n");//debug
                    }
                    break;
                /*
                default:
                    printf("Unhandled event number: %d\n", event.number); //maybe remove if this is too annoying
                    break;
                */
            }
        }

        if(access(devicePath, O_RDONLY) != -1)
        {
            //read a new event if the file is accessible
            /*numRead = */read(js, &event, sizeof(struct js_event));
        }
    }

    //cleanup
    close(js);
    free(axes);
    axes = NULL;
    return 0;
}

/*
   handles events from the horizontal D-Pad
  
   @param int value: the state of the component
   @return int 0 if a button is pressed, else -1
 */
int handleDpadH(int value)
{
    if(value > D_PAD_DEADZ) //start pressing right
    {
        printf("right\n");
        system("xdotool keydown 114");
        return 0;
    }
    else if(value > -D_PAD_DEADZ) //stop pressing both
    {
        printf("stop dpad horizontal\n");
        system("xdotool keyup 114 keyup 113");
        return -1;
    }
    else //start pressing left
    {
        printf("left\n");
        system("xdotool keydown 113");
        return 0;
    }
}

/*
   handles events from the vertical D-Pad
  
   @param int value: the state of the component
   @return int 0 if action goes through, else -1
 */
int handleDpadV(int value)
{
    if(value > D_PAD_DEADZ) //start pressing down
    {
        printf("down\n");
        system("xdotool keydown 116");
        return 0;
    }
    else if(value > -D_PAD_DEADZ) //stop pressing both
    {
        printf("stop dpad vertical\n");
        system("xdotool keyup 116 keyup 111");
        return -1;
    }
    else //start pressing up
    {
        printf("up\n");
        system("xdotool keydown 111");
        return 0;
    }
}

/*
   handles the horizontal right stick events.
   Checks the array size stored in axes_len to ensure there
   is no overflow from accessing indexes hAxisNum or vAxisNum.
   Pulls the horizonal andd vertical axis values from the passed
   array.
   Returns the system time to be used in book-keeping when the last event went through
  
   @param int* axes the head of an array of axis values
   @param int axes_len the length of the axes array
   @param int hAxisNum the number of the horizontal axis
   @param int vAxisNum the number of the vertical axis
   @param int deadZone the upper limit value of the deadzone
   @return -1 if hAxisNum or vAxisNum outside of axes, 
           0 if the values are outside deadzone range,
           1 otherwise
*/
int handleStick(const int* axes, int axes_len, int hAxisNum, int vAxisNum, int deadZone)
{
    //if indexes out of bounds, fail
    if(hAxisNum >= axes_len || vAxisNum >= axes_len)
    {
        return -1;
    }

    int hValue = axes[hAxisNum]; //the deflection value of the horizontal stick
    int vValue = axes[vAxisNum]; //the deflection value of the vertical stick

#if DEBUG
    printf("hValue: %d\nvValue: %d\n", hValue, vValue);
#endif

    int nudgeH = 0;
    int nudgeV = 0;

    //calculate nudge values, checking deadzones;
    //horizontal
    //if value is within the deadzone set the nudge value to 0
    if(hValue < deadZone && hValue > -deadZone)
    {
        nudgeH = 0;
    }
    else
    {
//         nudgeH = hValue/abs(hValue); //test
        nudgeH = hValue / 10000;
    }

    //vertical
    //if value is within the deadzone set the nudge value to 0
    if(vValue < deadZone && vValue > -deadZone)
    {
        nudgeV = 0;
    }
    else
    {
//         nudgeV = vValue/abs(vValue); //test
        nudgeV = vValue / 10000;
    }

#if DEBUG
    printf("nudgeH: %d\nnudgeV: %d\n", nudgeH, nudgeV);
#endif

    //if nonzero nudges, do something
    if(0 != nudgeH || 0 != nudgeV)
    {
        //build command
        char cmd[CMD_LEN];
        sprintf(cmd, "xdotool mousemove_relative -- %d %d", nudgeH, nudgeV);
        //run command
        system(cmd);
        return 1; //return success code
    }
    return 0;
}
