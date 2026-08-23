import { Game } from "./game.js";

// PSVitaJS - App + Dialog + Power + Game demo
// Put this file as: src/assets/main.js
//
// The game lives in src/assets/game.js and is imported as an ES module.
//
// SYSTEM VIEW:
//   CROSS      - native IME keyboard (change player name)
//   TRIANGLE   - native MessageDialog with battery/clocks
//   SQUARE     - switch to the game
//   SELECT     - ask for suspend
//   START      - ask for exit
//   L1 + R1    - abort currently active native dialog
//
// GAME VIEW:
//   Left stick / D-pad - move
//   CROSS              - boost
//   START              - restart game
//   CIRCLE             - return to system view

const bg=Audio.open_stream("app0:/assets/bg.wav");
Audio.play_stream(bg,.5,true);

const font = Font.load_font_file("app0:/assets/segoeui.ttf");

const WHITE  = 0xffffffff;
const MUTED  = rgba(165, 175, 190);
const GREEN  = rgba(90, 220, 135);
const YELLOW = rgba(255, 205, 80);
const RED    = rgba(245, 95, 95);
const BLUE   = rgba(90, 155, 255);
const PANEL  = rgba(28, 34, 44);

let running = true;
let view = "system";
let playerName = "Player";

let launchParams = "";
let lastEvent = "none";
let statusMessage = "Ready";

let clocks = null;
let quickPower = null;

let lastPowerRefresh = 0;
let pendingDialogAction = null;
let lastFrameTime = Date.now();

// Vita3K currently has some AppMgr calls stubbed. A stub may report
// "success" repeatedly even though no real event was delivered.
let appEventPollingEnabled = true;
let lastPolledAppEventRaw = null;
let repeatedAppEventCount = 0;

const previousButtons = new Map();

function rgba(r, g, b, a = 255) {
    return (
        ((a & 255) << 24) |
        ((b & 255) << 16) |
        ((g & 255) << 8) |
        (r & 255)
    ) >>> 0;
}

function text(x, y, size, value, color = WHITE) {
    Font.font_draw_text(
        font,
        x,
        y,
        color,
        size,
        String(value)
    );
}

function justPressed(button) {
    const pressed = Pads.check(button);
    const previous = previousButtons.get(button) === true;

    previousButtons.set(button, pressed);

    return pressed && !previous;
}

function rememberSystemButtons() {
    previousButtons.set(Pads.CROSS, Pads.check(Pads.CROSS));
    previousButtons.set(Pads.TRIANGLE, Pads.check(Pads.TRIANGLE));
    previousButtons.set(Pads.SQUARE, Pads.check(Pads.SQUARE));
    previousButtons.set(Pads.SELECT, Pads.check(Pads.SELECT));
    previousButtons.set(Pads.START, Pads.check(Pads.START));
}

function refreshPower(force = false) {
    const now = Date.now();

    if (!force && now - lastPowerRefresh < 1000) {
        return;
    }

    lastPowerRefresh = now;

    try {
        clocks = Power.getClocks();

        quickPower = {
            percent: Power.getBatteryPercent(),
            charging: Power.isCharging(),
            pluggedIn: Power.isPluggedIn(),
            low: Power.isLowBattery()
        };
    } catch (error) {
        statusMessage = "Power error: " + error;
    }
}

function openMessage(action, textValue, buttons = "ok") {
    if (Dialog.status() !== "none") {
        return false;
    }

    try {
        Dialog.message({
            text: textValue,
            buttons
        });

        pendingDialogAction = action;
        return true;
    } catch (error) {
        statusMessage = "Dialog error: " + error;
        return false;
    }
}

function openKeyboard() {
    if (Dialog.status() !== "none") {
        return;
    }

    try {
        Dialog.keyboard({
            title: "Player name",
            initialText: playerName,
            maxLength: 24,
            type: "text",
            withClear: true,
            withCancel: true,
            noAutoCapitalization: false,
            noAssistance: false,
            enterLabel: "default"
        });

        pendingDialogAction = "playerName";
    } catch (error) {
        statusMessage = "IME error: " + error;
    }
}

function showBatteryDialog() {

    if (!quickPower || !clocks) {
        openMessage(
            "batteryInfo",
            "Battery information is currently unavailable.",
            "ok"
        );
        return;
    }

    const message = `
		Battery: ${quickPower.percent}%
		Charging: ${quickPower.charging ? "yes" : "no"}
		AC: ${quickPower.pluggedIn ? "connected" : "disconnected"}
		Low: ${quickPower.low ? "yes" : "no"}
		CPU: ${clocks.cpu} MHz
		BUS: ${clocks.bus} MHz
		GPU: ${clocks.gpu} MHz XBAR: ${clocks.gpuXbar} MHz
		`.trim();

    openMessage("batteryInfo", message, "ok");
}

function handleDialogResult(result) {
    const action = pendingDialogAction;
    pendingDialogAction = null;

    if (!result) {
        return;
    }

    if (action === "playerName") {
        if (
            result.type === "keyboard" &&
            result.confirmed &&
            result.text.trim().length > 0
        ) {
            playerName = result.text.trim();
            statusMessage = "Player name changed to: " + playerName;
        } else {
            statusMessage = "Name change canceled.";
        }

        return;
    }

    if (action === "exit") {
        if (
            result.type === "message" &&
            result.result === "ok" &&
            result.button === "yes"
        ) {
            cleanup();
            App.exit(0);
            return;
        }

        statusMessage = "Exit canceled.";
        return;
    }

    if (action === "suspend") {
        if (
            result.type === "message" &&
            result.result === "ok" &&
            result.button === "yes"
        ) {
            statusMessage = "Requesting suspend...";

            try {
                Power.suspend();
            } catch (error) {
                statusMessage = "Suspend error: " + error;
            }

            return;
        }

        statusMessage = "Suspend canceled.";
        return;
    }

    if (action === "gameWin") {
        statusMessage = "Game finished with 10 points.";
        return;
    }

    if (action === "batteryInfo") {
        statusMessage = "Battery information closed.";
    }
}

function updateNativeDialog() {
    if (Dialog.status() === "none") {
        return;
    }

    try {
        const status = Dialog.update();

        if (status === "finished") {
            const result = Dialog.result();
            handleDialogResult(result);
        }
    } catch (error) {
        statusMessage = "Dialog.update: " + error;
    }
}

function pollAppEvents() {
    if (!appEventPollingEnabled) {
        return;
    }

    let event;

    try {
        // One poll per frame. Never loop on this call because emulator
        // stubs may report success forever.
        event = App.pollEvent();
    } catch (error) {
        appEventPollingEnabled = false;
        statusMessage = "App event polling unavailable: " + error;
        return;
    }

    if (event === null) {
        lastPolledAppEventRaw = null;
        repeatedAppEventCount = 0;
        return;
    }

    if (event.rawType === lastPolledAppEventRaw) {
        repeatedAppEventCount++;

        if (repeatedAppEventCount >= 2) {
            appEventPollingEnabled = false;
            statusMessage =
                "App events disabled: emulator repeated the same event.";
        }

        return;
    }

    lastPolledAppEventRaw = event.rawType;
    repeatedAppEventCount = 0;

    lastEvent = event.type + " (" + event.rawType + ")";

    if (event.type === "resume") {
        statusMessage = "Application resumed.";

        try {
            Power.requestDisplayOn();
        } catch (_) {
            // Vita3K may not implement this yet.
        }

        refreshPower(true);
    } else {
        statusMessage = "App event: " + event.type;
    }
}

function enterGame() {
    Game.enter({
        font,
        playerName
    });

    view = "game";
    statusMessage = "Game started.";
}

function leaveGame() {
    Game.leave();

    view = "system";

    // Synchronize edge-detection state so a button used inside the game
    // does not immediately trigger a system-view action.
    rememberSystemButtons();

    statusMessage = "Returned from game.";
}

function updateSystemInput() {
    if (Dialog.status() !== "none") {
        const abortCombo =
            Pads.check(Pads.L1) &&
            Pads.check(Pads.R1);

        const wasAbortCombo =
            previousButtons.get("abortCombo") === true;

        previousButtons.set("abortCombo", abortCombo);

        if (abortCombo && !wasAbortCombo) {
            try {
                if (Dialog.abort()) {
                    statusMessage = "Native dialog abort requested.";
                }
            } catch (error) {
                statusMessage = "Abort error: " + error;
            }
        }

        rememberSystemButtons();
        return;
    }

    previousButtons.set("abortCombo", false);

    if (justPressed(Pads.CROSS)) {
        openKeyboard();
    }

    if (justPressed(Pads.TRIANGLE)) {
        showBatteryDialog();
    }

    if (justPressed(Pads.SQUARE)) {
        enterGame();
        return;
    }

    if (justPressed(Pads.SELECT)) {
        openMessage(
            "suspend",
            "Suspend the PS Vita now?",
            "yesNo"
        );
    }

    if (justPressed(Pads.START)) {
        openMessage(
            "exit",
            "Exit VitaJS demo?",
            "yesNo"
        );
    }
}

function updateGame(dt) {
    if (Dialog.status() !== "none") {
        return;
    }

    const event = Game.update(dt);

    if (!event) {
        return;
    }

    if (event.type === "back") {
        leaveGame();
        return;
    }

    if (event.type === "win") {
        openMessage(
            "gameWin",
            playerName + " collected 10 orbs!\n\nScore: " + event.score,
            "ok"
        );
    }
}

function drawPowerPanel() {
    Screen.draw_rectangle(
        30,
        105,
        900,
        210,
        PANEL
    );

    text(55, 140, 27, "POWER", BLUE);

    if (!clocks || !quickPower) {
        text(55, 180, 24, "Reading power information...");
        return;
    }

    const batteryColor =
        quickPower.low
            ? RED
            : quickPower.charging
                ? GREEN
                : WHITE;

    text(
        55,
        185,
        30,
        quickPower.percent + "%",
        batteryColor
    );

    text(
        150,
        183,
        23,
        quickPower.charging ? "charging" : "battery",
        MUTED
    );

    text(
        55,
        225,
        22,
        "AC: " + (quickPower.pluggedIn ? "connected" : "no")
    );

    text(
        55,
        260,
        22,
        "Low battery: " + (quickPower.low ? "YES" : "no"),
        quickPower.low ? YELLOW : MUTED
    );

    text(330, 185, 22, "CPU: " + clocks.cpu + " MHz");
    text(330, 225, 22, "BUS: " + clocks.bus + " MHz");
    text(590, 185, 22, "GPU: " + clocks.gpu + " MHz");
    text(590, 225, 22, "XBAR: " + clocks.gpuXbar + " MHz");
}

function drawAppPanel() {
    Screen.draw_rectangle(
        30,
        335,
        900,
        150,
        PANEL
    );

    text(55, 370, 27, "APP", BLUE);
    text(55, 410, 22, "Player: " + playerName);
    text(55, 445, 20, "Last event: " + lastEvent, MUTED);

    const params =
        launchParams.length > 70
            ? launchParams.slice(0, 70) + "..."
            : launchParams;

    text(
        430,
        410,
        20,
        "Launch: " + (params || "(empty)"),
        MUTED
    );
}

function drawSystemView() {
    Screen.clear(14, 17, 23, 255);

    text(30, 55, 34, "VitaJS Native API Demo", WHITE);
    text(30, 85, 20, "App + Dialog + Power + separate game.js", MUTED);

    drawPowerPanel();
    drawAppPanel();

    text(40, 505, 19, "X  keyboard", WHITE);
    text(200, 505, 19, "TRIANGLE  battery", WHITE);
    text(430, 505, 19, "SQUARE  GAME", GREEN);
    text(650, 505, 19, "SELECT  suspend", WHITE);

    text(40, 530, 19, "START  exit", WHITE);
    text(200, 530, 19, "L1+R1  abort dialog", WHITE);

    text(40, 565, 18, statusMessage, MUTED);
}

function render() {
    Screen.start_drawing();

    if (view === "game") {
        Game.render();
    } else {
        drawSystemView();
    }

    Screen.end_drawing();

    updateNativeDialog();

    Screen.swap_buffers();
}

function cleanup() {
    if (!running) {
        return;
    }

    running = false;

    try {
        if (Dialog.status() !== "none") {
            Dialog.abort();
        }
    } catch (_) {
        // Cleanup-only error.
    }

    Game.leave();

    Font.free_font(font);
}

// -------------------------------------------------------------------------
// Initialization
// -------------------------------------------------------------------------

try {
    // Vita3K currently logs sceAppMgrGetAppParam as unimplemented.
    // On real hardware this can contain launch parameters.
    launchParams = App.getLaunchParams();
} catch (_) {
    launchParams = "";
}

refreshPower(true);

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------

const loop = os.setInterval(() => {
    if (!running) {
        os.clearInterval(loop);
        return;
    }

    const now = Date.now();
    const dt = Math.min(0.05, Math.max(0, (now - lastFrameTime) / 1000));
    lastFrameTime = now;

    pollAppEvents();
    refreshPower();

    if (view === "game") {
        updateGame(dt);
    } else {
        updateSystemInput();
    }

    render();
}, 0);
