package {
    import flash.display.*;
    import flash.events.*;
    import flash.geom.Matrix;
    import flash.net.*;
    import flash.utils.*;

    public class TooltipObserver extends Sprite {
        private var reader:URLLoader=new URLLoader();
        private var writer:URLLoader;
        private var clock:Timer=new Timer(150);
        private var base:String;
        private var busy:Boolean=false;
        public function TooltipObserver() {
            mouseEnabled=false;mouseChildren=false;
            base=root.loaderInfo.url.substr(0,root.loaderInfo.url.lastIndexOf("/")+1);
            reader.addEventListener(Event.COMPLETE,command);
            clock.addEventListener(TimerEvent.TIMER,poll);clock.start();
        }
        private function poll(e:Event):void {
            if(busy) return;busy=true;reader.load(new URLRequest(base+"next?t="+getTimer()));
        }
        private function command(e:Event):void {
            busy=false;var text:String=String(reader.data);if(!text.length) return;
            var parts:Array=text.split(",");var request:URLRequest;
            try {
                var app:Object=DisplayObjectContainer(root).getChildAt(0);
                var gameClass:Class=getDefinitionByName("com.brockw.stickwar.engine::StickWar") as Class;
                var tipClass:Class=getDefinitionByName("com.brockw.stickwar.engine::TipBox") as Class;
                var game:Object=new gameClass(app,app.getScreen("campaignGameScreen"));game.xml=app.xml;
                var tip:Object=new tipClass(game);var item:XMLList=app.xml.xml.Order.Units[parts[0]];
                if(parts[0]=="castle") {
                    item=app.xml.xml.Order.Tech.castleArchers1;
                    tip.displayTip(item.child("name").toString(),item.tip,int(item.time),int(item.cost),int(item.mana),0);
                } else tip.displayTip(item.child("name").toString(),item.info,int(item.cooldown),int(item.gold),int(item.mana),int(item.population));
                var width:int=int(parts[1]);
                var height:int=int(parts[2]);
                var bitmap:BitmapData=new BitmapData(width,height,false,0x666666);
                bitmap.draw(tip,new Matrix(height/700,0,0,height/700,0,0));
                var pixels:ByteArray=bitmap.getPixels(bitmap.rect);bitmap.dispose();pixels.compress();
                request=new URLRequest(base+"frame/"+parts[0]+"?title="+encodeURIComponent(tip.toolBox.title.text));request.contentType="application/octet-stream";request.data=pixels;
            } catch(error:Error) {request=new URLRequest(base+"error");request.data=error.toString();}
            request.method=URLRequestMethod.POST;writer=new URLLoader();writer.load(request);
        }
    }
}
