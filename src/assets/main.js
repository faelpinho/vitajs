
let inc = 0;

//let font = Font.load_font_file("app0:/assets/segoeui.ttf");

const texture1 = Screen.load_png_file("app0:/assets/test1.png");

let pads = Pads;
let x = 200, y = 200;
let steps = 3;

// Basically creates an infinite loop, similar to while true (you can use it too).
let interval = os.setInterval(() => {

    if (pads.check(pads.UP))
        y -= steps;

    if (pads.check(pads.DOWN))
        y += steps;

    if (pads.check(pads.LEFT))
        x -= steps;

    if (pads.check(pads.RIGHT))
        x += steps;

    Screen.start_drawing()
    Screen.clear(50, 50, 50) // rgba

    Screen.print(`Hello world menó! inc = ${inc++}`) // color is hardcoded!!

    Screen.draw_texture(texture1, x, y)

    Screen.end_drawing()
    Screen.swap_buffers()
    Screen.wait_vblank()

    if (pads.check(pads.START) || pads.check(pads.POWER)) {
        console.log('Closing app...\n'); // show the message on stdout.

        Screen.free_texture(texture1);

        os.clearInterval(interval); // it closes the app.
    }
}, 0);
