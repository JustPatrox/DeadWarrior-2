package {
    import flash.display.*;
    import flash.events.*;
    import flash.geom.*;
    import flash.filters.BitmapFilter;
    import flash.net.*;
    import flash.utils.*;

    public class CastleObserver extends Sprite {
        private var reader:URLLoader=new URLLoader();
        private var writer:URLLoader;
        private var clock:Timer=new Timer(50);
        private var base:String;
        private var busy:Boolean=false;
        private var filterBounds:BitmapData=new BitmapData(4096,4096,true,0);
        public function CastleObserver() {
            mouseEnabled=false;mouseChildren=false;
            base=root.loaderInfo.url.substr(0,root.loaderInfo.url.lastIndexOf("/")+1);
            reader.addEventListener(Event.COMPLETE,command);
            clock.addEventListener(TimerEvent.TIMER,poll);clock.start();
        }
        private function poll(e:Event):void {
            if(busy) return;busy=true;reader.load(new URLRequest(base+"next?t="+getTimer()));
        }
        private function pose(mc:MovieClip):String {
            var result:String=mc.currentFrame+"/"+mc.totalFrames+"[";
            for(var i:int=0;i<mc.numChildren;i++) {
                var d:DisplayObject=mc.getChildAt(i);
                if(d is MovieClip) result+=pose(MovieClip(d));
                else result+="s;";
            }
            return result+"]";
        }
        private function bounds(d:DisplayObject):Rectangle {
            var r:Rectangle=null;
            if(d is DisplayObjectContainer) {
                var container:DisplayObjectContainer=DisplayObjectContainer(d);
                for(var i:int=0;i<container.numChildren;i++) {
                    var child:Rectangle=bounds(container.getChildAt(i));
                    r=r==null ? child : r.union(child);
                }
            }
            if(r==null) r=d.getBounds(d);
            if(r.width>0 && r.height>0) for each(var f:Object in d.filters) {
                var expanded:Rectangle=filterBounds.generateFilterRect(new Rectangle(0,0,r.width,r.height),BitmapFilter(f));
                r=new Rectangle(r.x+expanded.x,r.y+expanded.y,expanded.width,expanded.height);
            }
            var matrix:Matrix=d.transform.matrix;
            var a:Point=matrix.transformPoint(new Point(r.left,r.top));
            var b:Point=matrix.transformPoint(new Point(r.right,r.top));
            var c:Point=matrix.transformPoint(new Point(r.left,r.bottom));
            var e:Point=matrix.transformPoint(new Point(r.right,r.bottom));
            var x:Number=Math.min(a.x,b.x,c.x,e.x);
            var y:Number=Math.min(a.y,b.y,c.y,e.y);
            return new Rectangle(x,y,Math.max(a.x,b.x,c.x,e.x)-x,Math.max(a.y,b.y,c.y,e.y)-y);
        }
        private function command(e:Event):void {
            busy=false;var text:String=String(reader.data);if(!text.length) return;
            var p:Array=text.split(",");var request:URLRequest;
            try {
                if(p[0]=="blend") {
                    var result:String="";
                    var sources:Array=[0xffc82496,0x804080c0,0x400000ff,0x00abcdef];
                    var destinations:Array=[0xff143278,0x806428c8,0x00000000];
                    var modes:Array=["normal","add","overlay","screen"];
                    for each(var mode:String in modes) for each(var source:uint in sources) for each(var destination:uint in destinations) {
                        var src:BitmapData=new BitmapData(1,1,true,source);
                        var dst:BitmapData=new BitmapData(1,1,true,destination);
                        dst.draw(src,null,null,mode);
                        result+=mode+","+source.toString(16)+","+destination.toString(16)+","+dst.getPixel32(0,0).toString(16)+"\n";
                        src.dispose();dst.dispose();
                    }
                    request=new URLRequest(base+"blend");request.data=result;
                    request.method=URLRequestMethod.POST;writer=new URLLoader();writer.load(request);return;
                }
                var baseType:Class=getDefinitionByName("baseMc") as Class;
                var util:Class=getDefinitionByName("com.brockw.game::Util") as Class;
                var castle:Object=new baseType();
                if(p[0]=="snapshot" || p[0]=="base" || p[0]=="component") {
                    util.animateToNeutral(castle,4);
                    var names:Array=["magicShopMc","templeMc","bankMc","siegeShop","barracksMc","archeryRangeMc"];
                    if(p[0]=="base") for each(var hiddenName:String in names) castle[hiddenName].visible=false;
                    for(var tick:int=0;tick<int(p[2]);tick++) for each(var name:String in names) {
                        var button:MovieClip=castle[name] as MovieClip;
                        var wanted:int=name==p[1] ? 2 : 1;
                        if(button.currentFrame!=wanted) {button.gotoAndStop(wanted);util.animateToNeutral(button,-1);}
                        util.animateMovieClip(button,0,-1);
                    }
                    var rectangle:Rectangle=p[0]=="component" ? new Rectangle(-300,-350,650,850) : new Rectangle(-200,-100,1400,900);
                    var left:int=Math.floor(rectangle.left)-2;
                    var top:int=Math.floor(rectangle.top)-2;
                    var width:int=Math.ceil(rectangle.right)-left+2;
                    var height:int=Math.ceil(rectangle.bottom)-top+2;
                    var picture:BitmapData=new BitmapData(width,height,true,0);
                    picture.draw(p[0]=="component" ? castle[p[1]] : castle,new Matrix(1,0,0,1,-left,-top));
                    var raw:ByteArray=picture.getPixels(picture.rect);picture.dispose();raw.compress();
                    var packed:ByteArray=new ByteArray();packed.writeInt(left);packed.writeInt(top);packed.writeInt(width);packed.writeInt(height);packed.writeBytes(raw);
                    request=new URLRequest(base+"snapshot");request.contentType="application/octet-stream";request.data=packed;
                    request.method=URLRequestMethod.POST;writer=new URLLoader();writer.load(request);return;
                }
                var b:MovieClip=castle[p[1]] as MovieClip;
                b.gotoAndStop(int(p[2]));util.animateToNeutral(b,-1);
                if(p[0]=="details") {
                    var details:String="root filters="+b.filters.length+" blend="+b.blendMode+" alpha="+b.alpha+" color="+b.transform.colorTransform+"\n";
                    for(var detailIndex:int=0;detailIndex<b.numChildren;detailIndex++) {
                        var node:DisplayObject=b.getChildAt(detailIndex);
                        details+=detailIndex+" filters="+node.filters.length+" mask="+(node.mask==null ? "none" : node.mask.name)+" blend="+node.blendMode+" direct="+node.getBounds(b)+" calculated="+bounds(node)+"\n";
                    }
                    request=new URLRequest(base+"details");request.data=details;request.method=URLRequestMethod.POST;writer=new URLLoader();writer.load(request);return;
                }
                if(p[0]=="layers") {
                    var description:String=b.numChildren+"\n";
                    for(var layerIndex:int=0;layerIndex<b.numChildren;layerIndex++) {
                        var item:DisplayObject=b.getChildAt(layerIndex);
                        description+=layerIndex+"|"+item.blendMode+"|"+item.name+"\n";
                    }
                    request=new URLRequest(base+"layers");request.data=description;
                    request.method=URLRequestMethod.POST;writer=new URLLoader();writer.load(request);return;
                }
                var selected:DisplayObject=null;
                if(p.length>7 && int(p[7])>=0) {
                    selected=b.getChildAt(int(p[7]));
                    for(var hidden:int=0;hidden<b.numChildren;hidden++) if(b.getChildAt(hidden)!=selected) b.getChildAt(hidden).visible=false;
                    selected.blendMode=BlendMode.NORMAL;
                }
                var seen:Dictionary=new Dictionary();var index:int=0;var prefix:int=-1;var period:int=0;
                var output:ByteArray=new ByteArray();output.writeInt(0);output.writeInt(0);output.writeInt(0);
                while(index<4000) {
                    util.animateMovieClip(b,0,-1);
                    var key:String=selected==null ? pose(b) : selected is MovieClip ? pose(MovieClip(selected)) : "static";
                    if(seen[key]!==undefined) {prefix=int(seen[key]);period=index-prefix;break;}
                    seen[key]=index;
                    if(p[0]=="bake") {
                        var rect:Rectangle=selected==null ? b.getBounds(b) : bounds(selected);
                        var bx:int=Math.floor(rect.left)-2;
                        var by:int=Math.floor(rect.top)-2;
                        var bw:int=Math.max(1,Math.ceil(rect.right)-bx+2);
                        var bh:int=Math.max(1,Math.ceil(rect.bottom)-by+2);
                        var bitmap:BitmapData=new BitmapData(bw,bh,true,0);
                        bitmap.draw(b,new Matrix(1,0,0,1,-bx,-by));
                        var pixels:ByteArray=bitmap.getPixels(bitmap.rect);bitmap.dispose();pixels.compress();
                        output.writeInt(bx);output.writeInt(by);output.writeInt(bw);output.writeInt(bh);
                        output.writeUnsignedInt(pixels.length);output.writeBytes(pixels);
                    }
                    index++;
                }
                if(prefix<0) throw new Error("Castle state did not repeat within 4000 source updates");
                output.position=0;output.writeInt(prefix);output.writeInt(period);output.writeInt(index);
                request=new URLRequest(base+"frames");request.contentType="application/octet-stream";request.data=output;
            } catch(error:Error) {request=new URLRequest(base+"error");request.data=error.toString();}
            request.method=URLRequestMethod.POST;writer=new URLLoader();writer.load(request);
        }
    }
}
