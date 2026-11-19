package {
    import flash.display.*;
    import flash.events.*;
    import flash.geom.*;
    import flash.net.Socket;
    import flash.system.Security;
    import flash.utils.*;

    // GameScreen drives updates with a Timer and sets stage.frameRate to zero.
    // A preconnected loopback socket can return pixels between timeline frames.
    public class BattleObserver extends Sprite {
        private var socket:Socket=new Socket();
        private var clock:Timer=new Timer(220);
        private var widthPx:int;
        private var heightPx:int;
        public function BattleObserver() {
            mouseEnabled=false;
            mouseChildren=false;
            var args:Object=root.loaderInfo.parameters;
            var port:int=int(args.tcp);
            widthPx=int(args.w); heightPx=int(args.h);
            Security.loadPolicyFile("xmlsocket://127.0.0.1:"+port);
            socket.addEventListener(Event.CONNECT,ready);
            socket.connect("127.0.0.1",port);
        }
        private function ready(e:Event):void {
            // USER32 posts a leave when the physical cursor is outside the
            // automated window, even though its synthetic pointer is inside.
            stage.addEventListener(Event.MOUSE_LEAVE,leave,false,10000);
            clock.addEventListener(TimerEvent.TIMER,sample);
            clock.start();
            send("ready",null);
        }
        private function leave(e:Event):void {
            if(stage.mouseX>=0 && stage.mouseX<850 && stage.mouseY>=0 && stage.mouseY<700)
                e.stopImmediatePropagation();
        }
        private function send(header:String,pixels:ByteArray):void {
            socket.writeUTF(header);
            socket.writeUnsignedInt(pixels==null?0:pixels.length);
            if(pixels!=null) socket.writeBytes(pixels);
            socket.flush();
        }
        private function sample(e:Event):void {
            if(!socket.connected) return;
            try {
                if(!DisplayObjectContainer(root).numChildren) return;
                var app:Object=DisplayObjectContainer(root).getChildAt(0);
                if(!("currentScreen" in app)) return;
                var request:int=socket.bytesAvailable ? socket.readUnsignedByte() : 0;
                if((request>=66 && request<=70) || request==72) {
                    app.campaign.currentLevel=request==72 ? 6 : request-65;
                    app.campaign.campaignPoints=app.campaign.currentLevel;
                    app.campaign.difficultyLevel=1;
                    app.showScreen("campaignGameScreen");
                    return;
                }
                var page:String=app.currentScreen();
                var frame:int=0;
                var camera:int=0;
                var selected:int=0;
                var step:String="";
                var message:String="";
                var details:String="";
                var actors:String="";
                if(page=="postGame") details=app.campaign.currentLevel+","+app.campaign.campaignPoints;
                if(page=="campaignGameScreen") {
                    var gs:Object=app.getScreen(page);
                    // Prepared death fixture isolates CampaignCutScene1. It does not
                    // provide evidence of battle balance or a naturally won mission.
                    if(request==71 && app.campaign.currentLevel==5)
                        for each(var fixtureUnit:Object in gs.team.enemyTeam.units)
                            if(fixtureUnit.type==19) fixtureUnit.health=0;
                    // Prepared actor fixture exercises the original throw and
                    // its target. It does not establish natural battle balance.
                    if(request==73 && app.campaign.currentLevel==6) {
                        var guest:Object=gs.game.unitFactory.getUnit(9);
                        gs.team.spawn(guest,gs.game);guest.px=2500;guest.py=gs.game.map.height/2;
                        gs.team.population+=guest.population;gs.team.attack(true);
                        var rival:Object=gs.game.unitFactory.getUnit(19);
                        gs.team.enemyTeam.spawn(rival,gs.game);rival.px=2900;rival.py=gs.game.map.height/2;
                        gs.team.enemyTeam.population+=rival.population;
                        app.stickWar.screenX=app.stickWar.targetScreenX=2200;
                    }
                    camera=int(app.stickWar.screenX);
                    frame=app.stickWar.frame;
                    selected=gs.userInterface.selectedUnits.selected.length;
                    details=gs.team.gold+","+gs.team.mana+","+gs.team.population+","+gs.team.statue.health+","+gs.team.enemyTeam.statue.health;
                    if(request==83) {
                        for each(var team:Object in [gs.team,gs.team.enemyTeam]) {
                            for each(var unit:Object in team.units) {
                                var body:Object=unit.mc;
                                var pose:String=body.currentLabel;
                                var animationFrame:int=("mc" in body && body.mc!=null) ? body.mc.currentFrame : 0;
                                actors+=team.id+","+unit.type+","+int(unit.px)+","+int(unit.py)+","+int(unit.health)+
                                        ","+unit.pwidth+","+unit.pheight+","+animationFrame+","+
                                        int("bowAngle" in unit ? unit.bowAngle : 0)+","+pose+";";
                            }
                        }
                    }
                    for(var i:int=0;i<gs.numChildren;i++) {
                        var child:Object=gs.getChildAt(i);
                        if(getQualifiedClassName(child)=="com.brockw.stickwar.campaign::InGameMessage") {
                            var box:Object=child.getChildAt(0);
                            step=box.step.text; message=box.text.text;
                        }
                    }
                }
                var data:ByteArray=null;
                if(request==83) {
                    var bitmap:BitmapData=new BitmapData(widthPx,heightPx,false,0xffffff);
                    bitmap.draw(root,new Matrix(heightPx/700,0,0,heightPx/700,0,0));
                    data=bitmap.getPixels(bitmap.rect); bitmap.dispose(); data.compress();
                }
                send(page+"|"+frame+"|"+camera+"|"+selected+"|"+step+"|"+message+"|"+details+"|"+actors,data);
            } catch(problem:Error) {
                send("error|"+problem.toString(),null);
            }
        }
    }
}
