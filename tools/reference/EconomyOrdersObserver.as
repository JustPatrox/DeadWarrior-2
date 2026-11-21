package {
    import flash.display.*;
    import flash.events.*;
    import flash.geom.*;
    import flash.net.Socket;
    import flash.system.Security;
    import flash.utils.*;

    // Prepared original-controller tests; never changes the root source SWF.
    public class EconomyOrdersObserver extends Sprite {
        private var socket:Socket=new Socket();
        private var clock:Timer=new Timer(220);
        private var widthPx:int;
        private var heightPx:int;
        private var formation:Array=[];
        private var depletion:Object=null;
        private var workers:Array=[];
        private var target:Object=null;
        public function EconomyOrdersObserver() {
            mouseEnabled=mouseChildren=false;
            var args:Object=root.loaderInfo.parameters;
            widthPx=int(args.w);heightPx=int(args.h);
            Security.loadPolicyFile("xmlsocket://127.0.0.1:"+args.tcp);
            socket.addEventListener(Event.CONNECT,function(e:Event):void {
                send("ready",null);clock.addEventListener(TimerEvent.TIMER,sample);clock.start();
            });
            socket.connect("127.0.0.1",int(args.tcp));
        }
        private function send(header:String,pixels:ByteArray):void {
            socket.writeUTF(header);socket.writeUnsignedInt(pixels==null?0:pixels.length);
            if(pixels!=null)socket.writeBytes(pixels);socket.flush();
        }
        private function clear(gs:Object):void {
            var old:Array=gs.team.units.concat();
            for each(var u:Object in old) {
                if(u.type==1)u.ai.targetOre=null;
                gs.team.removeUnitCompletely(u,gs.game);
            }
            gs.team.population=0;
        }
        private function spawn(gs:Object,type:int,x:Number,y:Number):Object {
            var u:Object=gs.game.unitFactory.getUnit(type);
            gs.team.spawn(u,gs.game);gs.team.population+=u.population;
            u.px=x;u.py=y;return u;
        }
        private function move(gs:Object,u:Object,ore:Object):void {
            var C:Class=Class(getDefinitionByName("com.brockw.stickwar.engine.multiplayer.moves::UnitMove"));
            var command:Object=new C();command.moveType=3;command.owner=gs.team.id;
            command.units.push(u.id);command.arg0=ore.x;command.arg1=ore.y;command.arg4=ore.id;
            command.execute(gs.game);
        }
        private function row(u:Object):Object {
            var resource:int=-2;
            if(u.type==1 && u.ai.targetOre!=null)resource=u.ai.targetOre.type==1000?-1:0;
            return {id:u.id,type:u.type,x:u.px,y:u.py,goalX:u.ai.currentCommand.goalX,
                goalY:u.ai.currentCommand.goalY,command:u.ai.currentCommand.type,
                facing:u.getDirection(),pose:u.mc.currentLabel,resource:resource};
        }
        private function sample(e:Event):void {
            try {
                if(!DisplayObjectContainer(root).numChildren)return;
                var app:Object=DisplayObjectContainer(root).getChildAt(0);
                if(!("currentScreen" in app))return;
                var request:int=socket.bytesAvailable?socket.readUnsignedByte():0;
                if(request==69) {
                    app.campaign.currentLevel=1;app.campaign.campaignPoints=1;app.campaign.difficultyLevel=1;
                    app.showScreen("campaignGameScreen");return;
                }
                var state:Object={page:app.currentScreen(),frame:0};
                if(state.page=="campaignGameScreen") {
                    var gs:Object=app.getScreen(state.page);
                    gs.doAiUpdates=false;
                    if(request==70) {
                        clear(gs);formation=[];
                        var types:Array=[4,7,8,1,6,2,3,7,8,4];
                        for(var n:int=0;n<types.length;n++)spawn(gs,types[n],n==1?800:n%2==0?1900:1500,125);
                        gs.team.defend(true);
                        for each(var unit:Object in gs.team.units)formation.push(row(unit));
                        gs.game.screenX=gs.game.targetScreenX=1000;
                    }
                    if(request==67) {
                        gs.game.fogOfWar.isFogOn=false;
                        if(!gs.team.enemyTeam.castleDefence.units.length)gs.team.enemyTeam.castleDefence.addUnit();
                        gs.game.screenX=gs.game.targetScreenX=gs.team.enemyTeam.homeX-600;
                    }
                    if(request==71) {
                        clear(gs);workers=[];
                        target=gs.game.map.gold[0];
                        for(n=0;n<3;n++)workers.push(spawn(gs,1,n<2?target.x+(2*n-1)*50:1300,n<2?target.y:125));
                        for(n=0;n<2;n++) {target.reserveMiningSpot(workers[n]);workers[n].ai.targetOre=target;workers[n].ai.isUnassigned=false;}
                        gs.team.statue.reserveMiningSpot(workers[2]);workers[2].ai.targetOre=gs.team.statue;workers[2].ai.isUnassigned=false;
                        workers[2].py=gs.team.statue.py+gs.team.statue.getMiningSpot(workers[2]);
                        gs.game.screenX=gs.game.targetScreenX=850;
                    }
                    if(request==72)move(gs,workers[2],target);
                    if(request==73) {
                        var mine:Object=null;
                        for each(var candidate:Object in gs.game.map.gold) {
                            var fullScale:Number=.4*(gs.game.backScale+candidate.py/gs.game.map.height*(gs.game.frontScale-gs.game.backScale));
                            if(Math.abs(candidate.ore.scaleX-fullScale)<.000001) {mine=candidate;break;}
                        }
                        if(mine==null)throw new Error("No untouched prepared ore");
                        var total:int=int(gs.game.xml.xml.goldAtStartOfMap);
                        var first:Number=mine.mine(total-1,workers[0]);
                        var last:Number=mine.mine(35,workers[0]);
                        var after:Number=mine.mine(35,workers[0]);
                        depletion={total:total,first:first,last:last,after:after,scale:mine.ore.scaleX,full:mine.isMineFull()};
                    }
                    state.frame=gs.game.frame;state.camera=gs.game.screenX;state.formation=formation;state.depletion=depletion;
                    state.units=[];state.castle=[];
                    for each(unit in gs.team.units)state.units.push(row(unit));
                    for each(var defender:Object in gs.team.enemyTeam.castleDefence.units) {
                        state.castle.push({type:defender.type,redOffset:defender.mc.transform.colorTransform.redOffset,
                            greenOffset:defender.mc.transform.colorTransform.greenOffset,blueOffset:defender.mc.transform.colorTransform.blueOffset});
                    }
                }
                var pixels:ByteArray=null;
                if(request==83) {
                    var bitmap:BitmapData=new BitmapData(widthPx,heightPx,false,0xffffff);
                    bitmap.draw(root,new Matrix(heightPx/700,0,0,heightPx/700,0,0));
                    pixels=bitmap.getPixels(bitmap.rect);bitmap.dispose();pixels.compress();
                }
                send(JSON.stringify(state),pixels);
            } catch(error:Error) {send("error|"+error.toString(),null);}
        }
    }
}
