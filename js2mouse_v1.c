/*
 * Author: James Pangia
 *
 * usage: ./js2mouse [deviceName] [L]
 *
 *  deviceName: the name of the joystick device to read; expects a js* device name
 *              If no device is specified, /dev/input/js0 is used.
 *  L: specify that the left joystick should move the cursor (default uses right)
 *
 * Description:
 * reads the inputs from the specified joystick device and uses the joystick for
 * keyboard/mouse inputs
 *
 * Notes:
 *  Designed for use with an XBox 360 controller; behavior for other joystick devices
 *  is undefined.
 *
 *  used as example, but not directly copied: https://gist.github.com/jasonwhite/c5b2048c15993d285130
 *
 *  pretty sure that event type 129 is button init and 130 is axis init
 *  also, when opening a js device, it runs through an init cycle that tests all the joystick elements
 *
 * TODO:
 * -?- translate usage of system() into exec family of functions (see man execl 3)
 * security risk: config file could inject malicious code into system() calls
 *      but if the program reads the file expecting an int,
 *          then the program should crash at worst,
 *          so I think it's ok
 */

#include <stdlib.h> //for system()
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h> //for file interface functions like read() and execl
#include <linux/joystick.h> //for joystick_event struct and related
#include <fcntl.h> //for open() function

/*preprocessor constants*/

//debug flag: uncomment for debug output
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

//button number constants
#define A_BTN 0
#define B_BTN 1
#define X_BTN 2
#define Y_BTN 3
#define LB_BTN 4
#define RB_BTN 5
#define BACK_BTN 6
#define START_BTN 7
#define XBOX_BTN 8 //(the middle "home" button)

//axes number constants
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

void handleDpadH(int value);
void handleDpadV(int value);

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
#if DEBUG
    printf("Running in debug mode. . .\n");
    sleep(2);
#endif

    //TODO: make sure that xdotool is available

    //check the first argument
    if(argc == 2)
    {
        if(0 == strcmp(argv[1], "L"))
        {
            printf("Running in left-handed mode. . .\n");
            //TODO: set left-handed flag
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
        //TODO: set left-handed flag
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
    int js = open(devicePath, O_RDONLY);
//     int js = open(devicePath, O_NONBLOCK);
    if(js < 0)
    {
        printf("Error: failed to open device %s\nExiting....", devicePath);
        return -1;
    }

    //get the number of axes in the device
    int axisCount = 0;
    ioctl(js, JSIOCGAXES, &axisCount);

    //dynamically allocate an array of axis values, with one cell per axis
    int* axes = (int*) calloc(axisCount, sizeof(int)); //gives an array filled with 0's

    //read first event
    size_t numRead = read(js, &event, sizeof(struct js_event));

    //debug
    printf("numRead: %lu\n", numRead);
    //command string that gets prepared using sprintf and executed using system()
    char cmd[CMD_LEN];

    //a flag to quit the loop; gets set when XBOX_BTN is pressed
    bool quit = false;

    //TODO: spawn stick handler thread

    //begin loop to read all the events until no event is read
    //if numRead is not sizeof(struct js_event), then the controller got disconnected and it's time to stop;
    //for some reason read() is returning 18446744073709551615, not -1 when the joystick disconnects
    while(numRead ==  sizeof(struct js_event) && !quit)
    {
        //debug
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

        /*
        //TODO: figure out why this loop doesn't keep going
        int success = 0;
        int hStick = 0;
        int vStick = 0;

        //test
        axes[event.number] = event.value;
        if(lefty)
        {
            hStick = L_STICK_H;
            vStick = L_STICK_V;
            success = handleStick(axes, axisCount, L_STICK_H, L_STICK_V, L_STICK_DEADZ);
        }
        else
        {
            hStick = R_STICK_H;
            vStick = R_STICK_V;
            success = handleStick(axes, axisCount, R_STICK_H, R_STICK_V, R_STICK_DEADZ);
        }

        if(-1 == success) //report if function errored
        {
            printf("Error: Tried to move an axis the device does not have\n");
            printf("\tAxes to move: %d, %d\n\tAxis count: %d\n", hStick, vStick, axisCount);
        }*/


        //handle buttons, taking button press events, excluding button release events
        if(JS_EVENT_BUTTON == event.type && true == event.value)
        {
            //TODO: possibly move this logic into a button handler function
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

        //*
        //handle axes
        else if(JS_EVENT_AXIS == event.type)
        {
            //TODO: lock axes array
            //update the axes array
            axes[event.number] = event.value;
//             printf("axes[%d]: %d\n", event.number, axes[event.number]); //debug
            //TODO: release axes array
            //TODO: remove stick cases; this would be handled in the stick thread
            switch(event.number)
            {
                //left stick moves mouse if option L
                case L_STICK_H:
                case L_STICK_V:
                    if(lefty)
                    {
                        int success = handleStick(axes, axisCount, L_STICK_H, L_STICK_V, L_STICK_DEADZ);
                        if(-1 == success) //report if function errored
                        {
                            printf("Error: Tried to move an axis the device does not have\n");
                            printf("\tAxes to move: %d, %d\n\tAxis count: %d\n", L_STICK_H, L_STICK_V, axisCount);
                        }
                    }
                    break;

                //right stick moves mouse if no option L
                case R_STICK_H:
                case R_STICK_V:
                    if(!lefty)
                    {
                        int success = handleStick(axes, axisCount, R_STICK_H, R_STICK_V, R_STICK_DEADZ);
                        if(-1 == success) //report if function errored
                        {
                            printf("Error: Tried to move an axis the device does not have\n");
                            printf("\tAxes to move: %d, %d\n\tAxis count: %d\n", R_STICK_H, R_STICK_V, axisCount);
                        }
                    }
                    break;
                //d-pad moves arrow keys
                case D_PAD_H:
                    handleDpadH(event.value);
                    break;
                case D_PAD_V:
                    handleDpadV(event.value);
                    break;
                default:
                    printf("Unhandled event number: %d\n", event.number); //maybe remove if this is too annoying
                    break;
            }
        }
        //*/

        //read a new event
        numRead = read(js, &event, sizeof(event));
        //debug
        printf("numRead: %lu\n", numRead);
    }

    if(sizeof(struct js_event) != numRead)
    {
        printf("Device %s disconnected.\nClosing. . .\n", devicePath);
    }

    //cleanup
    //TODO: clean up thread
    close(js);
    free(axes);
    axes = NULL;
    return 0;
}

/*
 * handles events from the horizontal D-Pad
 *
 * @param int value: the state of the component
 */
void handleDpadH(int value)
{
    if(value > D_PAD_DEADZ) //start pressing right
    {
        printf("right\n");
        system("xdotool keydown 114");
    }
    else if(value > -D_PAD_DEADZ) //stop pressing both
    {
        printf("stop dpad horizontal\n");
        system("xdotool keyup 114 keyup 113");
    }
    else //start pressing left
    {
        printf("left\n");
        system("xdotool keydown 113");
    }
}

/*
 * handles events from the vertical D-Pad
 *
 * @param int value: the state of the component
 */
void handleDpadV(int value)
{
    if(value > D_PAD_DEADZ) //start pressing down
    {
        printf("down\n");
        system("xdotool keydown 116");
    }
    else if(value > -D_PAD_DEADZ) //stop pressing both
    {
        printf("stop dpad vertical\n");
        system("xdotool keyup 116 keyup 111");
    }
    else //start pressing up
    {
        printf("up\n");
        system("xdotool keydown 111");
    }
}


//TODO: use this or delete it; global variables are a bad idea, but this one is only visible to functions
//      defined below it
//      but it is persistent across function calls....
//      don't think this really matters though; using p_thread_join is probably the solution....
// int oink = 5;

/*
 * handles the horizontal right stick events.
 * Checks the array size stored in axes_len to ensure there
 * is no overflow from accessing indexes hAxisNum or vAxisNum.
 * Pulls the horizonal andd vertical axis values from the passed
 * array.
 *
 * @param int* axes the head of an array of axis values
 * @param int axes_len the length of the axes array
 * @param int hAxisNum the number of the horizontal axis
 * @param int vAxisNum the number of the vertical axis
 * @param int deadZone the upper limit value of the deadzone
 * @return -1 if hAxisNum or vAxisNum outside of axes, 0 otherwise
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
    printf("hValue: %d\nvValue: %d\n", hValue, vValue); //debug
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
    printf("nudgeH: %d\nnudgeV: %d\n", nudgeH, nudgeV); //debug
#endif

    //TODO: set up a mechanism for keeping the cursor moving when the joystick is maxed out
    //      like say you move the joystick all the way left, the mouse only moves a little;
    //      it needs to keep moving until the joystick goes back to "home" position
    //idea: spawn thread to move mouse in infinite loop
    //      at beginning of funciton call, call thread join to kill previous thread, then make next
    //      thread
    //      possibly try to kill all threads
    if(0 != nudgeH || 0 != nudgeV)
    {
        //build command
        char cmd[CMD_LEN];
        sprintf(cmd, "xdotool mousemove_relative -- %d %d", nudgeH, nudgeV);
        //sprintf(cmd, "while true; do xdotool mousemove_relative -- %d %d; done", nudgeH, nudgeV);
        //run command
        system(cmd);
    }
    return 0;
}

/*
 * thread main function to move the cursor.
 * the axes array is a shared resource. This function reads the values.
 * If the values are in the deadzone range, move the mouse, else do nothing;
 * basically a port of handleStick above; maybe just call handleStick
 *
 */
