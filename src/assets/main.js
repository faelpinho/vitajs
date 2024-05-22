
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
    Screen.clear(0, 0, 0, 0) // rgba

    //Font.print(font, `Hello world menó! inc = ${inc++}`, 50, 50, 20) // font, str, x, y, sz, r, g, b, a.

    Screen.draw_texture(texture1, x, y)

    Screen.end_drawing()
    Screen.swap_buffers()
    Screen.wait_vblank()

    if (pads.check(pads.START) || pads.check(pads.POWER)) {
        console.log('Closing app...\n'); // show the message on stdout.

        os.clearInterval(interval); // it closes the app.
    }
}, 0);
