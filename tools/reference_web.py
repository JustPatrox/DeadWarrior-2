"""Run the unmodified SWF in the reference player at a physical 16:9 viewport."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import threading

from playwright.sync_api import sync_playwright


ROOT = Path(__file__).resolve().parents[1]
HTML = """<!doctype html><meta charset=utf-8><style>
*{margin:0}html,body,ruffle-player{width:100%;height:100%;display:block;overflow:hidden}
</style><script src="/work/tools/reference-web/ruffle.js"></script>
<script>window.addEventListener('DOMContentLoaded',async()=>{
const player=window.RufflePlayer.newest().createPlayer();
document.body.appendChild(player);
player.style.width='100%';player.style.height='100%';
await player.ruffle().load({url:'/stickwar2.swf',autoplay:'on',unmuteOverlay:'hidden',
scale:'showAll',forceScale:true,letterbox:'off',splashScreen:false,allowScriptAccess:false,
preferredRenderer:'canvas',warnOnUnsupportedContent:false});
window.loaded=true;});</script><body></body>"""


def run(width,height,samples,tutorial=False):
    out = ROOT/"evidence/reference"
    out.mkdir(exist_ok=True,parents=True)
    # GameScreen.enter sets stage.frameRate=0 and runs its own simulation
    # timer. Let Ruffle keep presenting frames while recording that screen.
    html = HTML.replace("scale:'showAll'", "frameRate:30,scale:'showAll'") if tutorial else HTML
    (ROOT/"work/player.html").write_text(html,encoding="utf-8")
    handler = partial(SimpleHTTPRequestHandler, directory=ROOT)
    http = ThreadingHTTPServer(("127.0.0.1",0),handler)
    threading.Thread(target=http.serve_forever,daemon=True).start()
    os.environ["PLAYWRIGHT_BROWSERS_PATH"] = str(ROOT/"work/tools/browser")
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(args=["--mute-audio","--disable-background-timer-throttling"])
            page = browser.new_page(viewport=dict(width=width,height=height),device_scale_factor=1)
            diagnostics = []
            def console(message):
                if message.type in ("error","warning") or "Error #" in message.text:
                    diagnostics.append(message.text)
            page.on("console",console)
            page.on("pageerror",lambda error: diagnostics.append(str(error)))
            origin = f"http://127.0.0.1:{http.server_port}"
            page.route("**/*",lambda r: r.continue_() if r.request.url.startswith(origin+"/") else r.abort())
            page.goto(origin+"/work/player.html")
            page.wait_for_function("window.loaded === true",timeout=90000)
            last = 0
            for second in samples:
                page.wait_for_timeout(1000*(second-last))
                page.screenshot(path=out/f"swf-web-{width}x{height}-{second:g}s.png")
                last = second
            if tutorial:
                # CampaignMenuScreen positions relative to the SWF's top-left stage.
                scale = height/700
                for name,x,y,wait in (("new",161,664,2),("difficulty",437,664,2),
                                      ("intro",154,668,5),("map",434,639,6),("tutorial",530,430,20)):
                    page.mouse.click(x*scale,y*scale,delay=160)
                    page.wait_for_timeout(1000*wait)
                    page.screenshot(path=out/f"swf-web-{width}x{height}-{name}.png")
                (ROOT/f"work/reference-{width}-console.log").write_text("\n".join(diagnostics),encoding="utf-8")
            browser.close()
    finally:
        http.shutdown()
        http.server_close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("width",type=int)
    parser.add_argument("height",type=int)
    parser.add_argument("samples",type=float,nargs="+")
    parser.add_argument("--tutorial",action="store_true")
    args = parser.parse_args()
    run(args.width,args.height,sorted(args.samples),args.tutorial)
