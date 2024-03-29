<h1>Description</h1>

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

    used as example, but not directly copied: https://gist.github.com/jasonwhite/c5b2048c15993d285130

    pretty sure that event type 129 is button init and 130 is axis init
    also, when opening a js device, it runs through an init cycle that tests all the joystick elements

    js2mouse_v1.c is an old version

<h2>Dependencies:</h2>

    xdotool
        standalone in Debian-based systems; installed with `sudo apt install xdotool`
        standalone for Arch-based as well; installed with `sudo pacman -S[yu] xdotool`

<h2>Known Bugs</h2>

1
-

<code>read()</code> call returning 2^64 bytes when reading from the joystick file.
even though there is a max parameter that seems like it should give the maximum number of bytes to read

2
-

memory overflow crash when reading from stdin. Occurs for both using getline and getchar

<h2>TODO:</h2>
	- re-compile with debug info and check valgrind output. see if solving the "address is 0 bytes after a block of size 32 is alloc'd" error fixes the crash 
	- translate usage of system() into exec family of functions (see man 3 execl)
	- using execl will also allow for checking if xdotool is installed
	- look into option to use wayland-based equivalent of xdotool
		(there exists ydotool, which is intended to be a drop-in replacement for xdotool)
	- security risk: config file could inject malicious code into system() calls
        but if the program reads the file expecting an int,
            then the program should crash at worst,
            so I think it's ok
   - research chardevice files to learn more about js0
   - /!\ look into using access again; looks like it can return 0 on an empty device file

   check file before reading in the loop
   record time since last input; prompt user to quit if left for too long
   port the config values to a struct that gets populated by a config file and
    pass the struct as a const pointer in each function that uses config values


   <h2>Long-Term TODO:</h2>
   - port to C++, making the joystick utilities being part of a class
   - investigate porting to Rust. If Rust supports an analog of the C joystick header, then that will probably get around the weird read bug. Might be able to use the C header directly, depending on how cross-language support works
<h1>dev notes:</h1>
<p>
xorg xlib docs:
https://x.org/releases/current/doc/libX11/libX11/libX11.html

pointed me to xlib:
https://forums.justlinux.com/showthread.php?104013-Controlling-the-mouse-from-bash-scripts

links to resources related to moving the mouse:
https://tronche.com/gui/x/xlib/input/XWarpPointer.html
https://www.x.org/releases/current/doc/man/man3/XSendEvent.3.xhtml

/!\example of using XLib:
https://www.linuxquestions.org/questions/programming-9/simulating-a-mouse-click-594576/


Realized that the tool xdotool exists. Simple bash command that does all the stuff I need.
Yes, I'm calling shell commands in a C program.

<h2>solution to getting both axis values from one event</h2>
code slippet from: https://archives.seul.org/linuxgames/Aug-1999/msg00107.html

	while( 1 ) 	/* infinite loop */
	{
		/* read the joystick state */
		read(joy_fd, &js, sizeof(struct js_event));

			/* see what to do with the event */
		switch (js.type & ~JS_EVENT_INIT)
		{
			case JS_EVENT_AXIS:
				axis   [ js.number ] = js.value;
				break;
			case JS_EVENT_BUTTON:
				button [ js.number ] = js.value;
				break;
		}

			/* print the results */
		printf( "X: %6d  Y: %6d  ", axis[0], axis[1] );

		if( num_of_axis > 2 )
			printf("Z: %6d  ", axis[2] );

		if( num_of_axis > 3 )
			printf("R: %6d  ", axis[3] );

		for( x=0 ; x<num_of_buttons ; ++x )
			printf("B%d: %d  ", x, button[x] );

		printf("  \r");
		fflush(stdout);
	}

note that every event sets values in an axis[] array. This way, a running book of the axis states is kept,
updated by every caught event


<h2>using supertuxkart to see how velocity is maintained</h2>
stk code base: https://github.com/supertuxkart/stk-code/tree/master/src

in src/karts/abstract_kart.cpp:241: body->setLinearVelocity(Vec3(0.0f));
body is of type btRigidBody.

This is from the Bullet physics library: https://pybullet.org/Bullet/BulletFull/classbtRigidBody.html

<h2>Note on read mode for sockets</h2>
in https://docs.python.org/3/howto/sockets.html#non-blocking-sockets:
it mentions that O_NONBLOCK is a BSD flavor flag. O_NDELAY is a POSIX flavor flag. Perhaps using POSIX would solve the weird reading behavior

<h2>A note on checking for data in the file:</h2>
in https://unix.stackexchange.com/a/25607

they say to use <code>poll()</code> or <code>select()</code> to check for incoming data
</p>


