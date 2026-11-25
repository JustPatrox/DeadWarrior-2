package {
    import flash.display.*;
    import flash.events.*;
    import flash.geom.*;
    import flash.media.*;
    import flash.net.Socket;
    import flash.system.Security;
    import flash.utils.*;
    public class TrailerObserver extends Sprite {
        private var connection:Socket=new Socket();
        private var movie:MovieClip;
        private var running:Boolean=false;
        private var readySent:Boolean=false;
        private var wide:Boolean=false;
        private var samplesOnly:Boolean=true;
        private var screenFills:Boolean=false;
        private var effectExtensions:Array=[];
        private var effectsPreserved:Boolean=true;
        private var effectAlphas:String="[]";
        private var rawCandidates:String="[]";
        private var rawStageChildren:String="[]";
        private var seen:Object={};
        private var camera:Object=null;
        private var widthOut:int=720;
        private var heightOut:int=480;
        private var selected:Array=[3,150,500,877,1376,1769,2306,2614,2640,2800,3000,3235];
        public function TrailerObserver() {
            mouseEnabled=mouseChildren=false;
            movie=MovieClip(root);
            var args:Object=loaderInfo.parameters;
            wide=args.wide=="1";samplesOnly=args.all!="1";
            screenFills=wide && args.screenfx=="1";
            if(args.frames) {
                selected=[];
                for each(var requested:String in String(args.frames).split(",")) {
                    var requestedFrame:int=int(requested);
                    if(requestedFrame>=3 && requestedFrame<=3235 && selected.indexOf(requestedFrame)<0)selected.push(requestedFrame);
                }
                selected.sort(Array.NUMERIC);
            }
            widthOut=wide?1280:720;heightOut=wide?720:480;
            SoundMixer.soundTransform=new SoundTransform(0);
            Security.loadPolicyFile("xmlsocket://127.0.0.1:"+args.tcp);
            connection.addEventListener(ProgressEvent.SOCKET_DATA,command);
            connection.connect("127.0.0.1",int(args.tcp));
            addEventListener(Event.EXIT_FRAME,sample,false,-10000);
        }
        private function send(header:String,pixels:ByteArray=null):void {
            connection.writeUTF(header);connection.writeUnsignedInt(pixels?pixels.length:0);
            if(pixels)connection.writeBytes(pixels);connection.flush();
        }
        private function command(e:ProgressEvent):void {
            while(connection.bytesAvailable) {
                var code:int=connection.readUnsignedByte();
                if(code==80) {
                    stage.scaleMode=StageScaleMode.NO_SCALE;stage.align=StageAlign.TOP_LEFT;
                    // Keep the original frame clock for reference captures.
                    // Offline all-frame capture removes only the audio stream
                    // clock and advances every authored frame at a slower rate.
                    stage.frameRate=samplesOnly?30:15;
                    if(wide) {movie.scaleX=movie.scaleY=1.5;movie.x=100;movie.y=0;}
                    running=true;movie.gotoAndStop(3);sample(null);movie.play();
                }
            }
        }
        private function findCamera(container:DisplayObjectContainer):Object {
            for(var i:int=0;i<container.numChildren;++i) {
                var child:DisplayObject=container.getChildAt(i);
                if(getQualifiedClassName(child).indexOf("VCam_AS3_134")>=0)return child;
                if(child is DisplayObjectContainer) {
                    var found:Object=findCamera(DisplayObjectContainer(child));if(found)return found;
                }
            }return null;
        }
        private function removeUniformExtensions():Boolean {
            var changed:Boolean=effectExtensions.length>0;
            for each(var extension:DisplayObject in effectExtensions)if(extension.parent)extension.parent.removeChild(extension);
            effectExtensions=[];return changed;
        }
        private function nearBounds(r:Rectangle,x:Number,y:Number,w:Number,h:Number):Boolean {
            // Flash getBounds round-trips through twip-quantized transforms.
            // .1 stage unit still uniquely identifies all four source shapes.
            return Math.abs(r.x-x)<.1 && Math.abs(r.y-y)<.1 && Math.abs(r.width-w)<.1 && Math.abs(r.height-h)<.1;
        }
        private function uniformEffectRect(child:Shape):Object {
            var r:Rectangle=child.getBounds(child);
            // Source SVG/inventory review proves these four definitions contain
            // only one constant-colour rectangle (plus same-colour edge strokes).
            // These signatures deliberately exclude bitmap scenery, actors,
            // loading rectangles, buttons and credit-card highlights.
            if(nearBounds(r,0,0,157.95,59))return {id:52,rect:new Rectangle(0,0,157.95,59)};
            if(nearBounds(r,743.2,-116.75,278.55,750))return {id:30,rect:new Rectangle(743.2,-116.75,278.55,750)};
            if(nearBounds(r,-62.5,35.5,862.95,405.95))return {id:158,rect:new Rectangle(-62,36,861.95,404.95)};
            if(nearBounds(r,-.5,-.5,800.95,368.95))return {id:1160,rect:new Rectangle(0,0,799.95,367.95)};
            return null;
        }
        private function collectUniformEffects(container:DisplayObjectContainer,result:Array):void {
            for(var i:int=0;i<container.numChildren;++i) {
                var child:DisplayObject=container.getChildAt(i);
                if(child is DisplayObjectContainer)collectUniformEffects(DisplayObjectContainer(child),result);
                if(!(child is Shape))continue;
                var info:Object=uniformEffectRect(Shape(child));
                if(info)result.push({display:child,id:info.id,rect:info.rect});
            }
        }
        private function uniformSignature(candidates:Array):String {
            var result:String="";
            for each(var info:Object in candidates) {
                var child:DisplayObject=info.display;
                result+=child.transform.matrix.toString()+child.transform.colorTransform.toString();
            }
            return result;
        }
        private function matrixValues(m:Matrix):String {
            return "["+[m.a,m.b,m.c,m.d,m.tx,m.ty].join(",")+"]";
        }
        private function stageMatrix(child:DisplayObject):Matrix {
            // concatenatedMatrix can expose bitmap-cache registration offsets
            // after VCam's BitmapData.draw(stage). Compose authored local
            // matrices explicitly; never change cache or original properties.
            var result:Matrix=new Matrix();
            var node:DisplayObject=child;
            while(node && node!=stage) {
                var p:Matrix=node.transform.matrix;
                result=new Matrix(p.a*result.a+p.c*result.b,p.b*result.a+p.d*result.b,
                    p.a*result.c+p.c*result.d,p.b*result.c+p.d*result.d,
                    p.a*result.tx+p.c*result.ty+p.tx,p.b*result.tx+p.d*result.ty+p.ty);
                node=node.parent;
            }
            return result;
        }
        private function throughRect(r:Rectangle,m:Matrix):Rectangle {
            var p:Point=m.transformPoint(new Point(r.left,r.top));
            var q:Point=m.transformPoint(new Point(r.right,r.bottom));
            return new Rectangle(Math.min(p.x,q.x),Math.min(p.y,q.y),Math.abs(q.x-p.x),Math.abs(q.y-p.y));
        }
        private function originalEffectiveAlpha(child:DisplayObject):Number {
            var value:Number=255;
            var node:DisplayObject=child;
            while(node && node!=stage) {
                var colour:ColorTransform=node.transform.colorTransform;
                value=Math.max(0,Math.min(255,value*colour.alphaMultiplier+colour.alphaOffset));
                node=node.parent;
            }
            return value/255;
        }
        private function rawShapeCandidates(container:DisplayObjectContainer,rows:Array,path:String):void {
            for(var i:int=0;i<container.numChildren && rows.length<64;++i) {
                var child:DisplayObject=container.getChildAt(i);
                if(child is DisplayObjectContainer)rawShapeCandidates(DisplayObjectContainer(child),rows,path+"/"+i);
                if(!(child is Shape) && !(child is Bitmap))continue;
                var r:Rectangle=child.getBounds(child);
                if(r.width<150 || r.height<50)continue;
                var matched:Object=child is Shape?uniformEffectRect(Shape(child)):null;
                rows.push('{"path":"'+path+"/"+i+'","kind":"'+getQualifiedClassName(child)+
                    '","bounds":['+[r.x,r.y,r.width,r.height].join(",")+'],"matrix":'+matrixValues(child.transform.matrix)+
                    ',"parentMatrix":'+matrixValues(child.parent.transform.concatenatedMatrix)+
                    ',"globalMatrix":'+matrixValues(child.transform.concatenatedMatrix)+
                    ',"parentLocalMatrix":'+matrixValues(child.parent.transform.matrix)+
                    ',"manualParentMatrix":'+matrixValues(stageMatrix(child.parent))+
                    ',"manualGlobalMatrix":'+matrixValues(stageMatrix(child))+
                    ',"alpha":'+child.alpha+',"matched":'+(matched?matched.id:0)+
                    ',"parentCached":'+child.parent.cacheAsBitmap+',"visible":'+child.visible+'}');
            }
        }
        private function readRawCandidates():void {
            var rows:Array=[];rawShapeCandidates(movie,rows,"root");rawCandidates="["+rows.join(",")+"]";
            var children:Array=[];
            for(var i:int=0;i<stage.numChildren;++i) {
                var child:DisplayObject=stage.getChildAt(i);
                children.push('{"kind":"'+getQualifiedClassName(child)+'","visible":'+child.visible+
                    ',"x":'+child.x+',"y":'+child.y+',"width":'+child.width+',"height":'+child.height+'}');
            }
            rawStageChildren="["+children.join(",")+"]";
        }
        private function uniformRectangleGraphics(graphics:Graphics,id:int,r:Rectangle):void {
            // Exact constant-colour rectangle paths from source SVG definitions
            // 30/52/158/1160. Timeline-owned Shape.graphics does not expose its
            // native tag geometry to copyFrom. These paths are used only for
            // the missing lateral field, never for original picture artwork.
            graphics.beginFill(id==30?0:0xffffff);
            graphics.moveTo(r.right,r.bottom);graphics.lineTo(r.left,r.bottom);
            graphics.lineTo(r.left,r.top);graphics.lineTo(r.right,r.top);graphics.lineTo(r.right,r.bottom);
            graphics.endFill();
            if(id==158 || id==1160) {
                graphics.lineStyle(1,0xffffff,1,false,LineScaleMode.NORMAL,CapsStyle.ROUND,JointStyle.ROUND);
                graphics.moveTo(r.right,r.bottom);graphics.lineTo(r.left,r.bottom);
                graphics.lineTo(r.left,r.top);graphics.lineTo(r.right,r.top);graphics.lineTo(r.right,r.bottom);
            }
        }
        private function widenUniformEffects(candidates:Array):Boolean {
            var changed:Boolean=false;
            var fields:Array=[];
            for each(var info:Object in candidates) {
                var child:Shape=Shape(info.display);
                var r:Rectangle=info.rect;
                var original:Matrix=child.transform.matrix;
                var parentMatrix:Matrix=stageMatrix(child.parent);
                var global:Matrix=stageMatrix(child);
                // Only axis-aligned, full-original-content fields qualify.
                // Keep intentional partial wipes and already-wide VCam fields.
                if(Math.abs(parentMatrix.b)>1e-9 || Math.abs(parentMatrix.c)>1e-9 ||
                   Math.abs(global.b)>1e-9 || Math.abs(global.c)>1e-9 || Math.abs(parentMatrix.a)<1e-9)continue;
                var p:Point=global.transformPoint(new Point(r.left,r.top));
                var q:Point=global.transformPoint(new Point(r.right,r.bottom));
                var left:Number=Math.min(p.x,q.x);
                var right:Number=Math.max(p.x,q.x);
                var top:Number=Math.min(p.y,q.y);
                var bottom:Number=Math.max(p.y,q.y);
                if(left>100 || right<1180 || top>127.5 || bottom<592.5)continue;
                var effectiveAlpha:Number=originalEffectiveAlpha(child);
                fields.push('{"id":'+info.id+',"alpha":'+child.alpha+',"effectiveAlpha":'+
                    effectiveAlpha+'}');
                if(effectiveAlpha<=0)continue;
                var painted:Rectangle=throughRect(child.getBounds(child),global);
                if(painted.left<=0 && painted.right>=1280)continue;
                var anchor:Number=(640-parentMatrix.tx)/parentMatrix.a;
                var expanded:Matrix=original.clone();
                expanded.a*=32/27;expanded.tx=anchor+(original.tx-anchor)*32/27;
                // Never assign properties on a timeline-owned Shape: Adobe
                // then stops its authored colour/alpha tween. Copy its exact
                // graphics into a separate, adjacent layer and expose only the
                // currently missing lateral pieces. Existing actor/matte order,
                // y geometry, colour, alpha, filters and blend mode stay intact.
                var holder:Sprite=new Sprite();holder.mouseEnabled=holder.mouseChildren=false;
                var copy:Shape=new Shape();uniformRectangleGraphics(copy.graphics,int(info.id),r);
                copy.transform.matrix=expanded;copy.transform.colorTransform=child.transform.colorTransform;
                copy.filters=child.filters;copy.blendMode=child.blendMode;
                var clip:Shape=new Shape();clip.graphics.beginFill(0);
                if(painted.left>0)clip.graphics.drawRect(0,0,Math.min(100,painted.left),720);
                if(painted.right<1280)clip.graphics.drawRect(Math.max(1180,painted.right),0,1280-Math.max(1180,painted.right),720);
                clip.graphics.endFill();
                var inverse:Matrix=parentMatrix.clone();inverse.invert();clip.transform.matrix=inverse;
                holder.addChild(copy);holder.addChild(clip);copy.mask=clip;
                child.parent.addChildAt(holder,child.parent.getChildIndex(child)+1);
                effectExtensions.push(holder);
                changed=true;
            }
            effectAlphas="["+fields.join(",")+"]";
            return changed;
        }
        private function captureFrame(frame:int,variant:String=""):Boolean {
            try {
                // This source is SWF 10: newer Stage.color/JSON APIs must
                // not be introduced into its original API-version domain.
                var image:BitmapData=new BitmapData(widthOut,heightOut,false,wide?0:0xffffff);
                image.draw(stage);
                var pixels:ByteArray=image.getPixels(image.rect);pixels.compress();image.dispose();
                var header:String='{"frame":'+frame+',"width":'+widthOut+',"height":'+heightOut+
                    ',"movieX":'+movie.x+',"movieY":'+movie.y+',"scaleX":'+movie.scaleX+
                    ',"scaleY":'+movie.scaleY+',"rotation":'+movie.rotation+
                    ',"wide":'+wide+',"camera":'+(camera!=null)+',"variant":"'+variant+'"'+
                    ',"screenEffectsPreserved":'+effectsPreserved+',"screenEffectAlphas":'+effectAlphas+
                    ',"screenExtensionCount":'+effectExtensions.length;
                if(samplesOnly)header+=',"rawShapeCandidates":'+rawCandidates+',"rawStageChildren":'+rawStageChildren;
                header+='}';
                send(header,pixels);return true;
            } catch(error:Error) {running=false;send("error|"+frame+"|"+error.toString());return false;}
        }
        private function sample(e:Event):void {
            if(!connection.connected)return;
            if(!running) {
                if(!readySent && movie.currentFrame==2){readySent=true;send("ready");}
                return;
            }
            var frame:int=movie.currentFrame;
            if(camera && DisplayObject(camera).stage==null)camera=null;
            if(wide && camera==null) {
                movie.scaleX=movie.scaleY=1.5;movie.x=100;movie.y=0;
            }
            if(wide)for(var i:int=0;i<movie.numChildren;++i) {
                var child:DisplayObject=movie.getChildAt(i);
                var bounds:Rectangle=child.getBounds(movie);
                // Source shape 18 is only black with transparent content, not
                // picture artwork. Extend that authored matte horizontally.
                if(child is Shape && Math.abs(bounds.x+120)<.1 && Math.abs(bounds.width-959.9)<.1 && Math.abs(bounds.height-485.95)<.1) {
                    child.scaleX=32/27;child.x=360*(1-32/27);
                }
            }
            if(frame>=2614 && frame<2921 && camera==null) {
                camera=findCamera(movie);
                if(camera && wide) {
                    // Preserve the original vertical field and scale while
                    // revealing the new horizontal sides. Authored camera
                    // motion, rotation, filters and color remain unchanged.
                    camera.camW*=32/27;
                    camera.sW=widthOut;camera.sH=heightOut;
                    camera.myBitmapData.dispose();
                    camera.myBitmapData=new BitmapData(widthOut,heightOut,true,0);
                    camera.bmp.bitmapData=camera.myBitmapData;
                    camera.camControl();
                }
            }
            if(screenFills && removeUniformExtensions() && camera)camera.camControl();
            var candidates:Array=[];
            if(screenFills)collectUniformEffects(movie,candidates);
            var originalSignature:String=uniformSignature(candidates);
            effectsPreserved=true;effectAlphas="[]";
            var wanted:Boolean=frame>=3 && !seen[frame] && (!samplesOnly || selected.indexOf(frame)>=0);
            if(wanted && samplesOnly)readRawCandidates();
            if(wanted && screenFills && samplesOnly)if(!captureFrame(frame,"before"))return;
            if(screenFills && widenUniformEffects(candidates) && camera)camera.camControl();
            effectsPreserved=originalSignature==uniformSignature(candidates);
            if(!effectsPreserved){running=false;send("error|"+frame+"|Original effect properties changed");return;}
            if(wanted) {seen[frame]=true;if(!captureFrame(frame,screenFills?"after":""))return;}
            if(frame==3235) {running=false;send("done");}
        }
    }
}
