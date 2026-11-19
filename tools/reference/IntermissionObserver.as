package {
    import flash.display.*;
    import flash.events.*;
    import flash.geom.*;
    import flash.net.*;
    import flash.utils.*;

    // Test-only child of the original document. The screen and its input
    // handlers are the ones constructed by stickwar2, running in Adobe Player.
    public class IntermissionObserver extends Sprite {
        private var clock:Timer = new Timer(150);
        private var reader:URLLoader = new URLLoader();
        private var writer:URLLoader;
        private var busy:Boolean = false;
        private var base:String;
        public function IntermissionObserver() {
            mouseEnabled=false;
            mouseChildren=false;
            base=root.loaderInfo.url.substr(0,root.loaderInfo.url.lastIndexOf("/")+1);
            reader.addEventListener(Event.COMPLETE,onCommand);
            reader.addEventListener(IOErrorEvent.IO_ERROR,onError);
            clock.addEventListener(TimerEvent.TIMER,poll);
            clock.start();
        }
        private function onError(e:Event):void {busy=false;}
        private function poll(e:Event):void {
            if (busy) return;
            busy=true;
            reader.load(new URLRequest(base+"next?t="+getTimer()));
        }
        private function prepare(app:Object):void {
            var gameType:Class=getDefinitionByName("com.brockw.stickwar.engine::StickWar") as Class;
            var randomType:Class=getDefinitionByName("com.brockw.random::Random") as Class;
            var game:Object=new gameType(app,app.getScreen("campaignGameScreen"));
            game.xml=app.xml;
            game.units=new Dictionary();
            game.random=new randomType(0);
            app.stickWar=game;
            app.campaign.currentLevel=1;
            app.campaign.campaignPoints=1;
            app.campaign.difficultyLevel=1;
            app.showScreen("campaignUpgradeScreen");
        }
        private function onCommand(e:Event):void {
            busy=false;
            var command:String=String(reader.data);
            if (!command.length) return;
            var parts:Array=command.split(",");
            var request:URLRequest;
            try {
                var app:Object=DisplayObjectContainer(root).getChildAt(0);
                if (parts[0]=="prepare") {
                    prepare(app);
                    request=new URLRequest(base+"prepared");
                    request.data="ok";
                } else {
                    var width:int=int(parts[1]);
                    var height:int=int(parts[2]);
                    var bitmap:BitmapData=new BitmapData(width,height,false,0xffffff);
                    bitmap.draw(root,new Matrix(height/700,0,0,height/700,0,0));
                    var pixels:ByteArray=bitmap.getPixels(bitmap.rect);
                    bitmap.dispose();
                    pixels.compress();
                    request=new URLRequest(base+"frame/"+parts[0]+"?screen="+app.currentScreen()+"&points="+app.campaign.campaignPoints);
                    request.contentType="application/octet-stream";
                    request.data=pixels;
                }
            } catch(problem:Error) {
                request=new URLRequest(base+"error");
                request.data=problem.toString();
            }
            request.method=URLRequestMethod.POST;
            writer=new URLLoader(); writer.load(request);
        }
    }
}
