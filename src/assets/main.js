
let x = 0;

// Basically creates an infinite loop, similar to while true (you can use it too).
let interval = os.setInterval(() => {
    Screen.start_drawing()
    Screen.clear(50, 50, 50) // rgba

    Screen.print(`Hello world menó! x = ${++x}`) // color is hardcoded!!

    Screen.end_drawing()

    if (x >= 200) {
        console.log('Closing app...\n'); // show the message on stdout.

        os.clearInterval(interval); // it closes the app.
    }
}, 0);

